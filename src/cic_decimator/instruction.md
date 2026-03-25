# IP Instruction: cic_decimator

## 1. Functional Description
A 4-stage CIC (Cascaded Integrator-Comb) decimation filter that reduces the
sample rate by a factor of 16. The CIC filter is a multiplier-free filter
consisting of N cascaded integrator stages operating at the input sample rate,
followed by a rate change (decimation), followed by N cascaded comb stages
operating at the reduced output rate.

The design uses compile-time constants for all parameters:
- **N = 4** stages (integrator-comb pairs)
- **R = 16** decimation ratio
- **M = 1** differential delay in each comb stage

The CIC decimation filter is the most efficient way to perform large-ratio sample
rate reduction on an FPGA because it requires zero multipliers. It is typically
used as the first decimation stage in a digital downconverter (DDC) chain,
followed by a compensation FIR filter to correct the CIC's passband droop.

### Key Properties
- Free-running streaming IP (`ap_ctrl_none`): accepts input samples continuously
  and produces one output sample every R=16 input samples
- All internal arithmetic uses two's complement modular (wraparound) addition.
  This is mathematically correct for CIC filters because the comb differentiators
  cancel any integrator wraparound. No saturation or overflow detection is needed
  on integrator stages.
- Output is bit-pruned from 32-bit full precision down to 16-bit by extracting
  the most significant 16 bits with rounding.

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| din | IN | ap_axiu<16,0,0,0> | 16 | AXI-Stream (axis) | Input data stream, 16-bit signed samples at the high input rate |
| dout | OUT | ap_axiu<16,0,0,0> | 16 | AXI-Stream (axis) | Decimated output stream, 16-bit signed samples at 1/R of the input rate |

### Block-Level Control
```
#pragma HLS INTERFACE ap_ctrl_none port=return
```
The IP is free-running: it starts processing as soon as valid data appears on
`din` and produces output on `dout` every R=16 input samples. There is no
software start/done handshake.

### TLAST Behavior
- The IP propagates TLAST from the input stream. If the input TLAST is asserted
  on any of the R input samples that correspond to one output sample, the output
  TLAST is asserted on that output sample.
- This allows frame boundary signaling to pass through the decimation filter.
- The filter's internal state (integrators, comb delays) persists across TLAST
  boundaries. TLAST is purely a sideband signal and does not reset the filter.

### Output Packing
```
TDATA[15:0] = decimated output sample (ap_int<16>, two's complement)
TLAST = propagated from input (OR of the R input TLAST values)
TKEEP = 0x3 (both bytes valid)
```

## 3. Data Types
```cpp
#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>

// Design parameters (compile-time constants)
const int CIC_N = 4;       // Number of integrator-comb stages
const int CIC_R = 16;      // Decimation ratio
const int CIC_M = 1;       // Differential delay

// Input/output data width
const int DIN_W = 16;      // Input sample width (signed)
const int DOUT_W = 16;     // Output sample width (signed)

// Internal accumulator width: DIN_W + N * ceil(log2(R * M))
// = 16 + 4 * ceil(log2(16 * 1)) = 16 + 4 * 4 = 32
const int ACC_W = 32;

// Data types
typedef ap_int<DIN_W>  din_t;      // 16-bit signed input sample
typedef ap_int<DOUT_W> dout_t;     // 16-bit signed output sample
typedef ap_int<ACC_W>  acc_t;      // 32-bit signed internal accumulator

// AXI-Stream types (top-level ports only)
typedef ap_axiu<DIN_W, 0, 0, 0>  axis_din_t;
typedef ap_axiu<DOUT_W, 0, 0, 0> axis_dout_t;

// Bit pruning: number of bits to discard from LSB side
// B_max = ACC_W = 32, output = DOUT_W = 16
// Discard the lower (ACC_W - DOUT_W) = 16 bits
const int PRUNE_BITS = ACC_W - DOUT_W;  // = 16
```

## 4. Algorithm / Processing

### CIC Decimation Filter Structure
```
x[n] -> [Integrator 1] -> [Integrator 2] -> [Integrator 3] -> [Integrator 4]
              (rate F_s)                                            |
                                                            [Downsample by R]
                                                                    |
         [Comb 4] <- [Comb 3] <- [Comb 2] <- [Comb 1] <-----------+
              (rate F_s/R)                         |
                                           [Bit Pruning]
                                                   |
                                               y[m] (output)
```

### Stage 1: Integrators (running at input rate F_s)
Each integrator is a simple accumulator with feedback:
```
integrator[k] = integrator[k] + input_to_stage_k

For k = 0: input_to_stage_0 = x[n]  (input sample, sign-extended to ACC_W)
For k > 0: input_to_stage_k = integrator[k-1]
```

All 4 integrator stages execute on EVERY input sample (II=1 at rate F_s). The
integrator state variables are `static` and persist across function calls.

CRITICAL: Integrator arithmetic MUST use natural two's complement wraparound
(the default `ap_int` behavior). Do NOT add saturation. The mathematical
correctness of CIC filters depends on the modular arithmetic property:
the comb difference stages cancel out any integrator overflow, yielding the
correct result.

### Stage 2: Decimation (rate change)
A sample counter increments on each input sample. When the counter reaches R-1
(i.e., every R-th sample), the current integrator output is passed to the comb
stages and an output is produced. The counter then resets to 0.

```
static ap_uint<ceil(log2(R))> count = 0;
bool decimate = (count == CIC_R - 1);
if (decimate) count = 0; else count++;
```

For R=16, `count` is `ap_uint<4>`.

### Stage 3: Comb Stages (running at output rate F_s/R)
Each comb stage computes the difference between the current value and a delayed
copy (delay = M = 1 output sample):

```
comb_out[k] = comb_in[k] - comb_delay[k]
comb_delay[k] = comb_in[k]  // update delay register for next output

For k = 0: comb_in[0] = decimated integrator output
For k > 0: comb_in[k] = comb_out[k-1]
```

The comb delay registers are `static` and persist across function calls. With
M=1, each comb stage needs exactly one delay register of ACC_W bits.

### Stage 4: Output Bit Pruning
The full-precision comb output is 32 bits. Extract the top 16 bits for the
output, with rounding:

```
// Rounding: add 1 at the MSB position of the discarded bits
acc_t rounded = comb_output + (acc_t(1) << (PRUNE_BITS - 1));

// Extract top DOUT_W bits
dout_t output = (dout_t)(rounded >> PRUNE_BITS);
```

The `>> PRUNE_BITS` operation is an arithmetic right shift (sign-preserving)
because `acc_t` is `ap_int<32>` (signed).

### Implementation Architecture
The recommended implementation is a single pipelined loop with II=1:

```cpp
void cic_decimator(
    hls::stream<axis_din_t>& din,
    hls::stream<axis_dout_t>& dout)
{
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS INTERFACE axis port=din
    #pragma HLS INTERFACE axis port=dout
    #pragma HLS PIPELINE II=1 style=flp

    // Persistent state
    static acc_t integrator[CIC_N] = {0, 0, 0, 0};
    static acc_t comb_delay[CIC_N] = {0, 0, 0, 0};
    static ap_uint<4> count = 0;
    static ap_uint<1> tlast_or = 0;

    // Read input
    axis_din_t in_word = din.read();
    acc_t sample = (acc_t)in_word.data;  // sign-extend 16->32 bits

    // Track TLAST across R input samples
    tlast_or |= in_word.last;

    // Integrator stages (all run every cycle)
    acc_t integ_chain = sample;
    INTEG_STAGES:
    for (int k = 0; k < CIC_N; k++) {
        integrator[k] = integrator[k] + integ_chain;
        integ_chain = integrator[k];
    }

    // Decimation + comb stages (run every R-th cycle)
    bool decimate = (count == (ap_uint<4>)(CIC_R - 1));

    if (decimate) {
        // Comb stages
        acc_t comb_chain = integ_chain;
        COMB_STAGES:
        for (int k = 0; k < CIC_N; k++) {
            acc_t comb_out = comb_chain - comb_delay[k];
            comb_delay[k] = comb_chain;
            comb_chain = comb_out;
        }

        // Bit pruning with rounding
        acc_t rounded = comb_chain + (acc_t(1) << (PRUNE_BITS - 1));
        dout_t out_sample = (dout_t)(rounded >> PRUNE_BITS);

        // Write output
        axis_dout_t out_word;
        out_word.data = out_sample;
        out_word.keep = 0x3;
        out_word.strb = 0x3;
        out_word.last = tlast_or;
        dout.write(out_word);

        tlast_or = 0;
        count = 0;
    } else {
        count++;
    }
}
```

NOTE: The integrator loop and comb loop should be automatically unrolled by the
HLS tool since CIC_N=4 is small. If not, add `#pragma HLS UNROLL` inside each
loop. The array declarations for `integrator` and `comb_delay` should use
`#pragma HLS ARRAY_PARTITION variable=integrator complete` and similarly for
`comb_delay` to ensure they are mapped to registers, not BRAM.

### CIC Frequency Response
The CIC decimation filter has the frequency response:
```
|H(f)| = |sin(pi * R * M * f / F_s) / sin(pi * f / F_s)|^N
```
For N=4, R=16, M=1, this gives approximately 48 dB of rejection at the first
alias frequency (F_s / R). The passband has a characteristic droop (sinc^N
rolloff) that is typically compensated by a downstream FIR filter.

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target II | 1 (one input sample per clock at 100 MHz) |
| Target Throughput | 100 MSamples/s input rate, 6.25 MSamples/s output rate |
| Output Latency | 1 clock cycle (combinational from input to integrators, output on the R-th sample) |
| Decimation Ratio | 16 (compile-time) |
| Number of Stages | 4 (compile-time) |

### Expected Resource Usage

| Resource | Core Estimate | Interface Overhead | Total Estimate |
|----------|---------------|--------------------|----------------|
| BRAM_18K | 0 | 0 | 0 |
| DSP | 0 | 0 | 0 |
| FF | ~300 (8 x 32-bit state regs + counter + pipeline) | ~60 (2 axis ports) | ~360 |
| LUT | ~400 (8 x 32-bit adders + decimation counter + bit pruning + muxing) | ~20 (2 axis ports) | ~420 |

Notes on resource estimation:
- Core datapath: 8 adders/subtractors at 32 bits = ~256 LUT. Decimation counter
  and comparator: ~20 LUT. Output rounding/shift: ~50 LUT. Control muxing: ~75 LUT.
- HLS infrastructure overhead: for a design this small (~400 LUT core), apply
  2.0-2.5x multiplier. Expected total: 800-1050 LUT.
- This is well within the xc7z020 budget (53,200 LUT, <2% utilization).
- Zero DSP and zero BRAM -- all storage is in registers (8 x 32-bit = 256 FF for
  state, plus pipeline registers).

## 6. Test Scenarios

### Test 1: Impulse Response
- **Input**: A single sample of value 1 (ap_int<16> = 1), followed by at least
  4*R = 64 zero samples.
- **Expected output**: The CIC impulse response. For N=4, R=16, M=1, the impulse
  response is a piecewise polynomial (cubic B-spline shape) with exactly 4
  non-zero output samples (output indices 0 through 3). The 4th-order CIC impulse
  response for R=16, M=1 is the 4-fold convolution of a rectangular pulse of
  length R=16 with itself, downsampled by R.
  - Output[0] = 1 (after bit pruning/rounding)
  - Output[1], [2], [3] should match the CIC impulse response coefficients.
- **Verification**: Compute the reference impulse response using a double-precision
  C++ model of the CIC (integrate N times, decimate, comb N times) and compare
  against the fixed-point output. The error should be at most +/-1 LSB of the
  16-bit output due to the rounding in bit pruning.
- **Purpose**: Verifies the core CIC algorithm (all integrator stages, decimation
  logic, all comb stages, and bit pruning) in a single deterministic test.

### Test 2: DC Input (Maximum Amplitude)
- **Input**: 256 samples all set to the maximum positive value (ap_int<16> = 32767).
- **Expected output**: After the filter settles (first ~4 output samples are
  transient), the output should converge to a constant value equal to the DC gain
  scaled input. The CIC DC gain is R^N = 16^4 = 65536. After bit pruning by
  shifting right 16 bits, the effective DC gain is 65536 / 65536 = 1.0, so the
  output should equal the input: 32767.
- **Verification**: After the initial transient (4 output samples), all subsequent
  output samples should equal 32767 (or 32766/32767 depending on rounding).
- **Purpose**: Verifies that modular arithmetic in integrators produces correct
  results even at full scale, and that the bit pruning correctly normalizes the
  DC gain to unity.

### Test 3: DC Input (Maximum Negative Amplitude)
- **Input**: 256 samples all set to -32768 (ap_int<16> minimum).
- **Expected output**: After the transient settles, output should converge to
  -32768 (the bit-pruned result of the full-precision accumulation).
- **Verification**: Same as Test 2 but for the negative rail. This exercises the
  two's complement modular arithmetic under heavy negative accumulation and
  verifies sign-correct bit pruning (arithmetic right shift).
- **Purpose**: Confirms correct behavior at the negative boundary. The integrators
  will wrap around extensively, but the comb stages must cancel the wraparound
  to produce the correct steady-state output.

### Test 4: Sinusoidal Input (In-Band)
- **Input**: 1024 samples of a sine wave at frequency f_in = F_s / 256 (well
  within the CIC passband, which extends to approximately F_s / (2*R) = F_s/32).
  Amplitude = 16384 (half scale). Generate as:
  `x[n] = (ap_int<16>)(16384.0 * sin(2.0 * M_PI * n / 256.0))`
- **Expected output**: 64 output samples (1024/16) of a decimated sine wave at
  the equivalent frequency f_out = F_s / (256 / R) = F_s_out * 16/256 = F_s_out/16.
  The sine amplitude should be approximately 16384 (CIC passband gain is near
  unity after bit pruning, with some sinc^4 rolloff).
- **Verification**: Compare each output sample against a double-precision reference
  model. Maximum absolute error should be less than 100 LSBs (16-bit) to account
  for the quantization effects of the CIC filter structure. Report the SNR of the
  output relative to the reference.
- **Purpose**: Verifies correct operation on a realistic in-band signal and
  measures the CIC filter's passband response.

### Test 5: TLAST Propagation
- **Input**: 32 input samples with TLAST=1 on sample 15 (marking a 16-sample
  frame boundary), then 16 more samples with TLAST=1 on sample 31.
- **Expected output**: First output sample (decimation of inputs 0-15) should
  have TLAST=1. Second output sample (inputs 16-31) should have TLAST=1.
- **Verification**: Check that TLAST is correctly OR-ed across the R input
  samples and appears on the corresponding output sample.
- **Purpose**: Verifies the TLAST sideband propagation through the decimation.

### Test 6: State Continuity Across Invocations
- **Input**: Call the function with 16 samples of value 1000, producing one output.
  Then call again with 16 samples of value 1000, producing another output.
  Then call a third time with 16 more samples of value 1000.
- **Expected output**: Each successive output should show the filter's transient
  response converging toward the steady-state value. The output values should be
  identical to processing all 48 samples in a single call (the static state
  persists between invocations in the HLS model).
- **Verification**: Process 48 samples in a single reference call, extract the 3
  output samples, and compare against the 3 outputs from 3 separate invocations.
  They must match exactly.
- **Purpose**: Verifies that `static` integrator and comb state correctly persists
  across function invocations (critical for the HLS free-running model).

## 7. Additional Notes

### Implementation Guidance
- The integrator and comb state arrays MUST be declared `static` inside the top
  function to persist across invocations (this is the HLS equivalent of hardware
  registers that retain state between clock cycles).
- Use `#pragma HLS ARRAY_PARTITION variable=integrator complete` and
  `#pragma HLS ARRAY_PARTITION variable=comb_delay complete` to map state to
  individual registers rather than BRAM. With only 4 elements of 32 bits each,
  registers are more efficient and enable parallel access needed for II=1.
- The top function should use `#pragma HLS PIPELINE II=1 style=flp` for a
  free-running pipelined design that continuously processes samples.
- The integrator loop (4 iterations) should be fully unrolled so all 4 additions
  execute in a single clock cycle. Similarly for the comb loop.
- The `style=flp` (flushable pipeline) is recommended for free-running streaming
  IPs to handle backpressure gracefully.

### Bit Pruning Details
The CIC filter has a gain of R^N = 16^4 = 65536 at DC. The bit growth formula
gives ACC_W = DIN_W + N*ceil(log2(R)) = 16 + 16 = 32 bits. The bit pruning step
extracts the top 16 bits from the 32-bit result, effectively dividing by 2^16 =
65536, which normalizes the DC gain to 1.0.

The rounding constant is `1 << (PRUNE_BITS - 1)` = `1 << 15` = 32768, added
before the right shift to implement round-half-up behavior.

### Modular Arithmetic Correctness
The CIC filter's mathematical correctness relies on the identity:
```
(a mod 2^B) - (b mod 2^B) = (a - b) mod 2^B
```
where B = ACC_W = 32. This means integrator wraparound is harmless as long as
the final comb output fits within the representable range. For a valid input
range (16-bit signed) and the given CIC parameters (N=4, R=16), the final
output after all N comb stages is guaranteed to fit within 32 bits.

IMPORTANT: Do NOT add saturation logic (AP_SAT) to the integrator accumulators.
Saturation would break the modular arithmetic property and produce incorrect
results. The `ap_int<32>` type uses AP_WRAP by default, which is correct.

### Testbench Implementation Notes
- The testbench should implement a double-precision reference CIC model:
  1. N cascaded integrators (running sum at input rate)
  2. Downsample by keeping every R-th sample
  3. N cascaded comb stages (difference with M-sample-ago value)
  4. Compare against the fixed-point IP output
- For the impulse response test, the reference model output can be computed
  analytically or via the double-precision model.
- The testbench must handle the output rate: for every R=16 input samples written
  to `din`, read exactly 1 sample from `dout`.
- Use `hls::stream` for the testbench I/O, writing all input samples then reading
  all output samples (or interleaving in blocks of R).

### Future Extensions
- Runtime-configurable decimation ratio via AXI-Lite (would require wider internal
  accumulators sized for the maximum R)
- Hogenauer pruning: reduce internal bit widths at each stage based on the
  per-stage noise analysis, saving registers and LUT
- Compensation FIR: a downstream IP that corrects the CIC passband droop
- Interpolation mode: reverse the CIC structure (comb first, then upsample, then
  integrate) for sample rate increase applications
