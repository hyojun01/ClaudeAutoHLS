# Synthesis Summary: cic_decimator

## Target Configuration
- **FPGA Part**: xc7z020clg400-1
- **Clock Period**: 10 ns
- **HLS Tool Version**: Vitis HLS 2025.2

## Timing
| Metric | Value |
|--------|-------|
| Target clock | 10.0 ns |
| Estimated clock | 6.923 ns |
| Uncertainty | 2.7 ns |
| Timing margin | 3.077 ns |

## Performance
| Metric | Value |
|--------|-------|
| Latency (cycles) | 5 |
| Latency (absolute) | 50 ns |
| Initiation Interval | 1 cycle |
| Pipeline type | function pipeline (flp) |

## Resource Utilization
| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|-----------------|
| BRAM_18K | 0 | 280 | 0.0 |
| DSP | 0 | 220 | 0.0 |
| FF | 393 | 106400 | 0.4 |
| LUT | 485 | 53200 | 0.9 |
| URAM | 0 | 0 | 0.0 |

## Assessment
- **Timing**: PASS — 3.077 ns slack, estimated Fmax 144.45 MHz exceeds 100 MHz target
- **C-Simulation**: PASS — 131 checks across 6 test cases, 0 error vs double-precision reference
- **Resource fit**: PASS — 0 DSP, 0 BRAM, minimal LUT/FF (<1% utilization)
- **Meets instruction targets**: YES — II=1 achieved, 5-cycle pipeline depth, multiplier-free, timing met
