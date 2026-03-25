# IP Instruction: mac_array

## 1. Functional Description

A 4x4 output-stationary MAC (Multiply-Accumulate) array for INT8 deep learning inference. The IP contains 16 processing elements arranged in a 4-row by 4-column grid, computing one tile of a matrix multiplication per invocation:

```
C_tile[4][4] += A_tile[4][K] x B_tile[K][4]
```

**Operating phases:**
1. **Compute phase** (K cycles): Each cycle, one packed activation word (4 x INT8) and one packed weight word (4 x INT8) are read from the input streams. All 16 PEs execute a multiply-accumulate in parallel — PE[r][c] accumulates `act[r] * weight[c]`.
2. **Drain phase** (16 cycles): The 16 INT32 accumulated results are written to the output stream in row-major order (row 0 col 0..3, row 1 col 0..3, ..., row 3 col 0..3).

For larger matrix multiplications C[M][N] = A[M][K] x B[K][N], the host software tiles the problem into `ceil(M/4) x ceil(N/4)` invocations, each processing the full K dimension. The IP is stateless — accumulators are zeroed at the start of each invocation.

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| act_in | IN | ap_axiu<32,0,0,0> | 32 | AXI-Stream (axis) | Packed activations: 4 x INT8 per word. K words per invocation. Byte layout: [3]=row3, [2]=row2, [1]=row1, [0]=row0. TLAST on last word. |
| weight_in | IN | ap_axiu<32,0,0,0> | 32 | AXI-Stream (axis) | Packed weights: 4 x INT8 per word. K words per invocation. Byte layout: [3]=col3, [2]=col2, [1]=col1, [0]=col0. TLAST on last word. |
| result_out | OUT | ap_axiu<32,0,0,0> | 32 | AXI-Stream (axis) | One INT32 result per word. 16 words per invocation (row-major). TLAST on 16th word. |
| k_dim | IN | ap_uint<16> | 16 | AXI-Lite (s_axilite), bundle=ctrl | Inner dimension K. Valid range: 1–4096. |
| return | — | — | — | AXI-Lite (s_axilite), bundle=ctrl | Block-level control (ap_ctrl_hs): start/done/idle. |

**Interface rationale:**
- AXI-Stream for data paths: activations and weights arrive as continuous bursts at 1 word/cycle; streaming avoids DDR latency.
- AXI-Lite for k_dim and control: scalar parameter set once per invocation, low bandwidth.
- Single s_axilite bundle (`ctrl`) for k_dim + return: minimizes adapter overhead.

## 3. Data Types

```cpp
#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// Array dimensions (compile-time constants)
const int ROWS = 4;    // PE array rows (activation/output M-tile dimension)
const int COLS = 4;    // PE array columns (weight/output N-tile dimension)
const int MAX_K = 4096; // Maximum inner dimension

// AXI-Stream element (top-level ports only)
typedef ap_axiu<32, 0, 0, 0> axis32_t;

// Core data types
typedef ap_int<8>   act_t;     // INT8 activation [-128, 127]
typedef ap_int<8>   weight_t;  // INT8 weight [-128, 127]
typedef ap_int<32>  acc_t;     // 32-bit accumulator (sufficient for K<=4096)
typedef ap_uint<16> kdim_t;    // Inner dimension parameter
```

**Accumulator sizing justification:**
- Product width: 8 (act) + 8 (weight) = 16 bits signed
- Accumulation growth: ceil(log2(4096)) = 12 bits
- Required accumulator: 16 + 12 = 28 bits minimum
- Using 32 bits provides 4 bits of headroom, supporting K up to 65535 without overflow
- Worst-case magnitude: 4096 x 128 x 128 = 67,108,864 (fits in 27 bits unsigned, 28 bits signed)

## 4. Algorithm / Processing

### Compute Phase (K cycles, pipelined at II=1)
```
Initialize acc[ROWS][COLS] = 0

COMPUTE_K: for k = 0 to K-1:       // PIPELINE II=1
    Read act_word from act_in
    Read wt_word from weight_in

    // Unpack INT8 values from 32-bit words
    act[0..3] = unpack_4x_int8(act_word.data)
    wt[0..3]  = unpack_4x_int8(wt_word.data)

    MAC_ROW: for r = 0 to ROWS-1:   // UNROLL complete
        MAC_COL: for c = 0 to COLS-1: // UNROLL complete
            acc[r][c] += (acc_t)act[r] * (acc_t)wt[c]
```

### Drain Phase (16 cycles)
```
DRAIN_ROW: for r = 0 to ROWS-1:
    DRAIN_COL: for c = 0 to COLS-1:  // PIPELINE II=1
        out_word.data = acc[r][c]
        out_word.last = (r == ROWS-1 && c == COLS-1) ? 1 : 0
        out_word.keep = 0xF
        Write out_word to result_out
```

### Data Feeding Convention

The host must pre-arrange data in column-of-activations / row-of-weights order:

**Activation stream** (K words, one per k-step):
```
Word k: { A[3][k], A[2][k], A[1][k], A[0][k] }  (byte 3 = row 3, byte 0 = row 0)
```

**Weight stream** (K words, one per k-step):
```
Word k: { B[k][3], B[k][2], B[k][1], B[k][0] }  (byte 3 = col 3, byte 0 = col 0)
```

**Result stream** (16 words, row-major):
```
Words 0-3:   C[0][0], C[0][1], C[0][2], C[0][3]
Words 4-7:   C[1][0], C[1][1], C[1][2], C[1][3]
Words 8-11:  C[2][0], C[2][1], C[2][2], C[2][3]
Words 12-15: C[3][0], C[3][1], C[3][2], C[3][3]
```

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target Latency | K + 16 + pipeline overhead cycles per tile |
| Target II | 1 (compute loop) |
| Target Throughput | 16 MACs/cycle = 3.2 GOPS @ 100 MHz |

### Expected Resource Usage

**Core logic:**
- DSP: 16 (one 8x8 multiply per PE, well within DSP48 25x18 native width)
- BRAM: 0 (16 x 32-bit accumulators fit in registers)
- LUT (datapath): ~800 (16 x 32-bit adders, unpack logic, control FSM)
- FF (datapath): ~600 (accumulators, pipeline registers)

**Interface overhead:**
- 1 x AXI-Lite bundle (ctrl): ~300 LUT, ~200 FF (base + 1 register)
- 3 x AXI-Stream ports: ~30 LUT, ~90 FF

**With HLS infrastructure overhead (2.0-2.5x for small design):**

| Resource | Core Estimate | Interface Overhead | Total Estimate | xc7z020 Capacity | Utilization |
|----------|---------------|--------------------|----------------|-------------------|-------------|
| BRAM_18K | 0 | 0 | 0 | 280 | 0% |
| DSP | 16 | 0 | 16 | 220 | 7% |
| FF | ~600 | ~290 | ~1500–2000 | 106,400 | 1–2% |
| LUT | ~800 | ~330 | ~2000–2800 | 53,200 | 4–5% |

## 6. Test Scenarios

### Test 1: Known-Answer — Small K
- **Input**: K=4, activations and weights set to small known integers (e.g., A = [[1,2,3,4],[5,6,7,8],[9,10,11,12],[13,14,15,16]], B = transpose of same or simple pattern)
- **Expected**: Hand-computed 4x4 result matrix
- **Purpose**: Verify basic MAC and drain correctness

### Test 2: Identity Multiplication
- **Input**: K=4, weight matrix = identity (B[k][c] = (k==c) ? 1 : 0)
- **Expected**: Output row r = sum of activation row r (each output = one activation value when K=4 identity)
- **Purpose**: Verify data routing — each PE accumulates the correct activation-weight pair

### Test 3: Boundary Values — INT8 Extremes
- **Input**: K=1, all activations = 127, all weights = 127 → each output = 127 x 127 = 16129
- **Input**: K=1, all activations = -128, all weights = 127 → each output = -16256
- **Input**: K=1, all activations = -128, all weights = -128 → each output = 16384
- **Purpose**: Verify correct signed multiplication at INT8 boundaries

### Test 4: Accumulator Stress — Large K
- **Input**: K=4096, all activations = 127, all weights = 127
- **Expected**: Each output = 4096 x 16129 = 66,064,384 (fits in 27 bits)
- **Purpose**: Verify no accumulator overflow or drift over long accumulation

### Test 5: Zero Inputs
- **Input**: K=8, all activations = 0
- **Expected**: All outputs = 0
- **Purpose**: Verify correct zero handling and accumulator initialization

### Test 6: Single Step (K=1)
- **Input**: K=1, act = {1, 2, 3, 4}, wt = {10, 20, 30, 40}
- **Expected**: C[r][c] = act[r] * wt[c] (outer product), e.g., C[0][0]=10, C[0][1]=20, C[2][3]=120
- **Purpose**: Verify correct operation without accumulation loop

## 7. Additional Notes

- **Stateless design**: Accumulators reset to zero at the start of each invocation. No persistent state across calls.
- **Bias addition**: Not included. If needed, the host or a downstream IP adds bias to the INT32 results before requantization. This keeps the MAC array composable.
- **Requantization**: Not included. Output is raw INT32 partial sums. A separate quantizer IP (or host postprocessing) handles scale/shift back to INT8 for the next layer.
- **Tiling controller**: Not included. The host software is responsible for partitioning large matrices into 4x4 tiles, feeding the correct data slices, and assembling results. This IP is a single-tile compute primitive.
- **Future extensions**: The array dimensions (ROWS, COLS) are compile-time constants. Changing to 8x4 (32 DSPs) or 8x8 (64 DSPs) is straightforward if the resource budget allows.
