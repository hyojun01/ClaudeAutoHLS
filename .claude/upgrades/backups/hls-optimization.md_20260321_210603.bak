# Skill: HLS Optimization Techniques

## Optimization Decision Tree

```
Goal: Improve Throughput?
├── YES → Pipeline innermost loops (II=1)
│         ├── II achieved? → Done
│         └── II > target? → Check synthesis log
│             ├── Memory port conflict → ARRAY_PARTITION
│             ├── Data dependency → Restructure code / break dependency
│             └── Resource limit → Partial unroll instead
│
Goal: Reduce Latency?
├── YES → Unroll loops (full or partial)
│         ├── Resources OK? → Done
│         └── Too many resources? → Partial unroll (factor=N)
│             ├── Still too high? → DATAFLOW between stages
│             └── Single-loop IP → Pipeline with lower II
│
Goal: Reduce Resources?
├── YES → Increase II (PIPELINE II=2,4,...)
│         ├── Share DSPs → BIND_OP with fabric impl
│         ├── Reduce memory → ARRAY_RESHAPE instead of PARTITION
│         └── Remove redundant logic → INLINE off (keep hierarchy)
```

## Pragma Quick Reference

| Pragma | Effect | Resource Impact | Latency Impact |
|--------|--------|----------------|----------------|
| `PIPELINE II=1` | Execute loop iterations every cycle | Moderate ↑ | Big ↓ |
| `UNROLL factor=N` | Parallelize N iterations | Big ↑ | Big ↓ |
| `ARRAY_PARTITION complete` | All elements → registers | Big ↑ (FF) | Enables parallelism |
| `ARRAY_PARTITION cyclic=N` | N memory banks | Moderate ↑ | Resolves port conflicts |
| `DATAFLOW` | Task-level pipelining | Moderate ↑ (FIFOs) | Overlap stages |
| `INLINE` | Remove function boundary | Varies | Exposes optimization |
| `LOOP_MERGE` | Combine adjacent loops | Neutral | Small ↓ |
| `LOOP_FLATTEN` | Remove loop nesting | Neutral | Small ↓ |
| `BIND_OP op=mul impl=dsp` | Force DSP for multiply | ↑ DSP, ↓ LUT | Neutral |
| `BIND_OP op=mul impl=fabric` | Force LUT for multiply | ↓ DSP, ↑ LUT | Slight ↑ |
| `BIND_STORAGE type=ram_2p` | Dual-port RAM | Neutral | Resolves port conflicts |
| `STREAM depth=N` | Set FIFO depth | ↑ BRAM/FF | Prevents stalls |
| `LATENCY min=X max=Y` | Constrain latency | Varies | Constrained |

## Common Bottleneck Patterns and Fixes

### 1. II Violation Due to Memory Conflict
**Symptom**: Synthesis reports II > 1 with "unable to schedule load/store on memory port"
**Root Cause**: Two or more array accesses in the same cycle competing for one memory port
**Fix**:
```cpp
int arr[256];
#pragma HLS ARRAY_PARTITION variable=arr type=cyclic factor=2
// Now arr has 2 ports, supporting 2 accesses per cycle
```

### 2. II Violation Due to Loop-Carried Dependency
**Symptom**: II > 1 with "dependency" warning
**Root Cause**: Iteration N depends on result of iteration N-1
**Fix options**:
- Code restructuring to break dependency chain
- Accumulator tree for reductions
- `#pragma HLS DEPENDENCE variable=arr inter false` (only if no true dependency)

### 3. High Latency from Sequential Stages
**Symptom**: Total latency = sum of all stage latencies
**Root Cause**: Stages execute sequentially
**Fix**:
```cpp
#pragma HLS DATAFLOW
read_stage(in, pipe1);
process_stage(pipe1, pipe2);
write_stage(pipe2, out);
// Now stages overlap → total latency ≈ max(stage latencies)
```

### 4. Excessive Resource Usage from Full Unroll
**Symptom**: LUT/DSP usage exceeds target
**Fix**: Use partial unroll
```cpp
for (int i = 0; i < 1024; i++) {
    #pragma HLS UNROLL factor=4  // Instead of full unroll
    #pragma HLS PIPELINE II=1
    // 4x parallelism instead of 1024x
}
```

### 5. Poor Burst Efficiency on AXI Master
**Symptom**: Low throughput on DDR access
**Fix**:
```cpp
#pragma HLS INTERFACE m_axi port=mem bundle=gmem \
    max_read_burst_length=256 num_read_outstanding=16
// Use sequential access pattern in a pipelined loop
for (int i = 0; i < N; i++) {
    #pragma HLS PIPELINE II=1
    local[i] = mem[i];  // Sequential → generates burst
}
```

## Optimization Recipes by IP Type

### Signal Processing (FIR, FFT, Convolution)
1. `ARRAY_PARTITION complete` on coefficient arrays
2. `UNROLL` on multiply-accumulate loops
3. `PIPELINE II=1` on sample processing loop
4. Use `ap_fixed` to minimize DSP usage

### Image Processing (Filter, Resize, Color Convert)
1. Line buffer pattern with `ARRAY_PARTITION`
2. `PIPELINE II=1` on pixel processing loop
3. `DATAFLOW` between read → process → write stages
4. AXI-Stream interface for streaming pixels

### Data Movement (DMA, Scatter-Gather)
1. Wide data bus (`ap_uint<512>`)
2. Maximum burst length on AXI Master
3. Separate read and write bundles
4. `DATAFLOW` between read and write paths

### Control-Dominated (Protocol Handler, FSM)
1. `PIPELINE II=1` on the FSM loop
2. Minimal unrolling (logic, not data-parallel)
3. `INLINE` on state handler functions
4. Compact data types to minimize register usage

## Before/After Analysis Template

```
============================================================
Optimization Report: <ip_name>
============================================================

BASELINE (sol1):
  Clock:    10 ns target, 8.5 ns estimated
  Latency:  1024 cycles (10,240 ns)
  II:       N/A (not pipelined)
  BRAM:     2
  DSP:      4
  FF:       523
  LUT:      1,204

OPTIMIZED (sol_opt):
  Clock:    10 ns target, 7.2 ns estimated
  Latency:  260 cycles (2,600 ns)
  II:       1 cycle
  BRAM:     4 (+2)
  DSP:      16 (+12)
  FF:       1,841 (+1,318)
  LUT:      3,402 (+2,198)

IMPROVEMENT:
  Latency:  3.94x faster
  Throughput: ~1024x higher (II=1 vs no pipeline)
  Resource cost: ~2.8x more LUT, 4x more DSP

TARGET MET: [YES/NO] (specify each target)
============================================================
```
