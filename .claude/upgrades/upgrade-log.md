# Environment Upgrade Log

This log tracks all environment upgrades — applied, rejected, and rolled back.
Entries are append-only. Never delete or modify existing entries.

---

<!-- Upgrade entries will be appended below this line -->
<!-- Format:
## [UPG-XXXX] <title> — <date>
- **Status**: Applied / Rejected / Rolled Back
- **Trigger**: <what prompted this>
- **Target**: <file path>
- **Category**: Additive skill / Rule refinement / Agent update / Command mod / Template update / Settings change / Structural
- **Priority**: HIGH / MEDIUM / LOW
- **Session**: <ip_name> / <stage>
- **Summary**: <one-line description>
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/<filename>_<timestamp>.bak`
- **Rollback**: <instructions or "N/A">
-->

## [UPG-0001] Add "Wide Cast in MAC Loop" bottleneck pattern — 2026-03-21
- **Status**: Applied
- **Trigger**: During `fir` optimization, explicit `(acc_t)` casts caused 76 DSPs instead of 23. Removing casts reduced DSP by 70% and improved all metrics.
- **Target**: `.claude/skills/hls-optimization.md`
- **Category**: Additive skill
- **Priority**: HIGH
- **Session**: fir / optimize-ip
- **Summary**: Added "### 6. Excessive DSP Usage from Wide Casts in MAC Loops" to Common Bottleneck Patterns with BAD/GOOD code examples and evidence from the fir IP.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-optimization.md_20260321_*.bak`
- **Rollback**: Remove section "### 6. Excessive DSP Usage from Wide Casts in MAC Loops" from `.claude/skills/hls-optimization.md`

## [UPG-0002] Add ap_fixed boundary value overflow pitfall — 2026-03-22
- **Status**: Applied
- **Trigger**: During `fft` design, `tw_real[0] = 1.0` silently wrapped to `-1.0` in `ap_fixed<16,1>` due to default `AP_WRAP` overflow mode, inverting all stage-0 butterfly outputs. Bug was only caught by C-simulation.
- **Target**: `.claude/rules/hls-coding.md`
- **Category**: Rule refinement
- **Priority**: HIGH
- **Session**: fft / design-ip
- **Summary**: Added "### Common Pitfalls" section to HLS coding standards with rules for `ap_fixed` boundary value initialization, clamping, and `AP_SAT` usage.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-coding.md_20260322_*.bak`
- **Rollback**: Remove "### Common Pitfalls" section (between "Forbidden Constructs" and "Allowed Constructs") from `.claude/rules/hls-coding.md`

## [UPG-0003] Add DATAFLOW PIPO BRAM-to-LUTRAM pattern — 2026-03-22
- **Status**: Applied
- **Trigger**: During `fft` optimization (v1.2), PIPO buffers consumed 107 BRAM. BIND_STORAGE with lutram reduced BRAM to 15 (86% reduction).
- **Target**: `.claude/skills/hls-optimization.md`
- **Category**: Additive skill
- **Priority**: HIGH
- **Session**: fft / optimize-ip
- **Summary**: Added "### 7. Excessive BRAM from DATAFLOW PIPO Buffers" pattern with BIND_STORAGE impl=lutram technique and trade-off guidance.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-optimization.md_20260322_*.bak`
- **Rollback**: Remove section "### 7. Excessive BRAM from DATAFLOW PIPO Buffers" from `.claude/skills/hls-optimization.md`

## [UPG-0004] Add DSP48 native width operand sizing pattern — 2026-03-22
- **Status**: Applied
- **Trigger**: During `fft` optimization (v1.3), narrowing acc_t from 45 to 25 bits reduced DSP from 80 to 30 by fitting each multiply in a single DSP48E1 (25×18 native).
- **Target**: `.claude/skills/hls-optimization.md`
- **Category**: Additive skill
- **Priority**: HIGH
- **Session**: fft / optimize-ip
- **Summary**: Added "### 8. DSP Chaining from Operands Exceeding DSP48 Native Width" with operand sizing rules and device-specific DSP width table (7-series, UltraScale, Versal).
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-optimization.md_20260322_*.bak`
- **Rollback**: Remove section "### 8. DSP Chaining from Operands Exceeding DSP48 Native Width" from `.claude/skills/hls-optimization.md`

## [UPG-0005] Add fabric multiply latency pitfall — 2026-03-22
- **Status**: Applied
- **Trigger**: During `fft` optimization (v1.2 attempt), `BIND_OP impl=fabric latency=3` failed to pipeline a 60-bit multiply — 13.3 ns combo delay caused timing violation and 133% LUT utilization.
- **Target**: `.claude/rules/hls-coding.md`
- **Category**: Rule refinement
- **Priority**: HIGH
- **Session**: fft / optimize-ip
- **Summary**: Added fabric multiply latency pitfall to "### Common Pitfalls" — BIND_OP impl=fabric latency=N only adds output registers, does not pipeline internal logic.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-coding.md_20260322_*.bak`
- **Rollback**: Remove the "BIND_OP impl=fabric latency=N" bullet from "### Common Pitfalls" in `.claude/rules/hls-coding.md`

## [UPG-0006] Add ap_axiu internal stream restriction pitfall — 2026-03-22
- **Status**: Applied
- **Trigger**: During `window` design, C-synthesis failed with HLS 214-208 because internal DATAFLOW streams used `hls::stream<ap_axiu<32,0,0,0>>`. Vitis HLS 2025.1 requires `ap_axiu` types only on top-level AXI-Stream ports.
- **Target**: `.claude/rules/hls-coding.md`
- **Category**: Rule refinement
- **Priority**: HIGH
- **Session**: window / design-ip
- **Summary**: Added `ap_axiu` / `hls::axis` internal stream restriction to "### Common Pitfalls" with BAD/GOOD examples and the rule to use plain types for inter-stage communication.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-coding.md_20260322_window.bak`
- **Rollback**: Remove the "`ap_axiu` / `hls::axis` types are restricted to top-level AXI-Stream ports" bullet from "### Common Pitfalls" in `.claude/rules/hls-coding.md`

## [UPG-0007] Add resource estimation guidance with interface overhead reference — 2026-03-23
- **Status**: Applied
- **Trigger**: During `nco` design, instruction estimated <200 LUT / <100 FF (core only) but synthesis reported 944 LUT / 712 FF total. The 4-5x discrepancy was entirely from AXI-Lite interface overhead (~234 LUT, ~157 FF). Without guidance, specs systematically underestimate resources.
- **Target**: `src/.template/instruction.md`
- **Category**: Template update
- **Priority**: MEDIUM
- **Session**: nco / design-ip (review-results)
- **Summary**: Added "Expected Resource Usage" sub-section to Target Configuration with core vs interface overhead columns and a reference table of typical Vitis HLS interface costs (AXI-Lite, AXI-Stream, AXI Master).
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/instruction.md_20260323_nco.bak`
- **Rollback**: Remove "### Expected Resource Usage (optional)" sub-section (between Target Configuration table and "## 6. Test Scenarios") from `src/.template/instruction.md`

## [UPG-0008] Add vitis-run shell execution command to synth-sim skill — 2026-03-23
- **Status**: Applied
- **Trigger**: During `cfar_detector` design, the synth-sim execution step wasted time trying to source `settings64.sh` and locate `vitis_hls` binary directly. The correct command (`vitis-run --tcl`) was not documented anywhere in the environment.
- **Target**: `.claude/skills/hls-synth-sim.md`
- **Category**: Additive skill
- **Priority**: HIGH
- **Session**: cfar_detector / design-ip
- **Summary**: Added "### Shell Execution" section to TCL Command Reference with `vitis-run --tcl` examples for csim, csynth, cosim, and a note to not use `vitis_hls` directly.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-synth-sim.md_20260323_173739.bak`
- **Rollback**: Remove "### Shell Execution" section (between "### Execution" and "### Solution Comparison") from `.claude/skills/hls-synth-sim.md`
