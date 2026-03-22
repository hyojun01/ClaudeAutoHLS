# Optimization Instruction Template: <IP Name>

## 1. Current Baseline
<!-- Reference the current synthesis results from reports/ -->
Reference the current synthesis results from reports/fft_csynth_opt.rpt.

## 2. Optimization Goals

| Metric | Current | Target | Priority |
|--------|---------|--------|----------|
| Latency (cycles) | 1598 | longer | Med |
| Initiation Interval | 256 | 256 | High |
| Throughput | | | High/Med/Low |
| LUT Usage | 5517 | more | High |
| DSP Usage | 80 | low | High |
| BRAM Usage | 107 | low | High |

## 3. Acceptable Trade-offs
<!-- e.g., "Can use 2x more DSPs if latency is halved" -->
Can be more latency if use less DSPs and BRAMs.

## 4. Specific Optimization Requests (optional)
<!-- e.g., "Pipeline the main processing loop", "Use DATAFLOW for read/process/write" -->
Maintain the current Dataflow architecture(10-stage DATAFLOW).

## 5. Constraints
<!-- e.g., "Must fit on xc7z020", "Cannot exceed 80% LUT utilization" -->
Must fit on xc7z020.
