# cic_decimator — Changelog

## v1.0 — 2026-03-25

Initial implementation.

- 4-stage CIC decimation filter (N=4, R=16, M=1)
- AXI-Stream 16-bit signed I/O, `ap_ctrl_none` free-running
- 32-bit modular accumulators with bit pruning and rounding on output
- TLAST propagation through decimation boundary (OR of R input TLASTs)
- **Performance**: II=1, 5-cycle pipeline depth, Fmax=144.45 MHz
- **Resources**: 0 BRAM, 0 DSP, 393 FF, 485 LUT (< 1% xc7z020)
- **Verification**: 131/131 C-simulation checks passed across 6 test cases
- Target: xc7z020clg400-1 @ 100 MHz
