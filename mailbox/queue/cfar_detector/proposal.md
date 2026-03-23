# IP Proposal: cfar_detector

## Domain
Radar

## Rationale
The CFAR detector is a core building block in any radar signal processing chain. It sits downstream of range-Doppler processing (pulse compression, Doppler filter bank) and provides adaptive target detection against a varying noise/clutter background. Without CFAR, a fixed threshold either misses weak targets in quiet regions or produces excessive false alarms in cluttered regions.

This IP implements Cell-Averaging CFAR (CA-CFAR) with runtime-selectable Greatest-Of (GO) and Smallest-Of (SO) modes, covering the three most widely used CFAR variants in a single IP. OS-CFAR (Ordered Statistics) is excluded because the sorting requirement fundamentally changes the architecture (O(N log²N) comparators vs. O(1) running sums) and warrants a separate IP.

The CFAR detector depends on upstream range-Doppler processing that produces power (magnitude-squared) values and feeds downstream target extraction / track initiation logic.

## Specification Summary
- **Algorithm**: Sliding-window CFAR with running-sum noise estimation. Supports CA, GO, and SO modes. Compile-time window geometry (16 reference cells + 2 guard cells per side), runtime-configurable threshold scaling factor and mode.
- **I/O**: AXI-Stream input (32-bit power samples), AXI-Stream output (64-bit: power + detection flag), AXI-Lite control (alpha, mode, sweep length)
- **Data types**: `ap_uint<32>` power input, `ap_uint<48>` internal accumulators, `ap_ufixed<16,8>` threshold scale factor
- **Target**: xc7z020clg400-1 @ 10 ns (100 MHz)
- **Complexity estimate**: Medium

## Optimization Strategy (if applicable)
- **Phase 1**: Baseline functional design with PIPELINE II=1 on the main sample loop. Shift register for the sliding window, running sums for noise estimation. Expected to meet II=1 without additional optimization.
- **Phase 2**: If synthesis reveals timing issues on the threshold multiply (48×16 bit), pipeline the multiply path with BIND_OP latency. If shift register size causes routing pressure, evaluate BRAM-based circular buffer alternative.

## Dependencies
- **Requires**: Upstream power computation (magnitude-squared of complex range-Doppler samples). Not an IP dependency — the CFAR input is simply `ap_uint<32>` power values.
- **Required by**: Target extraction / plot extractor (downstream IP that filters detected cells).

## User Decision
<!-- User fills this section -->
- [ ] Approve as-is -> proceed to implementation
- [ ] Approve with modifications -> (describe modifications below)
- [ ] Reject -> (reason)
- [ ] Defer -> (revisit later)

### Modifications (if applicable)
<!-- User writes requested changes here -->
