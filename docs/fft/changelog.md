# Changelog: FFT (256-Point Radix-2 DIT)

## [v1.0] - 2026-03-22

### Added
- Initial 256-point Radix-2 DIT FFT IP design
- AXI-Stream I/O with 32-bit packed complex data (real[31:16], imag[15:0])
- AXI-Lite control interface (ap_ctrl_hs)
- DATAFLOW architecture: read_and_reorder → compute_fft → write_output
- Precomputed twiddle factor ROM (128 complex values, ap_fixed<16,1>)
- Bit-reversal reordering via lookup table
- Internal computation using ap_fixed<45,15> accumulator type
- C-simulation verified with 3 test cases (zeros, 3-tone sine wave, DC)
- C-synthesis completed for xc7z020clg400-1 @ 10 ns

### Performance (v1.0 Baseline)
- Latency: 4,376 cycles
- Initiation Interval: 3,596 cycles (dataflow)
- Estimated Fmax: 43.06 MHz (timing not met at 100 MHz target)
- BRAM: 19/280 (6.8%)
- DSP: 12/220 (5.5%)
- FF: 462/106,400 (0.4%)
- LUT: 1,607/53,200 (3.0%)

### Known Issues
- Estimated clock period (23.2 ns) exceeds 10 ns target due to butterfly multiply critical path
- Output truncation from acc_t to data_t may cause overflow for large-amplitude inputs

## [v1.1] - 2026-03-22

### Optimized
- Split monolithic `compute_fft` into 8 template-instantiated `butterfly_stage<0..7>` functions
- Applied 10-stage DATAFLOW: read → stage0 → stage1 → ... → stage7 → write
- Added `BIND_OP latency=3` on all butterfly multiplies to pipeline the 60-bit DSP multiply across 3 clock cycles
- Merged read + bit-reverse into a single loop (saves 256 cycles)
- Separated complex_t struct into individual real/imag arrays for clean dual-port access (eliminates in-place memory dependency)

### Performance (v1.1)
- Latency: 4,376 → 1,598 cycles (-63%)
- Initiation Interval: 3,596 → 256 cycles (-93%)
- Estimated Fmax: 43.06 → 138.85 MHz (+222%)
- Estimated Clock: 23.2 → 7.2 ns (timing **met**)
- BRAM: 19 → 107 (38.2%)
- DSP: 12 → 80 (36.4%)
- FF: 462 → 4,698 (4.4%)
- LUT: 1,607 → 5,517 (10.4%)

### Fixed
- Clock timing violation resolved (was 23.2 ns, now 7.2 ns vs 10 ns target)
- Butterfly II improved from 3 to 1 (memory dependency eliminated)

## [v1.2] - 2026-03-22

### Optimized
- Converted all 18 PIPO buffers from BRAM to LUTRAM (`BIND_STORAGE impl=lutram`)
- BRAM usage reduced from 107 (38%) to 15 (5%)

### Performance (v1.2)
- Latency: 1,672 cycles (+5% vs v1.1)
- Initiation Interval: 256 cycles (unchanged)
- Estimated Fmax: 138.85 MHz (unchanged)
- BRAM: 15/280 (5.4%) — **86% reduction from v1.1**
- DSP: 80/220 (36.4%) — unchanged (fabric multiplies would exceed LUT budget)
- FF: 21,255/106,400 (20.0%)
- LUT: 18,437/53,200 (34.7%)

## [v1.3] - 2026-03-22

### Optimized
- Narrowed accumulator type from `ap_fixed<45,15>` to `ap_fixed<25,10>`
  - 10 integer bits: range [-512, 512) — sufficient for 256-point FFT (max output 256)
  - 15 fractional bits: matches input data_t precision
  - 25 bits total: fits in DSP48E1 A-input, enabling single-DSP multiply (16×25)
- Reduced BIND_OP latency from 3 to 2 (single-DSP needs fewer pipeline stages)

### Performance (v1.3)
- Latency: 1,662 cycles (unchanged)
- Initiation Interval: 256 cycles (unchanged)
- Estimated Fmax: 137.39 MHz (timing met)
- BRAM: 15/280 (5.4%) — unchanged
- DSP: 30/220 (13.6%) — **63% reduction from v1.2**
- FF: 15,593/106,400 (14.7%)
- LUT: 14,209/53,200 (26.7%)
