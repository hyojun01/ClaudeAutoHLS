# IP Instruction Template: <IP Name>

## 1. Functional Description
<!-- Describe what this IP does in plain language -->
45-taps FIR Filter

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| in | IN | ap_fixed<16,1> | 16 | AXI-Stream (axis) | Input data stream |
| out | OUT | ap_fixed<16,1> | 16 | AXI-Stream (axis) | Output data stream |

## 3. Data Types
<!-- Define custom types, fixed-point formats, struct definitions -->
in & out Data Type: ap_fixed<16,1>
Coefficient Data Type: ap_fixed<16,1>
Accumulated Data Type: ap_fixed<45,15>, This is the data type used to prevent overflow during the computation process.

## 4. Algorithm / Processing
<!-- Describe the algorithm, math, or processing steps -->
An input sample is received at every clock cycle, and the FIR filter operation is performed.
There is a register inside the IP that stores input samples with a length equal to the number of taps.
There is a register inside the IP that stores the filter coefficients. By exploiting the symmetry of the FIR filter coefficients, only half of them are stored. The filter coefficients are specified in the Additional Notes. 
The input sample register and the filter coefficient register are allocated to maximize memory bandwidth, ensuring the fastest possible access speed.
The FIR filter operation algorithm is a convolution operation.
When performing the filter operation, exploit the symmetry of the FIR filter coefficients. 
Propagate the TLAST signal of the input sample to the output sample. If the TLAST signal of the input sample is 1, the IP continues processing up to that input sample and then stops its operation.
IP has control interface using AXI-Lite for start, stop, etc

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target Latency | (optional) |
| Target II | (optional) |
| Target Throughput | (optional) |

## 6. Test Scenarios
<!-- Describe test cases: typical inputs, edge cases, expected outputs -->
The sampling rate is 100 MHz.
Simulate a sine wave with two tones and provide it as the input to the IP.
At this time, the two tones are 5 MHz and 15 MHz, respectively. 
The expected simulation result is that 5 MHz should pass through the filter, while 15 MHz should be attenuated (filtered out).

## 7. Additional Notes
<!-- Any other constraints, requirements, or preferences -->
Filter Coefficient: {
        -0.00875854, 0.04141235, 0.01757812, 0.00823975, -0.00152588, -0.01113892,
        -0.01669312, -0.01473999, -0.00497437, 0.00930786, 0.02175903, 0.02578735,
        0.01751709, -0.00210571, -0.02609253, -0.04367065, -0.04370117, -0.01934814,
        0.0289917, 0.09240723,  0.15591431,  0.20275879,  0.22000122
    }