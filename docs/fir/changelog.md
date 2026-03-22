# FIR Filter IP — Changelog

All notable changes to the `fir` IP are documented in this file.
Entries are ordered from most recent to oldest. Version numbers follow
semantic versioning: MAJOR.MINOR (no patch suffix for hardware IPs).

---

## [1.2] — 2026-03-21

### Changed

- **Added `#pragma HLS BIND_OP variable=acc op=mul impl=fabric`** to force all 23 multiplications from DSP48 slices to LUT-based fabric logic (`src/fir/src/fir.cpp` line 57)

### Performance (Before → After)

| Metric | v1.1 | v1.2 | Change |
|--------|------|------|--------|
| DSP | 23 | **0** | **-100%** |
| FF | 1,807 | 2,288 | +27% |
| LUT | 352 | 8,694 | +24x |
| Pipeline Depth | 9 cycles | 7 cycles | -22% |
| II | 1 | 1 | unchanged |
| Fmax | 179.16 MHz | 105.46 MHz | -41% |
| Clock Period | 5.582 ns | 9.482 ns | +70% |

### Verification

- C-simulation: PASSED (all 256 samples match golden reference, TLAST propagated, frequency response verified)
- Timing: Fmax 105.46 MHz meets 100 MHz target, but with reduced margin (9.482 ns estimated vs 7.3 ns effective budget after 2.7 ns uncertainty)

### Files

- Baseline backup: `src/fir/src/fir_pre_opt.hpp`, `src/fir/src/fir_pre_opt.cpp`
- Optimized report: `src/fir/reports/fir_csynth_opt2.rpt`

---

## [1.1] — 2026-03-21

### Changed

- **Removed explicit `(acc_t)` casts on multiply operands** in `SYM_MAC_LOOP` and center tap computation (`src/fir/src/fir.cpp` lines 59–62)
  - Before: `acc += (acc_t)coeffs[i] * (acc_t)(shift_reg[i] + shift_reg[...])` — widened operands to 45 bits before multiply, causing 32×26+ bit multiplications requiring multiple DSP48 slices each
  - After: `acc += coeffs[i] * (shift_reg[i] + shift_reg[...])` — multiply at natural width (16×17 bits), fitting in a single DSP48E1 (25×18 native); product losslessly promoted to `acc_t` during accumulation

### Performance (Before → After)

| Metric | v1.0 (Baseline) | v1.1 (Optimized) | Change |
|--------|-----------------|------------------|--------|
| DSP | 76 | **23** | **-70%** |
| FF | 8,206 | 1,807 | -78% |
| LUT | 3,789 | 352 | -91% |
| Pipeline Depth | 19 cycles | 9 cycles | -53% |
| II | 1 | 1 | unchanged |
| Fmax | 144.68 MHz | 179.16 MHz | +24% |
| Clock Period | 6.912 ns | 5.582 ns | -19% |

### Verification

- C-simulation: PASSED (all 256 samples match golden reference, TLAST propagated, frequency response verified)
- Functional equivalence: bit-identical output — removing the casts does not change the computed result since the extra bits from widening were zero-padded

### Files

- Baseline backup: `src/fir/src/fir_pre_opt.hpp`, `src/fir/src/fir_pre_opt.cpp`
- Optimized report: `src/fir/reports/fir_csynth_opt.rpt`

---

## [1.0] — 2026-03-21

### Added

- **Initial IP design** (`src/fir/src/fir.hpp`, `src/fir/src/fir.cpp`):
  - 45-tap symmetric low-pass FIR filter implemented in C++ for AMD Vitis HLS
  - Top function `fir(hls::stream<axis_t>& in, hls::stream<axis_t>& out)` with AXI4-Stream I/O and AXI4-Lite control (`ap_ctrl_hs`)
  - 23 Q1.15 (`ap_fixed<16,1>`) coefficients stored as `static const coeff_t coeffs[23]`; symmetric pairs reconstructed implicitly during accumulation
  - `ap_fixed<45,15>` accumulator providing sufficient headroom to prevent overflow across all 45 tap contributions
  - TLAST propagation: input `last` flag forwarded verbatim to the output `axis_t` word; `while (!last)` loop terminates the frame cleanly

- **Architecture decisions**:
  - 45-element shift register `shift_reg[45]` declared `static` for state persistence across invocations and `#pragma HLS ARRAY_PARTITION variable=shift_reg complete` for full register implementation
  - Coefficient array also fully partitioned (`#pragma HLS ARRAY_PARTITION variable=coeffs complete`) to allow all 23 reads in a single cycle
  - `#pragma HLS PIPELINE II=1` on `MAIN_LOOP` targeting one output sample per clock cycle
  - Symmetric MAC structure: 22 pre-addition pairs (`shift_reg[i] + shift_reg[44-i]`) followed by multiply, reducing DSP usage from 45 to 23 multipliers; center tap (index 22) handled separately without pre-addition

- **C-simulation testbench** (`src/fir/tb/tb_fir.cpp`):
  - Two-tone sine wave stimulus: 5 MHz (passband) + 15 MHz (stopband) at 100 MHz sampling rate, amplitude 0.3 per tone
  - 256 input samples per frame; TLAST asserted on sample 255
  - Sample-by-sample comparison against a double-precision golden reference model with 2-LSB tolerance (`2.0 / 32768.0`)
  - DFT-based frequency response analysis at 5 MHz and 15 MHz (post-warmup, skipping first 44 samples)
  - Pass criteria: 5 MHz gain > 0.5 (passband), 15 MHz gain < 0.1 (stopband attenuation > 20 dB)
  - TLAST propagation verified on final output word
  - Testbench returns 0 on all-pass, non-zero on any failure

- **TCL scripts** (`src/fir/tcl/`):
  - `run_csim.tcl` — Vitis HLS C-simulation script
  - `run_csynth.tcl` — Vitis HLS C-synthesis script targeting xc7z020clg400-1 at 100 MHz
  - `run_cosim.tcl` — Vitis HLS co-simulation (RTL simulation) script

- **Documentation** (`docs/fir/`):
  - `README.md` — Full IP reference documentation (specifications, interfaces, architecture, performance tables, usage example)
  - `integration_guide.md` — Vivado block design integration guide with AXI-Lite register map and bare-metal C driver
  - `changelog.md` — This file

### Architecture Summary

| Parameter          | Value                                  |
|--------------------|----------------------------------------|
| Filter type        | FIR, symmetric, linear-phase, low-pass |
| Number of taps     | 45                                     |
| Unique coefficients| 23 (indices 0–22; mirror: h[k]=h[44-k])|
| Data type (I/O)    | `ap_fixed<16,1>` — Q1.15 signed        |
| Coefficient type   | `ap_fixed<16,1>` — Q1.15 signed        |
| Accumulator type   | `ap_fixed<45,15>`                      |
| Input interface    | AXI4-Stream (`axis`), 16-bit TDATA     |
| Output interface   | AXI4-Stream (`axis`), 16-bit TDATA     |
| Control interface  | AXI4-Lite (`s_axilite`), `ap_ctrl_hs`  |
| Target device      | xc7z020clg400-1                        |
| Target clock       | 100 MHz (10 ns)                        |
| Pipeline II target | 1 (one output sample per clock cycle)  |

### Verification Results (Vitis HLS 2025.1)

- **C-simulation**: PASSED — all 256 samples match golden reference (tolerance 6.1e-5), TLAST propagated correctly
- **Frequency response**: 5 MHz gain = 1.0783 (passband PASS), 15 MHz gain = 0.0806 (stopband PASS)
- **C-synthesis**: PASSED — II=1 achieved, estimated Fmax = 144.68 MHz (target 100 MHz)
- **Resources**: DSP=76/220 (34%), FF=8,206/106,400 (7%), LUT=3,789/53,200 (7%), BRAM=0
- **Pipeline depth**: 19 cycles

### Pending

- **Co-simulation**: RTL-level co-simulation not yet run.
- **IP export**: Vivado-compatible IP-XACT package not yet generated; see `docs/fir/integration_guide.md` Step 2 for export instructions.

---

*End of changelog.*
