# FIR Filter IP — Documentation

**IP Name:** `fir`
**Version:** 1.0
**Date:** 2026-03-21
**Status:** Design complete; C-simulation passed; C-synthesis passed (Vitis HLS 2025.1)

---

## 1. Overview

The `fir` IP implements a **45-tap symmetric low-pass FIR filter** in AMD Vitis HLS. It is designed for digital signal processing pipelines in FPGA fabric and is suitable for audio, baseband, and sensor data filtering applications where a sharp low-pass characteristic is required.

### Key Features

- 45-tap linear-phase FIR filter with symmetric coefficient exploitation
- AXI4-Stream input and output interfaces for seamless streaming integration
- AXI4-Lite slave control interface (`ap_ctrl_hs`) for software-controlled start/stop/done signaling
- 23 unique Q1.15 coefficients stored (22 symmetric pairs + 1 center tap); 22 pre-additions reduce the multiply count from 45 to 23
- Fully partitioned shift register and coefficient arrays enabling PIPELINE II=1 in the main processing loop
- TLAST propagation: input TLAST is forwarded verbatim to output; the IP terminates after consuming the TLAST-marked sample
- All arithmetic performed in `ap_fixed<45,15>` accumulator to prevent overflow across the full 45-tap accumulation

---

## 2. Specifications

| Parameter        | Value                          |
|------------------|-------------------------------|
| HLS Tool         | AMD Vitis HLS 2023.1+          |
| FPGA Part        | xc7z020clg400-1                |
| Target Clock     | 100 MHz (10 ns period)         |
| Top Function     | `fir`                          |
| Language         | C++14                          |
| Filter Type      | FIR, linear-phase, low-pass    |
| Number of Taps   | 45                             |
| Unique Coefficients | 23 (exploiting symmetry)    |
| Input Data Type  | `ap_fixed<16,1>` (Q1.15)       |
| Output Data Type | `ap_fixed<16,1>` (Q1.15)       |
| Coefficient Type | `ap_fixed<16,1>` (Q1.15)       |
| Accumulator Type | `ap_fixed<45,15>`              |

---

## 3. I/O Ports

| Port Name | Direction | Data Type          | Interface Protocol | Description                                      |
|-----------|-----------|--------------------|-------------------|--------------------------------------------------|
| `in`      | Input     | `hls::stream<axis_t>` | AXI4-Stream (`axis`) | Streaming input of 16-bit fixed-point samples with TLAST |
| `out`     | Output    | `hls::stream<axis_t>` | AXI4-Stream (`axis`) | Streaming output of filtered 16-bit samples with TLAST propagated |
| `return`  | —         | `ap_ctrl_hs`       | AXI4-Lite (`s_axilite`) | Hardware control: start, done, idle, ready signals via AXI-Lite register map |

The AXI-Stream word type `axis_t` is defined as `ap_axiu<16, 0, 0, 0>`, providing:
- `data[15:0]` — 16-bit sample payload
- `last[0]`    — TLAST (end-of-frame marker)
- `keep[1:0]`  — byte enable (always `0x3`)
- `strb[1:0]`  — byte strobe (always `0x3`)

---

## 4. Functional Description

### Algorithm

The IP implements a direct-form FIR convolution with symmetric coefficient optimization:

```
y[n] = sum_{k=0}^{44} h[k] * x[n-k]
```

where `h[k]` is the 45-tap impulse response. Because the filter is symmetric (`h[k] = h[44-k]`), the computation reduces to:

```
y[n] = h[22]*x[n-22]
     + sum_{k=0}^{21} h[k] * (x[n-k] + x[n-44+k])
```

This exploits 22 pre-additions before multiplication, reducing the multiplier count from 45 to 23.

### Data Flow

1. The IP reads one `axis_t` word per cycle from `in` (once pipelined).
2. The raw 16-bit bit pattern is reinterpreted as `ap_fixed<16,1>` via `.range()`.
3. The new sample is inserted at index 0 of a 45-element static shift register; all existing samples shift right by one position.
4. The symmetric MAC accumulation runs in `SYM_MAC_LOOP` (22 pre-add-multiply iterations) followed by a single center-tap multiply.
5. The `ap_fixed<45,15>` accumulator result is truncated to `ap_fixed<16,1>` output.
6. The output word is packed into `axis_t` with TLAST forwarded from the input sample.
7. The loop exits after the sample carrying TLAST=1 is processed and written.

### Coefficient Symmetry

Only 23 coefficients are stored in ROM (`static const coeff_t coeffs[23]`). The full 45-tap response is reconstructed implicitly during accumulation:
- Indices 0–21: paired with mirror indices 44–23 (pre-addition before multiply)
- Index 22: center tap, no symmetric pair (multiplied directly)

### TLAST Behavior

The `last` flag captured from the input AXI-Stream word controls the `while (!last)` loop:
- TLAST=0: processing continues with the next sample.
- TLAST=1: the filtered output for that sample is written with TLAST=1 asserted on the output stream, and the function returns.

The IP must be re-invoked (via `ap_ctrl_hs` start) for the next frame.

---

## 5. Architecture

### Block Diagram (Textual)

```
         AXI-Lite (s_axilite)
              |
         ap_ctrl_hs
              |
  ┌───────────▼────────────────────────────────┐
  │              fir (top function)             │
  │                                            │
  │  AXI-Stream ──► [AXIS Read]                │
  │  in               │                        │
  │                   ▼                        │
  │           [Shift Register]                 │
  │           shift_reg[0..44]                 │
  │           (fully partitioned)              │
  │                   │                        │
  │    ┌──────────────┤                        │
  │    │              │                        │
  │    ▼              ▼                        │
  │ [Pre-Add]    [Center Tap]                  │
  │  22 pairs      h[22]*x[22]                 │
  │    │              │                        │
  │    ▼              │                        │
  │ [Multiply]        │                        │
  │  h[k]*sum[k]      │                        │
  │    │              │                        │
  │    └──────┬───────┘                        │
  │           ▼                                │
  │       [Accumulate]                         │
  │       acc_t (ap_fixed<45,15>)              │
  │           │                                │
  │           ▼                                │
  │       [Truncate to data_t]                 │
  │           │                                │
  │           ▼                                │
  │  [AXIS Write + TLAST propagate]            │
  │           │                                │
  └───────────┼────────────────────────────────┘
              │
          AXI-Stream ──► out
```

### Data Flow Summary

| Stage              | Width               | Resource Implication           |
|--------------------|---------------------|-------------------------------|
| Input sample       | 16 bits (Q1.15)     | AXI-Stream register            |
| Shift register     | 45 × 16 bits        | Fully partitioned → registers  |
| Pre-addition       | 22 × 17 bits        | 22 adders                      |
| Multiplication     | 23 × (16×17) → 33b  | LUT fabric (0 DSP, BIND_OP)    |
| Accumulation       | 45 bits (Q15.30)    | Adder tree / partial sum chain |
| Output sample      | 16 bits (Q1.15)     | Truncation (rounding TBD)      |

---

## 6. Performance

### Latency and Throughput

| Metric               | Value                          |
|----------------------|-------------------------------|
| Clock Frequency      | 100 MHz (target), 105.46 MHz (estimated max) |
| Estimated Clock Period | 9.482 ns (target 10.00 ns, uncertainty 2.70 ns) |
| Initiation Interval  | 1 cycle (achieved)             |
| Pipeline Depth       | 7 cycles                       |
| Latency (cycles)     | N + 6 (for N input samples)    |
| Throughput           | 1 sample/cycle (100 MSps at 100 MHz) |
| Filter Group Delay   | 22 samples (half of 45-1)      |

### Resource Utilization

| Resource | Utilized | Available (xc7z020) | Utilization % |
|----------|----------|---------------------|---------------|
| DSP      | 0        | 220                 | 0%            |
| FF       | 2,288    | 106,400             | 2%            |
| LUT      | 8,694    | 53,200              | 16%           |
| BRAM_18K | 0        | 280                 | 0%            |

> **Note:** Zero DSP usage achieved via `#pragma HLS BIND_OP variable=acc op=mul impl=fabric`, which forces all 23 multiplications to LUT-based logic. Each 16×17 or 16×14 bit multiply is implemented as a combinational LUT network. Timing is tight (9.482 ns estimated vs 7.3 ns effective budget after uncertainty) but Fmax of 105.46 MHz still meets the 100 MHz target.

---

## 7. Interface Details

### Control Interface (AXI4-Lite, `s_axilite`)

The `return` port is bound to `s_axilite` generating an `ap_ctrl_hs` handshake block. Vitis HLS maps this to the following register addresses on the IP's AXI-Lite slave port:

| Offset | Register | Bit | Name    | Access | Description                          |
|--------|----------|-----|---------|--------|--------------------------------------|
| 0x00   | AP_CTRL  | 0   | ap_start | W     | Write 1 to start IP execution        |
| 0x00   | AP_CTRL  | 1   | ap_done  | R     | Reads 1 when IP has completed        |
| 0x00   | AP_CTRL  | 2   | ap_idle  | R     | Reads 1 when IP is idle (ready)      |
| 0x00   | AP_CTRL  | 3   | ap_ready | R     | Reads 1 when IP can accept new start |
| 0x04   | GIE      | 0   | gier     | R/W   | Global interrupt enable              |
| 0x08   | IER      | 0   | ap_done  | R/W   | Interrupt enable for ap_done         |
| 0x08   | IER      | 1   | ap_ready | R/W   | Interrupt enable for ap_ready        |
| 0x0C   | ISR      | 0   | ap_done  | R/W1C | Interrupt status for ap_done         |
| 0x0C   | ISR      | 1   | ap_ready | R/W1C | Interrupt status for ap_ready        |

### Data Interfaces (AXI4-Stream)

Both `in` and `out` are `hls::stream<ap_axiu<16,0,0,0>>` bound to `#pragma HLS INTERFACE axis`. This generates standard AXI4-Stream ports:

| Signal      | Direction | Width | Description                                |
|-------------|-----------|-------|--------------------------------------------|
| `in_TDATA`  | Input     | 16    | Input sample (Q1.15 bit pattern)           |
| `in_TKEEP`  | Input     | 2     | Byte enable (expected 0x3)                 |
| `in_TSTRB`  | Input     | 2     | Byte strobe (expected 0x3)                 |
| `in_TLAST`  | Input     | 1     | End-of-frame marker; 1 on final sample     |
| `in_TVALID` | Input     | 1     | AXI-Stream handshake: data valid           |
| `in_TREADY` | Output    | 1     | AXI-Stream handshake: IP ready to accept   |
| `out_TDATA` | Output    | 16    | Filtered output sample (Q1.15 bit pattern) |
| `out_TKEEP` | Output    | 2     | Byte enable (always 0x3)                   |
| `out_TSTRB` | Output    | 2     | Byte strobe (always 0x3)                   |
| `out_TLAST` | Output    | 1     | Forwarded from corresponding input TLAST   |
| `out_TVALID`| Output    | 1     | AXI-Stream handshake: output valid         |
| `out_TREADY`| Input     | 1     | AXI-Stream handshake: downstream ready     |

---

## 8. Usage Example

The following shows the minimal pattern for calling the `fir()` top function from a C++ testbench or co-simulation harness:

```cpp
#include "fir.hpp"

// Declare AXI-Stream channels
hls::stream<axis_t> in_stream;
hls::stream<axis_t> out_stream;

// Push N samples into the input stream
for (int n = 0; n < N; n++) {
    axis_t word;
    word.data  = 0;
    word.data.range() = input_sample[n].range(); // reinterpret ap_fixed bits
    word.keep  = -1;  // all bytes valid
    word.strb  = -1;
    word.last  = (n == N - 1) ? 1 : 0;           // assert TLAST on last sample
    in_stream.write(word);
}

// Invoke the IP (one frame = N samples terminated by TLAST)
fir(in_stream, out_stream);

// Read filtered samples from the output stream
for (int n = 0; n < N; n++) {
    axis_t out_word = out_stream.read();
    data_t result;
    result.range() = out_word.data.range();       // reinterpret output bits
    // process result ...
}
```

**Important notes:**
- The input stream must always contain a TLAST=1 word as the last element of each frame; without TLAST the IP will block indefinitely.
- The static shift register persists state across invocations. For a fresh filter state (e.g., between unrelated frames), the shift register must be reset by feeding 44 zero samples before the first real sample, or by resetting the IP via the AXI-Lite control interface.
- `keep` and `strb` should be set to `-1` (all-ones) for a 16-bit word occupying 2 bytes.

---

## 9. Design Constraints and Limitations

| Constraint / Limitation | Detail |
|------------------------|--------|
| Frame termination required | The IP loops on `while (!last)`; a frame without TLAST=1 will stall permanently |
| Shift register state persistence | `static data_t shift_reg[45]` retains samples across function calls; first 44 outputs after reset are affected by warmup zeros |
| Single-frame execution model | The IP processes exactly one frame per invocation (ap_ctrl_hs start → done); concurrent frames not supported |
| No runtime coefficient update | Coefficients are `static const`; changing filter response requires re-synthesis |
| Fixed-point arithmetic | No saturation or rounding mode specified on output truncation from `acc_t` to `data_t`; large input amplitudes may clip |
| Input amplitude recommendation | Keep sum of all tones within [-1.0, 1.0) in Q1.15 representation to avoid accumulator overflow; the passband gain is ≤ 1.0 |
| Target device | Verified targeting xc7z020clg400-1; porting to other devices requires re-running synthesis |

---

## 10. Optimization History

| Version | Date       | Change                                      | Latency (cycles) | DSP | FF  | LUT |
|---------|------------|---------------------------------------------|-----------------|-----|-----|-----|
| 1.0     | 2026-03-21 | Baseline: 45-tap symmetric FIR, PIPELINE II=1, fully partitioned arrays | 19 (depth) | 76 | 8,206 | 3,789 |
| 1.1     | 2026-03-21 | Remove acc_t casts on multiply operands → natural-width 16×17 DSP mapping | 9 (depth) | 23 | 1,807 | 352 |
| 1.2     | 2026-03-21 | BIND_OP impl=fabric → zero DSP, all multiplies in LUT logic | 7 (depth) | 0 | 2,288 | 8,694 |

---

## Source Files

| File | Description |
|------|-------------|
| `src/fir/src/fir.hpp` | Type definitions, constants, top function prototype |
| `src/fir/src/fir.cpp` | Top function implementation, coefficient ROM |
| `src/fir/tb/tb_fir.cpp` | Two-tone (5 MHz + 15 MHz) C-simulation testbench |
| `src/fir/tcl/run_csim.tcl` | Vitis HLS C-simulation TCL script |
| `src/fir/tcl/run_csynth.tcl` | Vitis HLS C-synthesis TCL script |
| `src/fir/tcl/run_cosim.tcl` | Vitis HLS co-simulation TCL script |
