# FPGA Application Domain Catalog

## Domain 1: Digital Signal Processing (DSP)

### Basic Building Blocks
| IP | Description | Complexity | Key Parameters |
|----|-------------|-----------|----------------|
| FIR filter | Finite Impulse Response | Low | Taps, symmetry, data width |
| IIR filter | Infinite Impulse Response (SOS) | Medium | Sections, stability |
| FFT/IFFT | Fast Fourier Transform | Medium | Size (64-4096), radix |
| CORDIC | Coordinate Rotation | Medium | Iterations, mode (rotation/vectoring) |
| NCO | Numerically Controlled Oscillator | Low | Phase accumulator width, LUT size |
| CIC | Cascaded Integrator-Comb | Low | Decimation/interpolation ratio, stages |
| Resampler | Polyphase sample rate converter | High | Ratio, filter length |
| Hilbert Transform | Analytic signal generator | Low | FIR-based, taps |

### Data Type Guidelines (DSP)
- Audio: ap_fixed<24,1> or ap_fixed<16,1>
- Baseband I/Q: ap_fixed<16,1> (per component)
- Coefficients: match data width or narrower
- Accumulators: data_width + coeff_width + ceil(log2(taps))

## Domain 2: Communications

### Building Blocks
| IP | Description | Complexity | Key Parameters |
|----|-------------|-----------|----------------|
| QAM Modulator | Constellation mapping | Low | Order (4/16/64/256), gray coding |
| QAM Demodulator | Soft/hard decision | Medium | Order, soft-output width |
| OFDM Modulator | IFFT + CP insertion | Medium | FFT size, CP length |
| OFDM Demodulator | CP removal + FFT | Medium | FFT size, CP length |
| Convolutional Encoder | FEC encoding | Low | Rate (1/2, 1/3), constraint length |
| Viterbi Decoder | Convolutional FEC decoding | High | Constraint length, traceback depth |
| Turbo Decoder | Iterative MAP decoding | Very High | Block size, iterations |
| LDPC Encoder/Decoder | Modern FEC | Very High | Code rate, block length |
| Scrambler/Descrambler | LFSR-based | Low | Polynomial, seed |
| CRC Generator/Checker | Error detection | Low | Polynomial (CRC-8/16/32) |
| Symbol Sync | Timing recovery (Mueller-Muller) | High | Samples/symbol, loop BW |
| Carrier Recovery | Frequency/phase sync | High | PLL type, loop BW |

### Data Type Guidelines (Communications)
- Baseband I/Q: ap_fixed<16,1> complex
- Soft bits (LLR): ap_fixed<8,4> to ap_fixed<12,6>
- Phase accumulator: ap_fixed<32,1> or ap_ufixed<32,0>

## Domain 3: Radar Signal Processing

### Building Blocks
| IP | Description | Complexity | Key Parameters |
|----|-------------|-----------|----------------|
| Pulse Compressor | Matched filter via FFT | Medium | Pulse length, FFT size |
| MTI Filter | Moving Target Indicator | Low | Canceller order (1,2,3) |
| CFAR Detector | Constant False Alarm Rate | Medium | Window size, guard cells, type (CA/OS/GO) |
| Doppler Filter Bank | Range-Doppler processing | Medium | PRIs per CPI, FFT size |
| Beamformer | Phased array steering | High | Elements, channels, weights |
| Digital Downconverter | DDC with NCO + CIC + FIR | Medium | Decimation ratio |

### Data Type Guidelines (Radar)
- ADC samples: ap_fixed<14,14> to ap_fixed<16,16> (raw integer-like)
- After DDC: ap_fixed<16,1> complex
- Detection threshold: ap_ufixed<16,0> or wider

## Domain 4: Image Processing

### Building Blocks
| IP | Description | Complexity | Key Parameters |
|----|-------------|-----------|----------------|
| 2D Convolution | Generic kernel convolution | Medium | Kernel size (3x3, 5x5), line buffer |
| Gaussian Blur | Separable Gaussian filter | Low | Kernel size, sigma |
| Sobel Edge Detect | Gradient computation | Low | 3x3 kernels (Gx, Gy) |
| Median Filter | Non-linear noise removal | Medium | Window size (3x3, 5x5) |
| Histogram EQ | Contrast enhancement | Medium | Bins, streaming or frame |
| Color Convert | RGB<->YCbCr, RGB<->Grayscale | Low | Pipeline or streaming |
| Resize/Scale | Bilinear interpolation | Medium | Scale factor |
| Erosion/Dilation | Morphological operations | Low | Kernel shape and size |

### Data Type Guidelines (Image)
- Pixel channel: ap_uint<8> (or ap_uint<10/12> for raw sensor)
- Packed pixel: ap_uint<24> (RGB) or ap_uint<32> (RGBA)
- Gradient/intermediate: ap_int<16> or ap_fixed<16,8>

## Domain 5: Deep Learning Accelerator

### Building Blocks
| IP | Description | Complexity | Key Parameters |
|----|-------------|-----------|----------------|
| MAC Array | Systolic multiply-accumulate | Medium | Array size, data type (INT8/FP16) |
| Activation (ReLU) | Max(0, x) element-wise | Low | Data width, leaky alpha |
| Max Pooling | Spatial downsampling | Low | Pool size (2x2), stride |
| Batch Norm | Normalization with learned params | Medium | Channels, precision |
| Depthwise Conv | Channel-independent convolution | Medium | Kernel, channels |
| Softmax | Probability distribution | High | Fixed-point exp/div |
| Quantizer | Float->INT8 conversion | Low | Scale, zero point |

### Data Type Guidelines (DL)
- Weights/activations: ap_fixed<8,4> (INT8-equivalent) or ap_fixed<16,8>
- Accumulator: ap_fixed<32,16> minimum for INT8 MAC
- Bias: match accumulator width
