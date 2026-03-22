# IP Proposal: cordic

## Domain
DSP

## Rationale
CORDIC (COordinate Rotation DIgital Computer) is a fundamental DSP building block that computes trigonometric functions (sin, cos), vector magnitude, and phase using only shifts and additions — no multipliers required. This makes it extremely resource-efficient on FPGAs. It serves as a prerequisite for many communication and radar IPs: NCOs use CORDIC for phase-to-sinusoid conversion, carrier recovery loops need CORDIC for phase rotation, and beamformers use it for complex multiplication via rotation. Implementing CORDIC establishes a multiplier-free trig engine that downstream IPs can reuse.

## Specification Summary
- **Algorithm**: Iterative CORDIC in rotation mode (phase → sin/cos) and vectoring mode (I/Q → magnitude/phase), with configurable iteration count for precision vs. latency trade-off
- **I/O**: AXI-Stream input (phase angle or I/Q pair + mode select), AXI-Stream output (sin/cos pair or mag/phase pair), AXI-Lite control register for mode configuration
- **Data types**: `ap_fixed<16,2>` for I/Q and sin/cos values, `ap_fixed<16,2>` for phase angle (range -pi to +pi), `ap_fixed<32,2>` internal accumulator
- **Target**: xc7z020clg400-1 @ 10 ns (100 MHz)
- **Complexity estimate**: Medium

## Optimization Strategy (if applicable)
- **Phase 1**: Baseline functional design — iterative loop with no unrolling, verify correctness against math.h reference
- **Phase 2**: Pipeline the iteration loop to achieve II=1 throughput (one result per clock), trading latency for throughput

## Dependencies
- **Requires**: None
- **Required by**: NCO (phase-to-sinusoid), carrier recovery, digital downconverter, beamformer

## User Decision
<!-- User fills this section -->
- [ ] Approve as-is -> proceed to implementation
- [ ] Approve with modifications -> (describe modifications below)
- [ ] Reject -> (reason)
- [ ] Defer -> (revisit later)

### Modifications (if applicable)
<!-- User writes requested changes here -->
