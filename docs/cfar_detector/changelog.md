# Changelog: CFAR Detector

## [v1.0] - 2026-03-23
### Added
- Initial IP design: CA/GO/SO-CFAR detector with sliding window architecture
- AXI-Stream input (32-bit power) and output (64-bit packed detection result)
- AXI-Lite control: alpha, cfar_mode, num_range_cells, ap_ctrl_hs
- Running sum architecture for O(1) per-sample computation
- Edge suppression for incomplete reference windows
- C-simulation verified: 25/25 test checks passed across 8 test scenarios
- C-synthesis completed: II=1, Fmax=147.67 MHz, timing met with 3.23 ns margin

### Performance (v1.0)
- Latency: 68–65566 cycles (sweep-length dependent)
- Initiation Interval: 1 cycle
- Resources: 3 DSP, 2550 FF, 1729 LUT, 0 BRAM
- Device utilization on xc7z020: DSP 1.4%, FF 2.4%, LUT 3.2%
