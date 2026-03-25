# mac_array -- 4x4 Output-Stationary MAC Array IP

## Overview

This IP implements a 4x4 output-stationary MAC (Multiply-Accumulate) array for INT8 deep learning inference. It contains 16 processing elements (PEs) arranged in a 4-row by 4-column grid, computing one tile of a matrix multiplication per invocation:

```
C_tile[4][4] = A_tile[4][K] x B_tile[K][4]
```

The IP operates in two phases per invocation: a compute phase (K cycles, 16 parallel MACs at II=1) and a drain phase (16 cycles, outputting results row-major). It is designed as a single-tile compute primitive for systolic-array-based matrix multiplication, where the host software tiles larger matrix problems into 4x4 blocks.

## Specifications

| Parameter | Value |
|-----------|-------|
| Tool Version | Vitis HLS 2025.2 |
| FPGA Part | xc7z020-clg400-1 (Zynq-7000) |
| Clock Period | 10 ns (100 MHz) |
| Top Function | `mac_array` |
| Flow Target | Vivado IP Flow |
| C++ Standard | C++14 |

## I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| act_in | IN | hls::stream\<ap_axiu\<32,0,0,0\>\> | 32 | AXI-Stream (axis) | Packed activations: 4 x INT8 per word. K words per invocation. Byte layout: [3]=row3, [2]=row2, [1]=row1, [0]=row0. TLAST on last word. |
| weight_in | IN | hls::stream\<ap_axiu\<32,0,0,0\>\> | 32 | AXI-Stream (axis) | Packed weights: 4 x INT8 per word. K words per invocation. Byte layout: [3]=col3, [2]=col2, [1]=col1, [0]=col0. TLAST on last word. |
| result_out | OUT | hls::stream\<ap_axiu\<32,0,0,0\>\> | 32 | AXI-Stream (axis) | One INT32 result per word. 16 words per invocation (row-major). TLAST on 16th word. |
| k_dim | IN | ap_uint\<16\> | 16 | AXI-Lite (s_axilite, bundle=ctrl) | Inner dimension K. Valid range: 1-4096. |
| return | -- | -- | -- | AXI-Lite (s_axilite, bundle=ctrl) | Block-level control: ap_ctrl_hs (start/done/idle). |

## Functional Description

### Data Flow

```
act_in (axis) ----> [Unpack 4xINT8] ---+
                                        |---> [16 Parallel MACs] ---> acc[4][4]
weight_in (axis) -> [Unpack 4xINT8] ---+         (fully unrolled)

acc[4][4] ---> [Row-Major Drain] ---> [Pack INT32 + TLAST] ---> result_out (axis)
```

### Operating Phases

1. **Compute Phase** (K cycles at II=1): Each cycle reads one packed activation word and one packed weight word from the input streams, unpacks 4 INT8 values from each, and performs 16 parallel multiply-accumulate operations across all PEs. PE[r][c] accumulates `act[r] * weight[c]`.

2. **Drain Phase** (16 cycles at II=1): The 16 accumulated INT32 results are written to the output stream in row-major order (row 0 cols 0-3, row 1 cols 0-3, ..., row 3 cols 0-3). TLAST is asserted on the 16th (final) word.

### Data Packing Convention

**Activation stream** (K words, one per k-step):
```
Word k: { A[3][k], A[2][k], A[1][k], A[0][k] }
         byte[3]   byte[2]   byte[1]   byte[0]
```
Each word contains one element from each of the 4 activation rows, at column index k.

**Weight stream** (K words, one per k-step):
```
Word k: { B[k][3], B[k][2], B[k][1], B[k][0] }
         byte[3]   byte[2]   byte[1]   byte[0]
```
Each word contains one complete row of the weight matrix at row index k.

**Result stream** (16 words, row-major):
```
Words  0-3:  C[0][0], C[0][1], C[0][2], C[0][3]
Words  4-7:  C[1][0], C[1][1], C[1][2], C[1][3]
Words  8-11: C[2][0], C[2][1], C[2][2], C[2][3]
Words 12-15: C[3][0], C[3][1], C[3][2], C[3][3]
```

## Architecture

```
                +---------------+      +---------------+
  act_in ------>| Unpack 4xINT8 |      | Unpack 4xINT8 |<------ weight_in
  (axis, 32b)  | a[0..3]       |      | w[0..3]       |        (axis, 32b)
                +-------+-------+      +-------+-------+
                        |                      |
                        v                      v
                +--------------------------------------+
                |       16 PEs (4x4 Grid)              |
                |                                      |
                |  PE[0][0]: acc += a[0]*w[0]  ...  PE[0][3]: acc += a[0]*w[3]  |
                |  PE[1][0]: acc += a[1]*w[0]  ...  PE[1][3]: acc += a[1]*w[3]  |
                |  PE[2][0]: acc += a[2]*w[0]  ...  PE[2][3]: acc += a[2]*w[3]  |
                |  PE[3][0]: acc += a[3]*w[0]  ...  PE[3][3]: acc += a[3]*w[3]  |
                |                                      |
                |  (All 16 MACs fully unrolled,        |
                |   one DSP each, pipelined at II=1)   |
                +------------------+-------------------+
                                   |
                                   v
                +------------------+-------------------+
                |  Drain: 16-cycle row-major readout   |
                |  acc[r][c] -> result_out with TLAST  |
                +------------------+-------------------+
                                   |
                                   v
                              result_out (axis, 32b)

  k_dim ------> [AXI-Lite ctrl] ------> Loop bound for COMPUTE_K
```

## Performance

### Latency

| K Value | Total Cycles | Latency (time) | Compute Throughput |
|---------|-------------|-----------------|-------------------|
| K=1 | 30 | 0.30 us | 16 MACs/cycle |
| K=4 | 33 | 0.33 us | 16 MACs/cycle |
| K=64 | 93 | 0.93 us | 16 MACs/cycle |
| K=256 | 285 | 2.85 us | 16 MACs/cycle |
| K=4096 | 4125 | 41.25 us | 16 MACs/cycle |

Total latency = K + 16 + 13 overhead cycles (pipeline fill/drain and module entry/exit).

### Pipeline Performance

| Parameter | Value |
|-----------|-------|
| Compute Loop II | 1 cycle |
| Drain Loop II | 1 cycle |
| Compute Pipeline Depth | 5 cycles (DSP latency=3 for muladd) |
| Drain Pipeline Depth | 3 cycles |
| MAC Throughput | 16 MACs/cycle (3.2 GOPS at 100 MHz) |
| Estimated Fmax | 159.15 MHz |
| Timing Slack | 3.717 ns (at 10 ns target) |

### Worst-Case Latency (K=4096)

| Metric | Value |
|--------|-------|
| Min Latency | 30 cycles (0.30 us) |
| Max Latency | 4,125 cycles (41.25 us) |
| Min Interval | 31 cycles |
| Max Interval | 4,126 cycles |

## Resource Utilization

| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| BRAM_18K | 0 | 280 | 0% |
| DSP | 16 | 220 | 7.3% |
| FF | 757 | 106,400 | 0.7% |
| LUT | 850 | 53,200 | 1.6% |
| URAM | 0 | 0 | -- |

### Resource Notes

- **DSP**: Exactly 16 DSP48E1 slices, one per PE. Each implements an 8x8 signed multiply-accumulate using the DSP's native 25x18 multiplier and 48-bit accumulator. HLS automatically sets DSP latency=3 to utilize internal pipeline registers.
- **BRAM**: Zero. All 16 accumulators (32-bit each, 512 bits total) are stored in flip-flops via complete array partitioning.
- **FF/LUT**: Minimal overhead from unpack logic, loop control, and AXI interface adapters.

## Interface Details

### AXI-Lite Register Map (ctrl bundle)

| Register | Description |
|----------|-------------|
| AP_CTRL (0x00) | Control register (bit 0: AP_START, bit 1: AP_DONE, bit 2: AP_IDLE) |
| k_dim | 16-bit inner dimension K (low 16 bits of 32-bit register) |

### AXI-Stream Protocol

All three stream ports use the standard AXI4-Stream protocol with signals:
- **TDATA** (32 bits): Payload data
- **TKEEP** (4 bits): Byte enables (always 0xF)
- **TSTRB** (4 bits): Byte qualifiers (always 0xF)
- **TLAST** (1 bit): End-of-transfer marker
- **TVALID/TREADY**: Handshake signals

### Block-Level Control

Uses `ap_ctrl_hs` protocol:
1. Write `k_dim` via AXI-Lite
2. Assert AP_START (write 1 to AP_CTRL[0])
3. Stream K activation words into `act_in`
4. Stream K weight words into `weight_in`
5. Read 16 result words from `result_out`
6. Poll AP_DONE (AP_CTRL[1]) or wait for interrupt

## Usage Example

```cpp
#include "mac_array.hpp"

// Helper: pack 4 INT8 values into a 32-bit word
ap_uint<32> pack_4x_int8(int8_t v0, int8_t v1, int8_t v2, int8_t v3) {
    ap_uint<32> word;
    word( 7,  0) = (ap_uint<8>)(uint8_t)v0;
    word(15,  8) = (ap_uint<8>)(uint8_t)v1;
    word(23, 16) = (ap_uint<8>)(uint8_t)v2;
    word(31, 24) = (ap_uint<8>)(uint8_t)v3;
    return word;
}

int main() {
    hls::stream<axis32_t> act_in, weight_in, result_out;

    int K = 1;
    int8_t A[4] = {1, 2, 3, 4};       // 4 activations
    int8_t B[4] = {10, 20, 30, 40};   // 4 weights

    // Feed one word each
    axis32_t act_pkt, wt_pkt;
    act_pkt.data = pack_4x_int8(A[0], A[1], A[2], A[3]);
    act_pkt.keep = 0xF; act_pkt.strb = 0xF; act_pkt.last = 1;
    act_in.write(act_pkt);

    wt_pkt.data = pack_4x_int8(B[0], B[1], B[2], B[3]);
    wt_pkt.keep = 0xF; wt_pkt.strb = 0xF; wt_pkt.last = 1;
    weight_in.write(wt_pkt);

    // Run
    mac_array(act_in, weight_in, result_out, (kdim_t)K);

    // Read 16 results (outer product: C[r][c] = A[r] * B[c])
    for (int i = 0; i < 16; i++) {
        axis32_t pkt = result_out.read();
        int32_t val = (int32_t)(ap_int<32>)pkt.data;
        printf("C[%d][%d] = %d\n", i / 4, i % 4, val);
    }
    return 0;
}
```

## Constraints and Limitations

- **K dimension**: Must be in [1, 4096]. Values above 4096 are limited by the `ap_uint<16>` type (supports up to 65535) but exceed the accumulator overflow guarantee.
- **Stateless**: Accumulators reset to zero each invocation. No partial accumulation across calls.
- **No bias**: Bias addition is not included. The host or a downstream IP adds bias to the INT32 results.
- **No requantization**: Output is raw INT32 partial sums. A separate quantizer IP handles scale/shift.
- **Fixed 4x4**: Array dimensions are compile-time constants. Changing to 8x4 or 8x8 requires recompilation.
- **Data packing**: The host must arrange activation and weight data in the specific packed byte format described above.
