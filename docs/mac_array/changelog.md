# Changelog: mac_array

## [1.0.0] - 2026-03-25

### Added
- Initial implementation of 4x4 output-stationary MAC array IP
- 16 parallel processing elements with INT8 inputs and INT32 accumulators
- Compute phase: K cycles at II=1, fully unrolled 4x4 MAC grid
- Drain phase: 16 cycles at II=1, row-major output with TLAST
- AXI-Stream input (32-bit packed 4xINT8) for activations and weights
- AXI-Stream output (32-bit INT32) for results
- AXI-Lite control bundle for k_dim parameter and ap_ctrl_hs
- Comprehensive testbench with 6 test scenarios (144 individual checks):
  - Known-answer small K (K=4, A*B^T)
  - Identity multiplication (K=4, B=I)
  - INT8 boundary values (K=1, +127/+127, -128/+127, -128/-128)
  - Accumulator stress (K=4096, max values)
  - Zero inputs (K=8)
  - Single step outer product (K=1)

### Synthesis Metrics (xc7z020-clg400-1, 100 MHz)
- Timing: 6.283 ns estimated (3.717 ns slack)
- Compute loop II: 1 (target met)
- Drain loop II: 1 (target met)
- Compute pipeline depth: 5 cycles
- Estimated Fmax: 159.15 MHz
- BRAM_18K: 0 (0%)
- DSP: 16 (7.3%)
- FF: 757 (0.7%)
- LUT: 850 (1.6%)
