# CFAR Detector — IP Documentation

## 1. Overview

Cell-Averaging CFAR (Constant False Alarm Rate) detector with runtime-selectable CA, GO (Greatest-Of), and SO (Smallest-Of) modes. Processes a stream of power (magnitude-squared) values from radar range or range-Doppler processing and produces per-cell adaptive detection decisions at one sample per clock cycle.

Key features:
- Three CFAR modes: CA-CFAR, GO-CFAR, SO-CFAR (runtime selectable)
- Sliding window with 16 reference cells per side, 2 guard cells per side
- Running sum architecture: constant per-sample computation regardless of window size
- Edge suppression for cells with incomplete reference windows
- Configurable sweep length (37–65535 range cells) and threshold scaling factor

## 2. Specifications

| Parameter         | Value                |
|-------------------|----------------------|
| HLS Tool Version  | Vitis HLS 2025.2     |
| Target FPGA Part  | xc7z020clg400-1      |
| Clock Period       | 10 ns (100 MHz)      |
| Top Function       | cfar_detector         |
| Language           | C++ (C++14)          |

## 3. I/O Ports

| Port Name | Direction | Data Type | Interface Protocol | Description |
|-----------|-----------|-----------|-------------------|-------------|
| power_in | IN | `ap_axiu<32,0,0,0>` | AXI-Stream (axis) | Input power samples, one per clock. TLAST marks end of sweep. |
| detect_out | OUT | `ap_axiu<64,0,0,0>` | AXI-Stream (axis) | Output detection results. Packed: {reserved[63:33], detect[32], cut_power[31:0]}. TLAST on last result. |
| alpha | IN | `ap_uint<16>` | AXI-Lite (s_axilite), bundle=ctrl | Threshold scaling factor, encoded as `ufixed<16,8>`. Typical values: 3.0–15.0. |
| cfar_mode | IN | `ap_uint<2>` | AXI-Lite (s_axilite), bundle=ctrl | CFAR mode: 0=CA, 1=GO, 2=SO, 3=reserved (treated as CA). |
| num_range_cells | IN | `ap_uint<16>` | AXI-Lite (s_axilite), bundle=ctrl | Range cells per sweep. Min: 37, Max: 65535. |
| return | — | — | AXI-Lite (s_axilite), bundle=ctrl | Block-level control (ap_ctrl_hs): start/done/idle. |

## 4. Functional Description

### Algorithm

For each cell under test (CUT), the detector:
1. Estimates the local noise floor from reference cells surrounding the CUT (excluding guard cells)
2. Computes an adaptive threshold: `threshold = noise_average × alpha`
3. Compares the CUT power against the threshold
4. Outputs the CUT power value and a detection flag

### Sliding Window Structure

```
Index:    [0..15]      [16..17]    [18]   [19..20]     [21..36]
Content:  lagging ref  lag guard   CUT    lead guard   leading ref
          (16 cells)   (2 cells)   (1)    (2 cells)    (16 cells)

New samples enter at index 0; oldest exits at index 36.
```

### Running Sum Maintenance

Two running sums (`sum_lagging`, `sum_leading`) are updated incrementally per sample:
- `sum_lagging += new_sample − window[15]` (element leaving lagging ref)
- `sum_leading += window[20] − window[36]` (element entering/exiting leading ref)

This reduces per-sample computation to 2 additions + 2 subtractions regardless of window size.

### Threshold Computation

| Mode | Noise Estimate | Shift |
|------|---------------|-------|
| CA (mode=0) | sum_leading + sum_lagging | >> 13 (÷32 ref cells × ÷256 alpha scale) |
| GO (mode=1) | max(sum_leading, sum_lagging) | >> 12 (÷16 ref cells × ÷256 alpha scale) |
| SO (mode=2) | min(sum_leading, sum_lagging) | >> 12 |

### Edge Handling

The first and last 18 cells (REF_CELLS_PER_SIDE + GUARD_CELLS_PER_SIDE) have incomplete reference windows. Detection flag is forced to 0 for these cells. CUT power is still passed through.

## 5. Architecture

### 5.1 Block Diagram (Textual)

```
AXI-Stream IN → [Shift Register (37 × 32-bit)] → [Running Sum Update]
                         ↓                              ↓
                    [CUT readout]               [Mode Select MUX]
                         ↓                              ↓
                   [Edge Check] ←──── [Threshold Multiply (48×16→64)] → [Right-Shift]
                         ↓                                                    ↓
                    [Compare: CUT > threshold] ←──────────────────────────────┘
                         ↓
                   [Output Packing] → AXI-Stream OUT
```

### 5.2 Data Flow

1. Host writes `alpha`, `cfar_mode`, `num_range_cells` via AXI-Lite
2. Host asserts start (ap_ctrl_hs)
3. IP clears shift register and running sums
4. **Fill phase** (18 cycles): reads samples into shift register, no output
5. **Steady state** (num_range_cells − 18 cycles): reads sample + outputs detection result each cycle
6. **Drain phase** (18 cycles): inserts zeros, outputs remaining detection results
7. IP signals done

Total loop iterations: `num_range_cells + 18`. Total outputs: `num_range_cells`.

## 6. Performance

### 6.1 Latency & Throughput

| Metric              | Value               |
|---------------------|---------------------|
| Pipeline II          | 1 cycle             |
| Pipeline Depth       | 10 stages           |
| Latency (min, 37 cells) | 68 cycles (0.68 µs) |
| Latency (typ, 256 cells) | ~274 cycles (~2.74 µs) |
| Latency (max, 65535 cells) | 65566 cycles (655.66 µs) |
| Throughput           | 100 Msamples/sec @ 100 MHz |
| Estimated Fmax       | 147.67 MHz          |

### 6.2 Resource Utilization

| Resource | Used  | Available | Utilization (%) |
|----------|-------|-----------|-----------------|
| BRAM_18K | 0     | 280       | 0.0             |
| DSP      | 3     | 220       | 1.4             |
| FF       | 2550  | 106400    | 2.4             |
| LUT      | 1729  | 53200     | 3.2             |
| URAM     | 0     | 0         | —               |

### 6.3 Timing

| Metric | Value |
|--------|-------|
| Target clock | 10.000 ns |
| Estimated clock | 6.772 ns |
| Clock uncertainty | 2.700 ns |
| Timing margin | 3.228 ns |

## 7. Interface Details

### 7.1 Control Interface

The `ctrl` AXI-Lite bundle provides:
- **Registers**: `alpha` (offset 0x10), `cfar_mode` (offset 0x18), `num_range_cells` (offset 0x20)
- **Block-level control**: ap_ctrl_hs protocol (start bit at offset 0x00)
- The host writes configuration registers, then sets the start bit. The IP processes one full sweep and signals done.

### 7.2 Data Interfaces

**power_in (AXI-Stream, 32-bit)**:
- One power sample per TVALID/TREADY handshake
- TLAST asserted on the last sample of each sweep
- Backpressure supported via TREADY

**detect_out (AXI-Stream, 64-bit)**:
- Output word packing: `{31'b0, detect[1], cut_power[32]}`
- TLAST asserted on the last output of each sweep
- One output per clock when pipeline is active

## 8. Usage Example

```cpp
#include "cfar_detector.hpp"

// In testbench or host driver:
hls::stream<ap_axiu<32,0,0,0>> power_in;
hls::stream<ap_axiu<64,0,0,0>> detect_out;

// Configure: alpha=4.0, CA mode, 256 range cells
ap_uint<16> alpha = 0x0400;  // 4.0 in ufixed<16,8>
ap_uint<2>  mode  = 0;       // CA-CFAR
ap_uint<16> cells = 256;

// Feed power samples (one per cycle)
for (int i = 0; i < 256; i++) {
    ap_axiu<32,0,0,0> w;
    w.data = power_data[i];
    w.keep = -1; w.strb = -1;
    w.last = (i == 255) ? 1 : 0;
    power_in.write(w);
}

// Run detector
cfar_detector(power_in, detect_out, alpha, mode, cells);

// Read results
for (int i = 0; i < 256; i++) {
    ap_axiu<64,0,0,0> r = detect_out.read();
    uint32_t cut_power = r.data & 0xFFFFFFFF;
    bool detected = (r.data >> 32) & 1;
}
```

## 9. Design Constraints & Limitations

- **REF_CELLS_PER_SIDE must be a power of 2** (enables bit-shift division instead of actual divider)
- **Minimum sweep length**: 37 range cells (= WINDOW_SIZE). Behavior is undefined for shorter sweeps.
- **No inter-sweep state**: shift register and running sums are cleared at the start of each sweep
- **Unsigned power only**: input must be magnitude-squared values. Complex I/Q requires upstream conversion.
- **Guard cell sizing**: GUARD_CELLS_PER_SIDE=2 is suitable for point targets. Extended targets may require larger guard regions (requires recompilation).

## 10. Optimization History

| Version | Date | Optimization Applied | Latency (cycles) | II | LUT | DSP | Notes |
|---------|------|---------------------|-------------------|----|-----|-----|-------|
| v1.0    | 2026-03-23 | Baseline (PIPELINE II=1, ARRAY_PARTITION complete) | 68–65566 | 1 | 1729 | 3 | Initial design, all targets met |
