#ifndef FFT_HPP
#define FFT_HPP

#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// ============================================================
// Constants
// ============================================================
const int FFT_SIZE = 256;
const int FFT_STAGES = 8;          // log2(256)
const int HALF_FFT_SIZE = FFT_SIZE / 2;

// ============================================================
// Type Definitions
// ============================================================

// Real and imaginary data type (as specified in instruction)
typedef ap_fixed<16, 1> data_t;

// Twiddle factor type — same precision as data, values in [-1, 1)
typedef ap_fixed<16, 1> tw_t;

// Accumulated type for butterfly computation — prevents overflow across 8 stages
typedef ap_fixed<45, 15> acc_t;

// Internal complex representation using accumulated precision
struct complex_t {
    acc_t real;
    acc_t imag;
};

// AXI-Stream packet type (32-bit data with TLAST)
typedef ap_axiu<32, 0, 0, 0> axis_pkt;

// ============================================================
// Top Function Prototype
// ============================================================
void fft(
    hls::stream<axis_pkt>& in,
    hls::stream<axis_pkt>& out
);

#endif // FFT_HPP
