# Synthesis Summary: mac_array

## Target Configuration
- **FPGA Part**: xc7z020clg400-1
- **Clock Period**: 10 ns
- **HLS Tool Version**: Vitis HLS 2025.2

## Timing
| Metric | Value |
|--------|-------|
| Target clock | 10.0 ns |
| Estimated clock | 6.283 ns |
| Uncertainty | 2.7 ns |
| Timing margin | 3.717 ns |

## Performance
| Metric | Value |
|--------|-------|
| Latency (cycles) | 30 (best, K=1) to 4125 (worst, K=4096) |
| Latency (absolute) | 0.30 us (best) to 41.25 us (worst) |
| Initiation Interval | 1 cycle (both compute and drain loops) |
| Pipeline type | none (sequential compute then drain) |

## Resource Utilization
| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|--------------------|
| BRAM_18K | 0 | 280 | 0.0 |
| DSP | 16 | 220 | 7.3 |
| FF | 757 | 106400 | 0.7 |
| LUT | 850 | 53200 | 1.6 |
| URAM | 0 | 0 | 0.0 |

## Assessment
- **Timing**: PASS -- 3.717 ns slack, estimated Fmax 159.15 MHz (59% above target)
- **C-Simulation**: PASS -- 144/144 checks passed across 6 test scenarios (8 sub-tests)
- **Resource fit**: PASS -- 16 DSPs (7.3%), minimal BRAM/FF/LUT usage
- **Meets instruction targets**: YES -- II=1 achieved on compute loop, 16 DSPs as specified, all functional requirements met
