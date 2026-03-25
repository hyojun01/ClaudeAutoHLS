# IP Proposal: mac_array

## Domain
Deep Learning

## Rationale
The MAC (Multiply-Accumulate) array is the fundamental compute engine in every DNN accelerator. It performs tiled matrix multiplication — the core operation behind fully-connected layers, 1x1 convolutions, and the im2col-based convolution strategy. This IP complements the existing `depthwise_conv` (channel-independent 3x3) by providing the dense matrix-multiply building block needed for standard and pointwise convolution layers. Together they cover the two dominant compute patterns in MobileNet-style architectures.

## Specification Summary
- **Algorithm**: Output-stationary 4x4 broadcast MAC array performing C[4][4] += A[4][K] x B[K][4] with INT8 inputs and INT32 accumulators. K is runtime-configurable. The host tiles larger matrix multiplications across multiple invocations.
- **I/O**: `act_in` (AXI-Stream, 32b packed 4xINT8), `weight_in` (AXI-Stream, 32b packed 4xINT8), `result_out` (AXI-Stream, 32b INT32), `k_dim` (AXI-Lite)
- **Data types**: `ap_int<8>` activations/weights, `ap_int<32>` accumulator/output
- **Target**: xc7z020clg400-1 @ 10 ns (100 MHz)
- **Complexity estimate**: Medium

## Optimization Strategy (if applicable)
- **Phase 1**: Baseline functional design — inner loops fully unrolled (16 parallel MACs), outer k-loop pipelined at II=1. No additional optimization pragmas beyond interfaces and unroll.
- **Phase 2**: Optimize for throughput — explore DATAFLOW between compute and drain phases, evaluate increasing array to 8x4 if DSP budget allows.

## Dependencies
- **Requires**: None
- **Required by**: Potential future IPs: `conv2d_engine` (im2col + MAC array), `fc_layer` (fully-connected)

## User Decision
<!-- User fills this section -->
- [ ] Approve as-is -> proceed to implementation
- [ ] Approve with modifications -> (describe modifications below)
- [ ] Reject -> (reason)
- [ ] Defer -> (revisit later)

### Modifications (if applicable)
<!-- User writes requested changes here -->
