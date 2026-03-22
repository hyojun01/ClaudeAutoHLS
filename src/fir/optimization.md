# Optimization Instruction Template: <IP Name>

## 1. Current Baseline
<!-- Reference the current synthesis results from reports/ -->
Reference the current synthesis results from reports/.

## 2. Optimization Goals

| Metric | Current | Target | Priority |
|--------|---------|--------|----------|
| Latency (cycles) | 9 | more | High |
| Initiation Interval | 1 | 1 | Low |
| Throughput | | | High/Med/Low |
| LUT Usage | 352 | more | High |
| DSP Usage | 23 | 0 | High |
| BRAM Usage | 0 | 0 | Low |

## 3. Acceptable Trade-offs
<!-- e.g., "Can use 2x more DSPs if latency is halved" -->
Can be more latency if DSP usage is less.

## 4. Specific Optimization Requests (optional)
<!-- e.g., "Pipeline the main processing loop", "Use DATAFLOW for read/process/write" -->

## 5. Constraints
<!-- e.g., "Must fit on xc7z020", "Cannot exceed 80% LUT utilization" -->
Must fit on xc7z020.
Cannot use DSP.
