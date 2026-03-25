# IP Proposal: cic_decimator

## Domain
DSP

## Rationale
The CIC (Cascaded Integrator-Comb) decimation filter is a fundamental building
block in digital receiver signal chains. It efficiently reduces the sample rate
by a large integer factor without requiring any multipliers, making it extremely
resource-efficient on FPGAs. In a typical digital downconversion (DDC) chain, the
CIC decimator sits immediately after the mixer/NCO, performing the bulk of the
sample rate reduction before a compensating FIR filter handles the passband droop
correction and fine-grained filtering.

The CIC decimator complements the existing NCO IP in the repository. Together with
a future compensation FIR, they form a complete DDC subsystem suitable for
software-defined radio, radar digital receivers, and baseband processing
applications.

Key properties that make CIC an excellent FPGA building block:
- Zero multipliers (add/subtract only) -- extremely low resource cost
- Fixed coefficients (no coefficient storage or loading)
- Large decimation ratios (4x to 256x) with trivial control logic
- Streaming architecture with II=1 at the input rate

## Specification Summary
- **Algorithm**: 4-stage CIC decimation filter (N=4, M=1, R=16) with output bit
  pruning. Cascaded integrator stages at the input rate, rate change by R,
  cascaded comb stages at the output rate. All internal arithmetic uses
  full-precision two's complement modular addition/subtraction.
- **I/O**: `din` (AXI-Stream in, 16-bit signed), `dout` (AXI-Stream out, 16-bit
  signed). Free-running with `ap_ctrl_none`.
- **Data types**: Input `ap_int<16>`, internal accumulators `ap_int<32>`, output
  `ap_int<16>` after MSB extraction from the 32-bit full-precision result.
- **Target**: xc7z020clg400-1 @ 10 ns (100 MHz)
- **Complexity estimate**: Low

## Optimization Strategy (if applicable)
- **Phase 1**: Baseline functional design with II=1 at the input rate. All
  integrator and comb stages in a single pipelined loop. Verify correctness
  through impulse response and sinusoidal tests.
- **Phase 2**: If timing is tight due to the 4-stage integrator adder chain,
  consider pipeline registers between integrator stages or a DATAFLOW
  decomposition. Target: meet 10 ns timing with minimal additional latency.

## Dependencies
- **Requires**: None
- **Required by**: Digital Downconverter (DDC), compensation FIR filter
  (downstream), polyphase resampler (alternative to CIC in some chains)

## User Decision
<!-- User fills this section -->
- [ ] Approve as-is -> proceed to implementation
- [ ] Approve with modifications -> (describe modifications below)
- [ ] Reject -> (reason)
- [ ] Defer -> (revisit later)

### Modifications (if applicable)
<!-- User writes requested changes here -->
