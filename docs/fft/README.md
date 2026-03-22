# FFT — 256-Point Radix-2 DIT FFT IP Documentation

## 1. Overview

A 256-point Radix-2 Decimation-in-Time (DIT) Fast Fourier Transform IP core implemented in C++ for AMD Vitis HLS. The IP accepts 256 complex samples via AXI-Stream, computes the FFT using 8 butterfly stages with precomputed twiddle factors stored in internal ROM, and outputs 256 complex frequency-domain samples via AXI-Stream. The design uses DATAFLOW optimization to overlap I/O and computation.

## 2. Specifications

| Parameter         | Value                    |
|-------------------|--------------------------|
| HLS Tool Version  | Vitis HLS 2025.1         |
| Target FPGA Part  | xc7z020-clg400-1 (Zynq-7000) |
| Clock Period       | 10 ns (100 MHz target)   |
| Top Function       | `fft`                    |
| Language           | C++ (C++14)              |
| FFT Size           | 256 points               |
| FFT Algorithm      | Radix-2 DIT              |

## 3. I/O Ports

| Port Name | Direction | Data Type           | Interface Protocol | Description |
|-----------|-----------|---------------------|-------------------|-------------|
| `in`      | IN        | `hls::stream<ap_axiu<32,0,0,0>>` | AXI4-Stream (axis) | Input data stream (256 packed complex samples) |
| `out`     | OUT       | `hls::stream<ap_axiu<32,0,0,0>>` | AXI4-Stream (axis) | Output data stream (256 packed complex samples) |
| `return`  | —         | void                | AXI4-Lite (s_axilite) | Block-level control (ap_ctrl_hs: start/done/idle) |

### Data Packing Format

Each 32-bit AXI-Stream data word contains:
- Bits [31:16]: Real part (`ap_fixed<16,1>`, range [-1, 1))
- Bits [15:0]: Imaginary part (`ap_fixed<16,1>`, range [-1, 1))

TLAST is asserted on the 256th (last) sample of each frame.

## 4. Functional Description

### Algorithm

The IP implements a standard 256-point Radix-2 DIT FFT:

1. **Input**: 256 complex samples are read from the AXI-Stream input
2. **Bit-Reversal**: Input samples are reordered using bit-reversed indices (8-bit reversal for 256-point FFT)
3. **Butterfly Stages**: 8 stages of butterfly computations, each with 128 butterfly operations using precomputed twiddle factors W_256^k = cos(2πk/256) - j·sin(2πk/256)
4. **Output**: 256 complex frequency-domain samples are written to the AXI-Stream output

### Data Types

| Type | Definition | Purpose |
|------|-----------|---------|
| `data_t` | `ap_fixed<16,1>` | Input/output real and imaginary components |
| `tw_t` | `ap_fixed<16,1>` | Twiddle factor coefficients |
| `acc_t` | `ap_fixed<45,15>` | Internal accumulator for butterfly computation (prevents overflow across 8 stages) |

### Twiddle Factors

128 complex twiddle factors are precomputed and stored as constant ROM arrays. Values are cos(2πk/256) and -sin(2πk/256) for k = 0 to 127.

## 5. Architecture

### 5.1 Block Diagram

```
┌─────────────────────────────────────────────────────────┐
│                      fft (top)                          │
│                                                         │
│  ┌────────────────┐  ┌──────────────┐  ┌────────────┐  │
│  │ read_and_      │  │ compute_fft  │  │ write_     │  │
│  │ reorder        │─▶│ (8 butterfly │─▶│ output     │  │
│  │ (bit-reverse)  │  │  stages)     │  │            │  │
│  └────────────────┘  └──────┬───────┘  └────────────┘  │
│         ▲                   │                  │        │
│         │            ┌──────┴───────┐          ▼        │
│   AXI-Stream in      │ Twiddle ROM  │   AXI-Stream out  │
│                      │ (128 complex │                   │
│                      │  values)     │                   │
│                      └──────────────┘                   │
│                                                         │
│  DATAFLOW: stages overlap via PIPO buffers              │
│  AXI-Lite: ap_ctrl_hs (start/done/idle/interrupt)       │
└─────────────────────────────────────────────────────────┘
```

### 5.2 Data Flow (v1.1 — Optimized)

1. `read_and_reorder` reads 256 packets from AXI-Stream, unpacks real/imag, and writes to separate real/imag output buffers in bit-reversed order (single merged loop)
2. `butterfly_stage<0>` through `butterfly_stage<7>` — 8 independent template-instantiated hardware modules, each performing 128 butterfly operations with pipelined DSP multiplies (BIND_OP latency=3), connected by PIPO buffers via DATAFLOW
3. `write_output` reads from separate real/imag buffers, truncates `acc_t` → `data_t`, repacks, and writes to AXI-Stream with TLAST on the last sample

## 6. Performance

### 6.1 Latency & Throughput

| Metric              | Value         |
|---------------------|---------------|
| Latency (cycles)     | 1,598         |
| Latency (time)       | 15.98 us @ 100 MHz |
| Initiation Interval  | 256 cycles    |
| Pipeline Type        | Dataflow (10-stage) |
| Estimated Fmax       | 138.85 MHz    |
| Estimated Clock      | 7.202 ns      |

### Component Latency Breakdown

| Component           | Latency (cycles) | Interval (cycles) |
|---------------------|-------------------|--------------------|
| read_and_reorder    | 258               | 256                |
| butterfly_stage<0-7>| 134 each          | 128 each           |
| write_output        | 259               | 256                |

### 6.2 Resource Utilization

| Resource | Used  | Available | Utilization (%) |
|----------|-------|-----------|-----------------|
| BRAM_18K | 15    | 280       | 5.4%            |
| DSP      | 30    | 220       | 13.6%           |
| FF       | 15,593| 106,400   | 14.7%           |
| LUT      | 14,209| 53,200    | 26.7%           |

## 7. Interface Details

### 7.1 Control Interface (AXI-Lite)

The `control` AXI-Lite interface provides block-level control with `ap_ctrl_hs` protocol:

| Register | Offset | Description |
|----------|--------|-------------|
| AP_CTRL  | 0x00   | Bit 0: ap_start, Bit 1: ap_done, Bit 2: ap_idle, Bit 3: ap_ready |
| GIE      | 0x04   | Global Interrupt Enable |
| IER      | 0x08   | IP Interrupt Enable Register |
| ISR      | 0x0C   | IP Interrupt Status Register |

### 7.2 Data Interfaces

**AXI-Stream Input (`in_r`)**:
- TDATA: 32 bits (packed complex: real[31:16], imag[15:0])
- TKEEP: 4 bits
- TSTRB: 4 bits
- TLAST: 1 bit (asserted on sample 256)
- TVALID/TREADY handshake

**AXI-Stream Output (`out_r`)**:
- Same structure as input
- TLAST asserted on the 256th output sample

## 8. Usage Example

```cpp
#include "fft.hpp"

// Create streams
hls::stream<axis_pkt> in_stream, out_stream;

// Pack and send 256 input samples
for (int i = 0; i < 256; i++) {
    axis_pkt pkt;
    ap_uint<32> packed;
    data_t real_val = /* your real sample */;
    data_t imag_val = /* your imag sample */;
    packed.range(31, 16) = real_val.range();
    packed.range(15, 0)  = imag_val.range();
    pkt.data = packed;
    pkt.keep = 0xF;
    pkt.strb = 0xF;
    pkt.last = (i == 255) ? 1 : 0;
    in_stream.write(pkt);
}

// Run FFT
fft(in_stream, out_stream);

// Read 256 output samples
for (int k = 0; k < 256; k++) {
    axis_pkt pkt = out_stream.read();
    data_t re, im;
    re.range() = pkt.data.range(31, 16);
    im.range() = pkt.data.range(15, 0);
    // re and im are the FFT output for bin k
}
```

## 9. Design Constraints & Limitations

- **Output range**: Output values are truncated from `ap_fixed<45,15>` to `ap_fixed<16,1>` (range [-1, 1)). Input amplitudes must be kept small enough that FFT output magnitudes stay within this range (approximately < 1/128 per tone for single-tone inputs).
- **Fixed FFT size**: The FFT size is fixed at 256 points. Variable-length transforms are not supported.
- **Timing**: Estimated clock 7.202 ns meets the 10 ns target (Fmax 138.85 MHz).
- **No scaling**: The FFT does not apply 1/N output scaling. Output magnitudes scale with N (256) relative to input amplitudes.

## 10. Optimization History

| Version | Date       | Optimization Applied | Latency (cycles) | II    | LUT   | DSP | Notes |
|---------|------------|---------------------|-------------------|-------|-------|-----|-------|
| v1.0    | 2026-03-22 | Baseline (no opt)    | 4,376             | 3,596 | 1,607 | 12  | Initial design, timing not met (23.2 ns) |
| v1.1    | 2026-03-22 | Split stages + DATAFLOW + BIND_OP | 1,598 | 256 | 5,517 | 80 | Timing met (7.2 ns), Fmax 138.85 MHz |
| v1.2    | 2026-03-22 | PIPO buffers → LUTRAM | 1,672 | 256 | 18,437 | 80 | BRAM 107→15 (-86%), LUT trades for BRAM |
| v1.3    | 2026-03-22 | acc_t narrowed to ap_fixed<25,10> | 1,662 | 256 | 14,209 | 30 | DSP 80→30 (-63%), single-DSP multiply |
