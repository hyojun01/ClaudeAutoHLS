# Skill: FPGA System Architecture & Interface Design

This skill provides the engineering methodology for designing the FPGA-level system architecture of an HLS IP: choosing the right interfaces, data flow patterns, data types, and integration strategy. These decisions must be made before writing the instruction.md, not improvised during implementation.

## 1. Interface Protocol Selection

### Decision Tree

```
For each port, ask these questions in order:

Q1: Is the data a continuous, high-bandwidth stream?
    (samples arriving every cycle, indefinite or burst length)
    YES → AXI4-Stream (axis)
    NO  → go to Q2

Q2: Is the data a large block in external memory (DDR/HBM)?
    (needs burst access, >1KB per transaction)
    YES → AXI4 Master (m_axi)
    NO  → go to Q3

Q3: Is the data a control register, configuration parameter, or small result?
    (scalar values, set once per invocation or read-back after completion)
    YES → AXI4-Lite (s_axilite)
    NO  → go to Q4

Q4: Is the port a simple wire/flag with no handshaking?
    YES → ap_none (input only) or ap_vld (output with valid)
    NO  → AXI-Stream (default for streaming) or ap_fifo (for FIFO semantics)
```

### Protocol Characteristics

| Protocol | Direction | Throughput | Latency | Handshaking | Typical Use |
|----------|-----------|------------|---------|-------------|-------------|
| AXI4-Stream (axis) | Unidirectional | 1 word/cycle | 0 cycles (combinational) | TVALID/TREADY | Streaming data path |
| AXI4-Lite (s_axilite) | Bidirectional | ~1 word/10 cycles | 2-4 cycles | AXI4-Lite protocol | Control/status registers |
| AXI4 Master (m_axi) | Bidirectional | Up to 1 word/cycle (burst) | 10-100 cycles (DDR) | AXI4 full protocol | DDR/HBM bulk access |
| ap_none | Input only | Wire speed | 0 | None | Static configuration |
| ap_vld / ap_ack | Either | 1 word/cycle | 0-1 | Valid or Ack signal | Simple handshake |
| ap_fifo | Either | 1 word/cycle | 0 | FIFO empty/full | Inter-IP FIFOs |

### Interface Design Rules

**AXI4-Stream:**
- Use `ap_axiu<DATA_WIDTH, 0, 0, 0>` as the stream element type for top-level ports
- TDATA width should be a power of 2 (8, 16, 32, 64) for system compatibility
- Pack multiple fields into TDATA if they are always produced/consumed together:
  ```
  Example: I/Q pair → TDATA[31:16] = I, TDATA[15:0] = Q
  Example: Pixel RGB → TDATA[23:16] = R, TDATA[15:8] = G, TDATA[7:0] = B
  ```
- Always propagate TLAST for packet/frame boundary signaling
- Set TKEEP = all-ones (all bytes valid) unless partial words are expected

**AXI4-Lite:**
- Group all scalar control/status ports under a **single** s_axilite bundle to minimize interface overhead (~230 LUT per bundle):
  ```cpp
  #pragma HLS INTERFACE s_axilite port=param_a bundle=ctrl
  #pragma HLS INTERFACE s_axilite port=param_b bundle=ctrl
  #pragma HLS INTERFACE s_axilite port=return  bundle=ctrl
  ```
- Note: each s_axilite parameter occupies a 64-bit-aligned register offset (0x10, 0x18, 0x20, ...)
- Document the register map in instruction.md for driver development

**AXI4 Master:**
- Separate read and write into different bundles if accessing different memory regions:
  ```cpp
  #pragma HLS INTERFACE m_axi port=input  bundle=gmem_rd
  #pragma HLS INTERFACE m_axi port=output bundle=gmem_wr
  ```
- Set burst parameters for optimal DDR throughput:
  ```cpp
  #pragma HLS INTERFACE m_axi port=input max_read_burst_length=256 num_read_outstanding=16
  ```
- Use sequential access patterns (pipelined loops) to enable burst inference
- Buffer data locally before processing: DDR latency (~100 cycles) kills pipeline efficiency

**Block-level control protocol:**
- Default: `ap_ctrl_hs` (start/done/idle handshake via s_axilite)
- For free-running streaming IPs: `ap_ctrl_none` (no software control, IP runs continuously)
  ```cpp
  #pragma HLS INTERFACE ap_ctrl_none port=return
  ```

### Interface Overhead Budget

When estimating total resource usage, add interface overhead separately:

| Interface Type | LUT Overhead | FF Overhead | BRAM | DSP |
|---------------|-------------|-------------|------|-----|
| AXI-Lite bundle (s_axilite) | ~230 | ~160 | 0 | 0 |
| AXI-Stream port (axis) | ~10 | ~30 | 0 | 0 |
| AXI Master bundle (m_axi) | ~600 | ~500 | 0-2 | 0 |
| ap_none / ap_vld | ~0 | ~0 | 0 | 0 |

Multiple s_axilite ports sharing one bundle = one overhead instance.
Each m_axi bundle incurs its own overhead regardless of number of ports using it.

## 2. Data Flow Architecture Patterns

### Pattern Selection

| Pattern | When to Use | HLS Mechanism | Buffer Cost |
|---------|-------------|---------------|-------------|
| **Pure streaming** | Each output depends only on current + recent inputs; no block boundaries | Single pipelined loop, shift register | Shift register (small) |
| **Block processing** | Transform requires collecting N samples before producing output (FFT, DCT) | Pipelined loop with local array, double-buffer for overlap | 2× block_size × data_width |
| **DATAFLOW pipeline** | Multiple distinct processing stages that can overlap | `#pragma HLS DATAFLOW` with hls::stream between stages | FIFO/PIPO per stage boundary |
| **Load-compute-store** | Bulk data in DDR, process locally, write back | 3 sequential phases with local BRAM buffers | ≥ block_size × data_width |
| **Streaming DATAFLOW** | Combination of streaming and stage overlap | DATAFLOW with streaming stages, each consuming/producing 1 element/cycle | FIFO depth per stage |

### Pattern Details

**Pure Streaming** (FIR, moving average, pixel filter):
```
Input → [Shift Register + Computation] → Output
         (all in one pipelined loop)

Key properties:
- One sample in, one sample out per cycle (II=1)
- State held in shift registers or static variables
- No block boundaries needed
- Lowest latency (= pipeline depth)
- Lowest buffer cost (= window size)
```

**Block Processing** (FFT, block-based codec):
```
[Collect N samples] → [Process N-point block] → [Output N results]

Key properties:
- Must buffer full block before processing
- Latency ≥ N + processing_cycles
- Double-buffering enables continuous streaming:
  While processing block K, collect block K+1
- BRAM cost = 2 × N × data_width (ping-pong)
```

**DATAFLOW Pipeline** (multi-stage processing):
```
Stage 1 → [FIFO] → Stage 2 → [FIFO] → Stage 3

Key properties:
- Stages execute concurrently once FIFOs are primed
- Total latency ≈ max(stage latencies) + FIFO priming
- Throughput limited by slowest stage
- FIFO depth: set to match the latency imbalance between adjacent stages
  (default: 2 is often insufficient; size based on actual stage latency ratio)
```

### Internal Stream Type Rules

**Critical**: Vitis HLS (2025.1+) restricts `ap_axiu`/`hls::axis` types to top-level ports only.

```
For DATAFLOW inter-stage streams:
  ✗ hls::stream<ap_axiu<32,0,0,0>> internal_fifo;  // SYNTHESIS ERROR
  ✓ hls::stream<ap_uint<32>> internal_fifo;          // Plain data type
  ✓ hls::stream<my_data_t> internal_fifo;             // Custom struct

Pattern for top-level AXI-Stream with internal plain streams:
  read_stage:  ap_axiu → extract data → hls::stream<data_t>
  processing:  hls::stream<data_t> → hls::stream<data_t>
  write_stage: hls::stream<data_t> → pack → ap_axiu
```

### Persistent State Design

When an IP must maintain state across invocations (NCO phase, IIR state, CIC integrators):

```
Design questions:
1. What state must persist? → Declare as `static` local variable
2. Does the user need to reset the state? → Add a reset control port (s_axilite)
3. Does the state affect pipeline II? → Check for loop-carried dependency on state
4. Is the state large? → Consider BRAM storage with explicit read/write control

Rules:
- static variables persist across function calls (HLS synthesizes them as registers/BRAM)
- Initialize static arrays with `= {}` or `= {0}` for deterministic reset
- Expose reset control via s_axilite if the system requires phase-coherent restart
```

## 3. Data Type Engineering

### Systematic Type Selection Process

```
For each signal in the design:

1. Determine the value range:
   - What is the minimum and maximum value this signal can take?
   - Is it signed or unsigned?
   - integer_bits = ceil(log2(max_abs_value + 1)) + 1 (for signed)

2. Determine the precision requirement:
   - What SNR or accuracy is needed? → fractional_bits (see algorithm-analysis.md)
   - Or: what is the input precision? → preserve or match

3. Choose the type:
   total_bits = integer_bits + fractional_bits
   → ap_fixed<total_bits, integer_bits> (signed)
   → ap_ufixed<total_bits, integer_bits> (unsigned, fractional)
   → ap_int<total_bits> (signed integer)
   → ap_uint<total_bits> (unsigned integer)

4. Check against DSP48 native width:
   If this signal is a multiply operand:
     7-series: keep ≤ 25 bits (A-port) or ≤ 18 bits (B-port)
     If wider: accept DSP chaining cost or narrow the type

5. Check for boundary value hazards:
   If the value can reach exactly +2^(I-1) in ap_fixed<W,I>:
     This value is NOT representable → use AP_SAT or clamp
```

### Common Type Recipes by Domain

**DSP / Signal Processing:**
```cpp
// Audio / baseband: Q1.15 (standard)
typedef ap_fixed<16, 1> sample_t;        // range: [-1, +1)
typedef ap_fixed<16, 1> coeff_t;         // match input precision
typedef ap_fixed<40, 8> acc_t;           // headroom for accumulation

// High-precision audio: Q1.23
typedef ap_fixed<24, 1> sample_hd_t;     // 24-bit audio

// Wideband / radar ADC: integer samples
typedef ap_int<14> adc_t;                // 14-bit ADC output
typedef ap_int<16> baseband_t;           // after digital downconversion
```

**Communications:**
```cpp
// I/Q baseband: Q1.15 per component
typedef ap_fixed<16, 1> iq_t;           // I or Q component
typedef ap_uint<32> iq_packed_t;         // I[31:16] | Q[15:0]

// Soft bits (LLR): Q4.4 to Q6.6
typedef ap_fixed<8, 4> llr_t;           // soft decision, 8-bit
typedef ap_fixed<12, 6> llr_wide_t;     // higher precision LLR

// Phase accumulator: unsigned, full-range
typedef ap_uint<32> phase_t;             // frequency resolution = f_clk / 2^32
```

**Image Processing:**
```cpp
// Pixel channel: unsigned integer
typedef ap_uint<8> pixel_t;              // 8-bit grayscale or per-channel
typedef ap_uint<24> rgb_t;               // packed R[23:16] G[15:8] B[7:0]

// Gradient / intermediate: signed, wider
typedef ap_int<16> grad_t;              // Sobel gradient output
typedef ap_int<11> conv_t;              // 3×3 kernel output (8-bit × 3-bit × 9 terms)
```

**Deep Learning:**
```cpp
// INT8 quantized inference
typedef ap_fixed<8, 4> weight_t;         // INT8-equivalent
typedef ap_fixed<8, 4> act_t;            // activation
typedef ap_fixed<32, 16> acc_t;          // MAC accumulator (8+8+ceil(log2(N)) bits)
```

### Accumulator Sizing Formula

```
For a sum of N products of types A × B:
  acc_integer_bits = A_integer + B_integer + ceil(log2(N))
  acc_fractional_bits = A_fractional + B_fractional
  acc_total_bits = acc_integer_bits + acc_fractional_bits

Example: 45-tap FIR, input Q1.15, coeff Q1.15:
  integer = 1 + 1 + ceil(log2(45)) = 2 + 6 = 8
  fractional = 15 + 15 = 30
  acc_total = 8 + 30 = 38 → ap_fixed<38, 8>
  
  But for DSP48 efficiency (25-bit A-port):
  Consider narrowing to ap_fixed<25, 8> with 17 fractional bits
  Trade-off: 13 bits less precision vs. 1 DSP per multiply instead of 2
```

## 4. Resource Budget Methodology

### Step-by-Step Resource Estimation

```
1. Count multiply operations per output sample:
   → DSP_core = multiplies × (1 if within DSP48 width, 2-4 if wider)

2. Count memory structures:
   → BRAM_core = sum of (array_bits / 18432) for each buffer/LUT ≥ 1024 bits
   → Shift registers and small arrays → LUT/FF, not BRAM

3. Estimate data path LUT:
   → LUT_datapath ≈ (adders × bit_width) + (muxes × bit_width / 4) + control_logic

4. Add interface overhead:
   → LUT_interface = (s_axilite_bundles × 230) + (axis_ports × 10) + (m_axi_bundles × 600)
   → FF_interface = (s_axilite_bundles × 160) + (axis_ports × 30) + (m_axi_bundles × 500)

5. Total estimate:
   → DSP_total = DSP_core
   → BRAM_total = BRAM_core + BRAM_interface (m_axi buffers)
   → LUT_total = LUT_datapath + LUT_interface + LUT_control (100-300 for FSM/sequencing)
   → FF_total = FF_datapath + FF_interface + FF_pipeline

6. Check against device budget:
   → Flag any resource > 50% of target device capacity
   → Target ≤ 60% overall to leave routing margin
```

### Sanity Check Table

| IP Type | Typical DSP | Typical BRAM | Typical LUT | Notes |
|---------|-------------|-------------|-------------|-------|
| N-tap FIR (II=1) | N/2 (symmetric) or N | 0 | 200-1000 | Shift reg in FF |
| N-point FFT (radix-2) | log2(N) × 2 per stage, or N × log2(N)/2 shared | 2-10 | 1000-5000 | Depends on pipeline vs. memory |
| CORDIC (16-bit, iterative) | 0 | 0 | 300-600 | Shift-add only |
| NCO (10-bit LUT) | 0 | 1 | 100-200 | Quarter-wave LUT |
| 3×3 image filter | 9 (or 0 if kernel is small integer) | 2-4 (line buffers) | 500-1000 | Line buffer in BRAM |
| Viterbi decoder (K=7) | 0 | 0-4 | 2000-5000 | Comparators, ACS |

## 5. System Integration Considerations

### Clock Domain Awareness

```
Questions to address in the instruction.md:
1. What clock frequency is the IP targeting? (must match system clock or use CDC)
2. Is the IP free-running or software-triggered?
   Free-running: ap_ctrl_none, starts immediately, no start/done signals
   Software-triggered: ap_ctrl_hs (default), needs driver to issue start
3. Does the IP need to interface with IPs at different clock rates?
   If yes: specify CDC FIFO requirements in the integration guide
```

### Data Packing for System Bus Width

```
If the system data bus is 32/64/128 bits wide:
  Pack multiple samples into one bus word for bandwidth efficiency.

Example: 16-bit I/Q samples on a 32-bit AXI-Stream bus:
  TDATA[31:16] = I (cosine / in-phase)
  TDATA[15:0]  = Q (sine / quadrature)
  → 1 I/Q pair per clock cycle

Example: 8-bit pixels on a 64-bit AXI-Stream bus:
  Pack 8 pixels per word → 8 pixels/cycle throughput
  TKEEP indicates valid bytes on the last word of a row
```

### Upstream/Downstream IP Compatibility

```
When specifying an IP, consider:
1. What IP produces the input data? → Match input type and interface
2. What IP consumes the output data? → Match output type and interface
3. Are there rate mismatches? → May need FIFO with depth sizing
4. Is there a frame/packet structure? → TLAST must be aligned

Example chain: ADC → DDC → FIR → FFT → Detector
  All use AXI-Stream with matching TDATA widths
  Each IP propagates TLAST for frame boundaries
  FIFO between stages sized to handle latency differences
```

### Test Scenario Design Guidance

```
Every instruction.md should include test scenarios that cover:

1. Functional correctness:
   - Known-answer test (e.g., impulse response for FIR = coefficient values)
   - Reference model comparison (C++ double-precision vs. fixed-point output)

2. Boundary conditions:
   - Maximum/minimum input values → check for overflow/saturation
   - Zero input → verify zero or expected DC output
   - All-ones input → stress test accumulator range

3. Interface behavior:
   - TLAST generation and propagation
   - Back-to-back transactions (no gap between packets)
   - State continuity across invocations (for stateful IPs)

4. Precision verification:
   - Maximum absolute error vs. double-precision reference
   - RMS error over a sweep of representative inputs
   - SFDR or SNR measurement for signal processing IPs

5. Stress test:
   - Long burst (>1000 samples) to verify no drift or state corruption
   - Rapid parameter changes between invocations
```
