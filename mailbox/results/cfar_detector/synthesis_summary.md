# Synthesis Summary: cfar_detector

## Target Configuration
- **FPGA Part**: xc7z020clg400-1
- **Clock Period**: 10 ns
- **HLS Tool Version**: Vitis HLS 2025.2

## Timing
| Metric | Value |
|--------|-------|
| Target clock | 10.000 ns |
| Estimated clock | 6.772 ns |
| Uncertainty | 2.700 ns |
| Timing margin | 3.228 ns |

## Performance
| Metric | Value |
|--------|-------|
| Latency (cycles) | 68 – 65566 |
| Latency (absolute) | 0.68 µs – 655.66 µs |
| Initiation Interval | 1 cycle |
| Pipeline type | pipeline (loop MAIN_LOOP, depth=10) |

## Resource Utilization
| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|--------------------|
| BRAM_18K | 0 | 280 | 0.0 |
| DSP | 3 | 220 | 1.4 |
| FF | 2550 | 106400 | 2.4 |
| LUT | 1729 | 53200 | 3.2 |
| URAM | 0 | 0 | — |

## Assessment
- **Timing**: PASS — 3.23 ns margin, estimated Fmax 147.67 MHz exceeds 100 MHz target
- **C-Simulation**: PASS — 25/25 checks across 8 test scenarios, 0 errors
- **Resource fit**: PASS — all resources well under budget (max 3.2% utilization)
- **Meets instruction targets**: YES — II=1 achieved, 100 Msample/sec throughput, 0 BRAM, 3 DSP (spec estimated 2; 3rd DSP from mode-select MUX or tool allocation)
