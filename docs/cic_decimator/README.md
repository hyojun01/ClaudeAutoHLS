# cic_decimator — IP Documentation

## 1. Overview

A 4-stage CIC (Cascaded Integrator-Comb) decimation filter that reduces the sample rate by a factor of 16. The CIC filter is a multiplier-free structure consisting of N=4 cascaded integrator stages operating at the input sample rate, followed by a rate change (decimation by R=16), followed by N=4 cascaded comb stages operating at the reduced output rate.

Key features:
- Multiplier-free pipeline (0 DSP, 0 BRAM) — all computation via addition/subtraction with register storage
- Free-running streaming IP (`ap_ctrl_none`) — accepts input samples continuously
- AXI-Stream 16-bit signed I/O with TLAST propagation through the decimation boundary
- Modular two's complement arithmetic — integrators wrap without saturation, comb stages cancel the overflow
- Output bit pruning with rounding from 32-bit full precision to 16-bit

## 2. Specifications

| Parameter         | Value                |
|-------------------|----------------------|
| HLS Tool Version  | Vitis HLS 2025.2     |
| Target FPGA Part  | xc7z020clg400-1      |
| Clock Period       | 10 ns (100 MHz)      |
| Top Function       | cic_decimator        |
| Language           | C++ (C++14)          |

## 3. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| din | IN | ap_axiu<16,0,0,0> | 16 | AXI-Stream (axis) | Input data stream, 16-bit signed samples at the high input rate |
| dout | OUT | ap_axiu<16,0,0,0> | 16 | AXI-Stream (axis) | Decimated output stream, 16-bit signed samples at 1/R of the input rate |
| (return) | — | — | — | ap_ctrl_none | No block-level handshake (free-running) |

### AXI-Stream Signal Details

| Signal | din (input) | dout (output) |
|--------|-------------|---------------|
| TDATA[15:0] | 16-bit signed input sample | 16-bit signed decimated output |
| TKEEP[1:0] | 0x3 (both bytes valid) | 0x3 (both bytes valid) |
| TSTRB[1:0] | 0x3 | 0x3 |
| TLAST | Frame boundary marker | OR of R input TLASTs in the decimation block |
| TVALID | Driven by upstream | Asserted every R=16 input samples |
| TREADY | Backpressure to upstream | Driven by downstream |

### Clock and Reset

| Signal | Description |
|--------|-------------|
| ap_clk | System clock (100 MHz) |
| ap_rst_n | Synchronous active-low reset |

## 4. Design Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| CIC_N | 4 | Number of integrator/comb stages |
| CIC_R | 16 | Decimation ratio |
| CIC_M | 1 | Differential delay per comb stage |
| DIN_W | 16 | Input sample width (bits) |
| DOUT_W | 16 | Output sample width (bits) |
| ACC_W | 32 | Internal accumulator width (bits) |
| PRUNE_BITS | 16 | Bits discarded during output pruning |

### Data Types

| Type | Definition | Description |
|------|-----------|-------------|
| din_t | ap_int<16> | 16-bit signed input sample |
| dout_t | ap_int<16> | 16-bit signed output sample |
| acc_t | ap_int<32> | 32-bit signed modular accumulator |
| axis_din_t | ap_axiu<16,0,0,0> | AXI-Stream input packet |
| axis_dout_t | ap_axiu<16,0,0,0> | AXI-Stream output packet |

## 5. Functional Description

### Architecture

```
din (16-bit) --> [Integrator 0] --> [Integrator 1] --> [Integrator 2] --> [Integrator 3]
                    (rate Fs)                                                  |
                                                                       [Decimate by R=16]
                                                                               |
                 [Comb 3] <-- [Comb 2] <-- [Comb 1] <-- [Comb 0] <-----------+
                    (rate Fs/16)                              |
                                                       [Bit Pruning + Round]
                                                              |
                                                         dout (16-bit)
```

### Processing Stages

**Stage 1 — Integrators (all run at input rate Fs):**
Each integrator is a running accumulator: `integrator[k] += input_to_stage_k`. All 4 integrators execute on every input sample (II=1). Arithmetic is modular (two's complement wraparound), which is mathematically correct for CIC filters.

**Stage 2 — Decimation:**
A 4-bit counter increments on each input sample. When the counter reaches 15 (every 16th sample), the integrator output is passed to the comb stages and the counter resets.

**Stage 3 — Comb stages (run at output rate Fs/R):**
Each comb computes the difference between the current value and a 1-sample delayed copy: `comb_out = comb_in - comb_delay[k]`. The delay register is then updated. Only fires on the decimation boundary.

**Stage 4 — Bit pruning with rounding:**
The 32-bit comb output is rounded to 16 bits by adding `1 << 15` (0.5 LSB) before arithmetic right-shifting by 16 bits.

### TLAST Propagation

TLAST is accumulated (logical OR) across all R=16 input samples in each decimation block. If any input sample in the block has TLAST=1, the corresponding output sample has TLAST=1. The filter's internal state persists across TLAST boundaries — TLAST is purely a sideband signal.

### CIC Frequency Response

```
|H(f)| = |sin(pi * R * M * f / Fs) / sin(pi * f / Fs)|^N
```

For N=4, R=16, M=1:
- DC gain: R^N = 16^4 = 65536 (normalized to 1.0 by bit pruning)
- First null: f = Fs / R = Fs / 16
- Alias rejection: ~48 dB at the first alias frequency
- Passband droop: sinc^4 rolloff (typically compensated by a downstream FIR)

## 6. Performance

| Metric | Value |
|--------|-------|
| Initiation Interval (II) | 1 cycle |
| Pipeline Depth | 5 cycles |
| Input Throughput | 100 MSamples/s (1 sample/clock) |
| Output Throughput | 6.25 MSamples/s (1 output per 16 inputs) |
| Estimated Clock Period | 6.923 ns |
| Maximum Frequency (Fmax) | 144.45 MHz |
| Timing Slack | 3.077 ns |

## 7. Resource Utilization

| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|-----------------|
| BRAM_18K | 0 | 280 | 0.0 |
| DSP | 0 | 220 | 0.0 |
| FF | 393 | 106400 | 0.4 |
| LUT | 485 | 53200 | 0.9 |

Resource breakdown:
- **FF (393)**: 8 x 32-bit state registers (integrators + comb delays) = 256 FF, plus pipeline registers, decimation counter, and TLAST accumulator
- **LUT (485)**: 8 x 32-bit adders/subtractors, decimation counter/comparator, bit pruning with rounding, control muxing, and AXI-Stream interface logic
- **DSP (0)**: Entirely multiplier-free — all operations are addition/subtraction
- **BRAM (0)**: All state stored in registers (8 x 32-bit values fully partitioned)

## 8. Verification

| Test | Description | Result |
|------|-------------|--------|
| Impulse response | Unit impulse, 64 samples, compare vs reference | PASS |
| DC max positive | 256 samples of 32767, check steady-state = 32767 | PASS |
| DC max negative | 256 samples of -32768, check steady-state = -32768 | PASS |
| Sinusoidal in-band | 1024 samples, f=Fs/256, compare vs reference | PASS (0 LSB error) |
| TLAST propagation | TLAST on decimation boundaries | PASS |
| State continuity | 3 separate 16-sample batches, verify static state persistence | PASS |

Total: **131/131 checks passed**, 0 errors.

## 9. File Listing

```
src/cic_decimator/
├── instruction.md              # IP specification
├── src/
│   ├── cic_decimator.hpp       # Header: types, constants, prototype
│   └── cic_decimator.cpp       # Implementation
├── tb/
│   └── tb_cic_decimator.cpp    # Testbench (6 test cases, reference model)
├── tcl/
│   ├── run_csim.tcl            # C-simulation script
│   ├── run_csynth.tcl          # C-synthesis script
│   └── run_cosim.tcl           # Co-simulation script
└── reports/
    └── csynth.xml              # Synthesis report
```
