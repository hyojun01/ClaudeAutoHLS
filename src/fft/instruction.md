# IP Instruction Template: <IP fft>

## 1. Functional Description
<!-- Describe what this IP does in plain language -->
256-point Radix-2 DIT FFT

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| in | IN | ap_uint<32> | 32 | AXI-Stream (axis) | Input data stream |
| out | OUT | ap_uint<32> | 32 | AXI-Stream (axis) | Output data stream |

## 3. Data Types
<!-- Define custom types, fixed-point formats, struct definitions -->
Input and Output Data is 32-bit unsigned int which is concated with 16-bit signed fixed-point real and 16-bit signed fixed-point imag.
Real and image Data Type: ap_fixed<16,1>
Twiddle Factor Data Type: "choose proper data type"
Accumulated Data Type: ap_fixed<45,15>, This is the data type used to prevent overflow during the computation process.

## 4. Algorithm / Processing
<!-- Describe the algorithm, math, or processing steps -->
FFT Size is 256.
IP has control interface using AXI-Lite for start, stop, etc.
256 data which last data has tlast 1 signal are streamed into the block and then 256 data which last data has tlast 1 signal are streamed out of the block.
The flow of IP is input streaming data -> bit reverse -> each fft stage -> output streaming data.
IP use DATAFLOW optimization.
The twiddle factor is stored internal memory.

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
Simulate a sine wave composed of three different frequency tones and provide it as the input to the IP.
Compute the magnitude using the real and imaginary parts of the IP output, and verify whether the frequencies corresponding to the tones are present.

## 7. Additional Notes
<!-- Any other constraints, requirements, or preferences -->
Compute the real and imaginary parts of the twiddle factors directly and store them in internal memory, using a tool such as Python.