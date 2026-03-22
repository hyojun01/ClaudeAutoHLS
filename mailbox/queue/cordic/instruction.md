# IP Instruction: CORDIC

## 1. Functional Description
A CORDIC (COordinate Rotation DIgital Computer) engine that computes trigonometric and vector operations using iterative shift-and-add, requiring zero multipliers. The IP supports two modes:

- **Rotation mode**: Given an angle theta, compute sin(theta) and cos(theta). The input (x0, y0) vector is rotated by theta; for unit-magnitude output, initialize x0 = 1/K (CORDIC gain compensation), y0 = 0.
- **Vectoring mode**: Given an (x, y) vector, compute its magnitude and phase angle. The algorithm drives y toward zero, accumulating the total rotation angle.

The mode is selected per-sample via a sideband signal on the input stream, allowing dynamic switching without reconfiguration.

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| in_stream | IN | axis_in_t (struct) | 48 + sideband | AXI-Stream (axis) | Input: x_in[16], y_in[16], phase_in[16], mode[1], last[1] |
| out_stream | OUT | axis_out_t (struct) | 48 + sideband | AXI-Stream (axis) | Output: x_out[16], y_out[16], phase_out[16], last[1] |

### Input interpretation by mode
- **Rotation mode (mode=0)**: x_in = initial X (set to 1/K for sin/cos), y_in = initial Y (set to 0), phase_in = target angle
- **Vectoring mode (mode=1)**: x_in = I component, y_in = Q component, phase_in = initial angle (typically 0)

### Output interpretation by mode
- **Rotation mode**: x_out = cos(theta) * K_compensated, y_out = sin(theta) * K_compensated, phase_out = residual (should be ~0)
- **Vectoring mode**: x_out = magnitude, y_out = residual (should be ~0), phase_out = accumulated angle (atan2(y,x))

## 3. Data Types

```
// Fixed-point format for data path
typedef ap_fixed<16, 2> data_t;      // Range [-2, 2), 14 fractional bits

// Internal accumulator (extra guard bits for iterative accumulation)
typedef ap_fixed<32, 2> acc_t;       // 30 fractional bits for internal precision

// Phase angle type (same as data, range covers -pi to +pi)
typedef ap_fixed<16, 2> phase_t;

// Number of CORDIC iterations (determines precision: ~1 bit per iteration)
const int NUM_ITERATIONS = 14;       // 14 iterations for ~14-bit precision
```

## 4. Algorithm / Processing

### CORDIC Iteration
For each iteration i (0 to NUM_ITERATIONS-1):

```
if (rotation_mode):
    sigma_i = (z_i >= 0) ? +1 : -1       // drive z toward 0
else (vectoring_mode):
    sigma_i = (y_i < 0) ? +1 : -1        // drive y toward 0

x_{i+1} = x_i - sigma_i * y_i * 2^(-i)
y_{i+1} = y_i + sigma_i * x_i * 2^(-i)
z_{i+1} = z_i - sigma_i * atan(2^(-i))
```

### CORDIC Gain
The iterative rotation introduces a gain factor K = prod(sqrt(1 + 2^(-2i))) for i = 0..N-1. For 14 iterations, K ~ 1.64676. Pre-compensation: divide input x0 by K (i.e., multiply by 1/K ~ 0.60725) before iteration.

### Arctangent Lookup Table
Store atan(2^(-i)) for i = 0..NUM_ITERATIONS-1 as a const array of phase_t values:
- atan(2^0) = 0.7854 (45 deg)
- atan(2^-1) = 0.4636 (26.57 deg)
- ... down to atan(2^-13)

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target Latency | NUM_ITERATIONS + 2 cycles (pipeline fill + drain) |
| Target II | 1 (one input per clock after pipeline is full) |
| Target Throughput | 100 MSamples/s (one result per clock at 100 MHz) |

## 6. Test Scenarios

### Test 1: Rotation mode — Known angles
- Input angles: 0, pi/6, pi/4, pi/3, pi/2, -pi/4, -pi/2, pi (boundary)
- Expected: sin/cos values matching math.h within +/-2^(-12) (~0.000244)
- Verify CORDIC gain compensation is correct

### Test 2: Vectoring mode — Known vectors
- Input vectors: (1,0), (0,1), (1,1), (-1,1), (0.5, 0.866)
- Expected magnitude and phase matching sqrt(x^2+y^2) and atan2(y,x)
- Tolerance: magnitude within +/-2^(-12), phase within +/-2^(-12) radians

### Test 3: Full rotation sweep
- Sweep angle from -pi to +pi in 256 steps (rotation mode)
- Compare all sin/cos outputs against math.h reference
- Report max absolute error and RMS error

### Test 4: TLAST propagation
- Send a burst of 10 samples with TLAST on the last sample
- Verify TLAST appears on the correct output sample (latency-aligned)

## 7. Additional Notes
- The atan lookup table should be a `const` array (synthesizes to ROM/LUT)
- CORDIC gain compensation factor 1/K should be pre-computed as a constant
- For rotation mode sin/cos output, the testbench should pre-load x_in = 1/K, y_in = 0, so that the output directly gives sin/cos without external scaling
- Bit-accurate shift operations use `>>` on `ap_fixed` types which preserves sign (arithmetic shift)
- The design should propagate TLAST from input to output with correct latency alignment
