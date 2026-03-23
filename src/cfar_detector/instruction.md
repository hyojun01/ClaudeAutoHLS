# IP Instruction: cfar_detector

## 1. Functional Description

Cell-Averaging CFAR (Constant False Alarm Rate) detector with runtime-selectable Greatest-Of and Smallest-Of modes. The IP processes a stream of power (magnitude-squared) values from radar range or range-Doppler processing and produces per-cell adaptive detection decisions.

For each cell under test (CUT), the detector:
1. Estimates the local noise floor from reference cells surrounding the CUT (excluding guard cells)
2. Computes an adaptive threshold by multiplying the noise estimate by a user-configurable scaling factor (alpha)
3. Compares the CUT power against the threshold
4. Outputs the CUT power value and a detection flag

Three CFAR modes are supported:
- **CA-CFAR (mode=0)**: Noise estimate = average of all leading + lagging reference cells. Best performance in homogeneous clutter.
- **GO-CFAR (mode=1)**: Noise estimate = max(leading average, lagging average). Suppresses false alarms at clutter edges (e.g., land-sea boundary).
- **SO-CFAR (mode=2)**: Noise estimate = min(leading average, lagging average). Better detection sensitivity at clutter edges, at the cost of higher false alarm rate at transitions.

The IP operates on fixed-length sweeps. The host configures sweep length, threshold factor, and mode via AXI-Lite, then triggers processing. The IP reads exactly `num_range_cells` power samples and produces exactly `num_range_cells` output results (1:1 mapping).

### Edge Handling
The first and last `REF_CELLS_PER_SIDE + GUARD_CELLS_PER_SIDE` (= 18) cells in each sweep do not have a full reference window. For these cells, the detection flag is forced to 0 regardless of the comparison result. The CUT power value is still passed through.

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| power_in | IN | `ap_axiu<32,0,0,0>` | 32 | AXI-Stream (axis) | Input power (magnitude-squared) samples, one per clock cycle. TLAST marks end of sweep. |
| detect_out | OUT | `ap_axiu<64,0,0,0>` | 64 | AXI-Stream (axis) | Output detection results. See output packing format below. TLAST on last result of sweep. |
| alpha | IN | `ap_uint<16>` | 16 | AXI-Lite (s_axilite), bundle=ctrl | Threshold scaling factor, interpreted as `ap_ufixed<16,8>` (8 integer bits, 8 fractional bits). Range [0, 256). Typical values: 3.0–15.0 (encoded as 0x0300–0x0F00). |
| cfar_mode | IN | `ap_uint<2>` | 2 | AXI-Lite (s_axilite), bundle=ctrl | CFAR mode: 0=CA, 1=GO, 2=SO. Values 3 is reserved (treat as CA). |
| num_range_cells | IN | `ap_uint<16>` | 16 | AXI-Lite (s_axilite), bundle=ctrl | Number of range cells per sweep. Minimum: 2×(REF_CELLS_PER_SIDE + GUARD_CELLS_PER_SIDE) + 1 = 37. Maximum: 65535. |
| return | — | — | — | AXI-Lite (s_axilite), bundle=ctrl | Block-level control (ap_ctrl_hs): start/done/idle. |

### Interface Protocol Rationale
- **AXI-Stream for data ports**: Power samples arrive as a continuous stream from upstream range processing at 1 sample/cycle. AXI-Stream provides backpressure via TREADY and frame boundary via TLAST, matching the streaming data flow pattern.
- **AXI-Lite for control**: Alpha, mode, and sweep length are set once per sweep (quasi-static configuration). A single AXI-Lite bundle keeps interface overhead to one instance (~230 LUT). Block-level `ap_ctrl_hs` allows the host to start processing and poll for completion.

### Output Packing Format

The 64-bit output word is packed as:

| Bits | Field | Description |
|------|-------|-------------|
| [31:0] | `cut_power` | CUT power value (pass-through from input) |
| [32] | `detect` | Detection flag: 1 = target detected, 0 = no detection |
| [63:33] | reserved | Set to 0 |

## 3. Data Types

```cpp
#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// --- Compile-time window geometry ---
const int REF_CELLS_PER_SIDE  = 16;  // Reference cells per side (must be power of 2)
const int GUARD_CELLS_PER_SIDE = 2;  // Guard cells per side
const int WINDOW_SIZE = 2 * REF_CELLS_PER_SIDE + 2 * GUARD_CELLS_PER_SIDE + 1;  // = 37

// Derived constants
const int CUT_INDEX = REF_CELLS_PER_SIDE + GUARD_CELLS_PER_SIDE;  // = 18 (lagging side + guards)
const int EDGE_CELLS = REF_CELLS_PER_SIDE + GUARD_CELLS_PER_SIDE; // = 18

// Log2 of ref cells for shift-division (REF_CELLS_PER_SIDE must be power of 2)
const int LOG2_REF_PER_SIDE = 4;   // log2(16) = 4
const int LOG2_TOTAL_REF    = 5;   // log2(32) = 5

// --- Data types ---
typedef ap_uint<32> power_t;       // Input power (magnitude-squared), unsigned 32-bit
typedef ap_uint<48> sum_t;         // Accumulator for sum of reference cells
                                   // 32 + ceil(log2(32)) = 37 bits; 48 provides margin
typedef ap_ufixed<16, 8> alpha_t;  // Threshold scaling factor: range [0, 256), 8 frac bits
typedef ap_uint<64> threshold_t;   // Intermediate threshold product (48 × 16 = 64 bits max)
typedef ap_uint<64> detect_out_t;  // Output word: {reserved[63:33], detect[32], power[31:0]}

// CFAR mode encoding
const ap_uint<2> MODE_CA = 0;
const ap_uint<2> MODE_GO = 1;
const ap_uint<2> MODE_SO = 2;
```

### Type Sizing Rationale

- **power_t (ap_uint<32>)**: Accommodates magnitude-squared of 16-bit complex I/Q: max(I² + Q²) = 2 × (2^15)² = 2^31, fits in 32 unsigned bits. Also covers 14-bit ADC inputs with headroom.
- **sum_t (ap_uint<48>)**: Sum of up to 32 power values. Worst case: 32 × (2^32 − 1) requires 37 bits. 48 bits provides 11 bits of margin.
- **alpha_t (ap_ufixed<16,8>)**: 8 integer bits support scaling factors up to 255. 8 fractional bits provide resolution of ~0.004, sufficient for fine-tuning false alarm rate. The AXI-Lite register stores the raw 16-bit encoding; the IP reinterprets it as fixed-point internally.
- **threshold_t (ap_uint<64>)**: Product of 48-bit sum × 16-bit alpha before right-shift normalization. 64 bits covers the full product without truncation.

### Boundary Value Notes
- `alpha_t` is unsigned fixed-point with no ±1.0 boundary risk.
- `power_t` is unsigned integer — no sign overflow issues.
- `sum_t` overflow: 48 bits with max 37-bit values — 11 bits of margin, no overflow possible.

## 4. Algorithm / Processing

### Sliding Window Structure

The detector maintains a shift register of `WINDOW_SIZE` (37) power samples:

```
Index:    [0 .. R-1]    [R .. R+G-1]     [R+G]    [R+G+1 .. R+2G]    [R+2G+1 .. W-1]
Content:  lagging ref   lagging guard     CUT      leading guard       leading ref
          (16 cells)    (2 cells)         (1)      (2 cells)           (16 cells)

Where: R = REF_CELLS_PER_SIDE = 16, G = GUARD_CELLS_PER_SIDE = 2, W = WINDOW_SIZE = 37
New samples enter at index 0; oldest sample exits at index W-1.
```

- **Lagging reference cells** [0..R-1]: The R most recent samples (arrived after the CUT).
- **Lagging guard cells** [R..R+G-1]: Buffer zone preventing target leakage into lagging ref.
- **Cell Under Test (CUT)** [R+G]: The sample being evaluated for detection.
- **Leading guard cells** [R+G+1..R+2G]: Buffer zone preventing target leakage into leading ref.
- **Leading reference cells** [R+2G+1..W-1]: The R oldest samples (arrived before the CUT).

### Running Sum Maintenance

Instead of summing all reference cells each cycle, maintain two running sums (`sum_lagging`, `sum_leading`) and update them incrementally as each new sample enters the window:

```
When new sample x_new enters at index 0 (all elements shift right by 1):

  sum_lagging += x_new − window[R−1]
    (gained: x_new entering lagging ref; lost: element leaving lagging ref into guard zone)

  sum_leading += window[R+2G] − window[W−1]
    (gained: element entering leading ref from guard zone; lost: element exiting the window)

Note: window[] values are read BEFORE the shift is applied.
```

This reduces the per-sample computation to 2 additions + 2 subtractions regardless of window size.

### Threshold Computation

Based on the selected CFAR mode:

```
Mode CA (Cell Averaging):
  noise_sum = sum_leading + sum_lagging
  threshold = (noise_sum × alpha) >> (LOG2_TOTAL_REF + ALPHA_FRAC_BITS)
            = (noise_sum × alpha) >> (5 + 8) = >> 13

Mode GO (Greatest Of):
  selected_sum = max(sum_leading, sum_lagging)
  threshold = (selected_sum × alpha) >> (LOG2_REF_PER_SIDE + ALPHA_FRAC_BITS)
            = (selected_sum × alpha) >> (4 + 8) = >> 12

Mode SO (Smallest Of):
  selected_sum = min(sum_leading, sum_lagging)
  threshold = (selected_sum × alpha) >> (LOG2_REF_PER_SIDE + ALPHA_FRAC_BITS)
            = (selected_sum × alpha) >> (4 + 8) = >> 12
```

Where `ALPHA_FRAC_BITS = 8` (fractional bits in the `ap_ufixed<16,8>` alpha type).

### Detection Decision

```
cut_power = window[CUT_INDEX]

if (sample_index >= EDGE_CELLS) AND (sample_index < num_range_cells − EDGE_CELLS):
    detect = (cut_power > threshold) ? 1 : 0
else:
    detect = 0    // Edge cells: incomplete window, suppress detection

output = {31'b0, detect, cut_power}
```

### Processing Flow (per sweep)

1. Host writes `alpha`, `cfar_mode`, `num_range_cells` to AXI-Lite registers
2. Host asserts start (ap_ctrl_hs)
3. IP initializes: clears shift register and running sums, resets sample counter
4. Main loop (num_range_cells iterations, pipelined II=1):
   a. Read one power sample from `power_in` stream
   b. Save boundary values from shift register (for sum update)
   c. Shift register right by 1, insert new sample at index 0
   d. Update `sum_lagging` and `sum_leading`
   e. Read CUT power from shift register center
   f. Compute threshold based on `cfar_mode`
   g. Determine detection (with edge suppression)
   h. Pack and write output to `detect_out` stream
   i. Assert TLAST on the last output sample
5. IP signals done (ap_ctrl_hs)

### Implementation Notes
- The shift register should be fully partitioned into registers (`ARRAY_PARTITION complete`) for single-cycle parallel access. At 37 × 32 bits = 1,184 FFs, this is well within register budget.
- The threshold multiply (48 × 16 bits) will map to 2 DSP48E1 slices on 7-series. The multiply can be pipelined (BIND_OP latency) if it creates a timing bottleneck, since the result is only needed for comparison, not for a loop-carried dependency.
- The running sum update (2 add + 2 sub, 48-bit) has no loop-carried dependency because `sum_lagging` and `sum_leading` are updated with values from the CURRENT shift register state, not from the previous sum computation.
- Actually, `sum_lagging` and `sum_leading` ARE loop-carried (each iteration updates them based on their previous value), but the dependency is a simple add/subtract that completes in one cycle — it does not prevent II=1.

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target Latency | WINDOW_SIZE/2 = 18 cycles (initial pipeline fill) |
| Target II | 1 (one sample per clock cycle) |
| Target Throughput | 100 Msamples/sec @ 100 MHz |

### Expected Resource Usage

| Resource | Core Estimate | Interface Overhead | Total Estimate |
|----------|---------------|--------------------|----------------|
| BRAM_18K | 0 | 0 | 0 |
| DSP | 2 (threshold multiply 48×16) | 0 | 2 |
| FF | ~1,500 (shift reg 1,184 + accumulators 96 + pipeline/control ~220) | ~220 (AXI-Lite 160 + 2×AXI-Stream 60) | ~1,720 |
| LUT | ~400 (sum update 128 + mode/compare 88 + threshold shift 16 + output pack 32 + control 136) | ~250 (AXI-Lite 230 + 2×AXI-Stream 20) | ~650 |

Device utilization on xc7z020: DSP 0.9%, BRAM 0%, LUT 1.2%, FF 1.6% — well within budget.

## 6. Test Scenarios

### Test 1: Uniform Noise, No Targets
- **Input**: 256 range cells, all set to power = 1000
- **Config**: alpha = 4.0 (encoded 0x0400), mode = CA
- **Expected**: All detection flags = 0 (noise average ≈ 1000, threshold = 4000, no cell exceeds threshold)
- **Verifies**: Basic noise estimation and threshold computation; no false alarms in homogeneous environment

### Test 2: Single Strong Target
- **Input**: 256 range cells at power = 1000; cell 128 set to power = 50000 (17 dB above noise)
- **Config**: alpha = 4.0, mode = CA
- **Expected**: Detection flag = 1 at cell 128; all other flags = 0
- **Verifies**: Target detection with sufficient SNR; guard cells prevent target self-masking

### Test 3: Target Near Sweep Edge
- **Input**: 256 range cells at power = 1000; cell 10 set to power = 50000 (within edge region, index < 18)
- **Config**: alpha = 4.0, mode = CA
- **Expected**: Detection flag = 0 at cell 10 (edge suppression), despite power exceeding threshold
- **Verifies**: Edge handling — no false detections in incomplete-window region

### Test 4: Clutter Edge (Step Change)
- **Input**: 512 range cells; cells [0..255] at power = 1000, cells [256..511] at power = 10000 (10× step)
- **Config**: alpha = 4.0, mode = CA vs. GO
- **Expected CA**: Possible false detections near cell 256 (mixed reference window biases threshold)
- **Expected GO**: No false detections at the transition (GO selects the higher reference average)
- **Verifies**: GO-CFAR advantage at clutter boundaries

### Test 5: Two Closely Spaced Targets
- **Input**: 256 range cells at power = 1000; cells 128 and 132 (spacing = 4 = guard region width) set to power = 50000
- **Config**: alpha = 4.0, mode = CA
- **Expected**: Both cells detected (targets are separated by exactly the guard region, so neither contaminates the other's reference window)
- **Verifies**: Guard cell effectiveness for closely spaced targets

### Test 6: Alpha Sweep (Threshold Sensitivity)
- **Input**: 256 range cells at power = 1000; cell 128 at power = 5000 (7 dB above noise)
- **Config**: mode = CA; alpha swept through {2.0, 4.0, 6.0, 8.0}
- **Expected**: Detection at alpha=2.0 and alpha=4.0; no detection at alpha=6.0 and alpha=8.0
- **Verifies**: Threshold scaling factor correctly controls detection sensitivity

### Test 7: Stress Test (Long Sweep)
- **Input**: 4096 range cells at power = 1000; targets at cells {100, 500, 1000, 2000, 3500, 4000} with power = 30000
- **Config**: alpha = 4.0, mode = CA
- **Expected**: Exactly 6 detections at the specified positions; no false alarms
- **Verifies**: Correct operation over long sweeps; running sum stability (no drift/accumulator errors)

### Test 8: All Three Modes Comparison
- **Input**: 512 range cells at power = 1000; cell 300 at power = 20000; clutter step at cell 256 (power jumps to 5000)
- **Config**: alpha = 4.0; run three times with mode = CA, GO, SO
- **Expected**: Compare detection behavior near the clutter edge and at the target. Document differences in false alarm and detection performance across modes.
- **Verifies**: Functional correctness of all three CFAR modes

## 7. Additional Notes

- **REF_CELLS_PER_SIDE must be a power of 2** (4, 8, 16, 32). This constraint enables bit-shift division instead of actual division, eliminating a divider from the critical path. The default value of 16 (32 total reference cells) is suitable for medium-resolution radar with range cell widths of 15–150 m.
- **Guard cell sizing**: GUARD_CELLS_PER_SIDE = 2 is typical for point targets. For extended targets (occupying multiple range cells), increase guard cells to at least the expected target extent.
- **Alpha selection**: The probability of false alarm (Pfa) for CA-CFAR in Gaussian noise is: `Pfa = (1 + alpha/N)^(−N)`, where N = total reference cells (32). For Pfa = 1e-6 with N=32: alpha ≈ 4.5 (encoded as 0x0480).
- **Sweep-to-sweep operation**: The IP clears all internal state (shift register, running sums) at the start of each sweep. There is no state carried between sweeps. For continuous processing of multiple sweeps, the host re-triggers the IP after each completion.
- **Upstream data format**: The input must be unsigned power (magnitude-squared) values. If the upstream stage produces complex I/Q samples, a magnitude-squared computation block (I² + Q²) must precede the CFAR detector.
