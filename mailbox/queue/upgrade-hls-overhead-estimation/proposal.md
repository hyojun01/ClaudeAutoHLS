# Upgrade Proposal: HLS Infrastructure Overhead in Resource Estimation

## Category
Additive skill

## Target File
`.claude/skills/fpga-system-design.md` — Section 4: Resource Budget Methodology

## Trigger / Evidence
During the `cfar_detector` design review, actual synthesis results showed systematic underestimation of LUT and FF:

| Resource | Spec Estimate | Actual (Vitis HLS 2025.2) | Ratio |
|----------|---------------|---------------------------|-------|
| LUT | 650 | 1,729 | 2.66× |
| FF | 1,720 | 2,550 | 1.48× |
| DSP | 2 | 3 | 1.50× |

Root cause: the estimation methodology correctly accounts for core datapath and per-interface overhead, but does not account for HLS tool-generated infrastructure — pipeline control muxes, AXI-Lite address decode logic, AXI-Stream sideband registers, and FSM encoding overhead. For small IPs where core logic is <2,000 LUT, this infrastructure dominates.

Additionally, the AXI-Lite overhead figure (~230 LUT) represents only register storage, not the full adapter including address decode, read-back mux, and write arbitration, which typically totals 400–600 LUT.

## Proposed Change

Add a new **Step 6** (renumber existing Step 6 → Step 7) to the Step-by-Step Resource Estimation in Section 4, and update the AXI-Lite overhead figure in the Interface Overhead Budget table.

### Diff

**In the Interface Overhead Budget table (Section 1)**, update the AXI-Lite row:

```
Before:
| AXI-Lite bundle (s_axilite) | ~230 | ~160 | 0 | 0 |

After:
| AXI-Lite bundle (s_axilite) | ~400–600 | ~250–350 | 0 | 0 |
```

Add a note below the table:
```
Note: AXI-Lite overhead scales with the number of registers in the bundle.
The low end (~400 LUT) applies to bundles with 1–4 registers; the high end
(~600 LUT) applies to bundles with 8+ registers. The overhead includes
address decode, read-back mux, write arbitration, and protocol FSM —
not just the register storage.
```

**In Step-by-Step Resource Estimation (Section 4)**, update Step 4 to match the new interface figures, then add Step 6 and renumber:

```
6. Apply HLS infrastructure overhead factor:
   The steps above estimate core datapath + interface adapter logic.
   Vitis HLS adds additional resources for pipeline control, valid/ready
   propagation, multi-cycle operation scheduling, and FSM encoding.
   Apply a multiplier based on core LUT estimate:

   → Core LUT < 2,000:   multiply LUT/FF totals by 2.0–2.5×
   → Core LUT 2,000–10,000: multiply LUT/FF totals by 1.5–2.0×
   → Core LUT > 10,000:  multiply LUT/FF totals by 1.2–1.5×

   DSP and BRAM estimates are not affected (HLS does not add infrastructure DSP/BRAM).

   For pipelined multiplies wider than the DSP48 native width:
     7-series unsigned: DSPs = ceil(A_bits / 24) × ceil(B_bits / 17)
     7-series signed:   DSPs = ceil(A_bits / 25) × ceil(B_bits / 18)
     UltraScale signed: DSPs = ceil(A_bits / 27) × ceil(B_bits / 18)

7. Check against device budget:
   → Flag any resource > 50% of target device capacity
   → Target ≤ 60% overall to leave routing margin
```

## Rationale
- Spec estimates that are 2–3× below actual create a misleading impression of resource budget, even when the design comfortably fits. If a future IP targets a small device or occupies a significant fraction of a mid-size device, the underestimate could lead to incorrect "fits" assessments.
- The DSP cascade formula addresses the cfar_detector's 2→3 DSP discrepancy and will prevent similar mis-estimates for any wide-multiply IP.
- The AXI-Lite overhead update reflects actual HLS adapter complexity rather than just register storage.

## Scope
- One file modified: `.claude/skills/fpga-system-design.md`
- Additive change: existing steps are preserved; one new step + one table update
- ~20 lines of additions
- No impact on existing IP designs (estimation guidance only, not implementation rules)

## Rollback
Revert `.claude/skills/fpga-system-design.md` from backup at `.claude/upgrades/backups/`.

## User Decision
<!-- User fills this section -->
- [ ] Approve as-is
- [ ] Approve with modifications
- [ ] Reject
- [ ] Defer
