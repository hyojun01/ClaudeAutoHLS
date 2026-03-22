#ifndef FIR_HPP
#define FIR_HPP

#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// ============================================================
// Constants
// ============================================================
const int NUM_TAPS = 45;
const int NUM_UNIQUE_COEFFS = 23; // (NUM_TAPS + 1) / 2, exploiting symmetry

// ============================================================
// Type Definitions
// ============================================================
typedef ap_fixed<16, 1> data_t;     // Input/output data: Q1.15 signed fixed-point
typedef ap_fixed<16, 1> coeff_t;    // Filter coefficient: Q1.15 signed fixed-point
typedef ap_fixed<45, 15> acc_t;     // Accumulator: wide enough to prevent overflow

// AXI-Stream word type (16-bit data with TLAST)
typedef ap_axiu<16, 0, 0, 0> axis_t;

// ============================================================
// Top Function Prototype
// ============================================================
void fir(hls::stream<axis_t>& in, hls::stream<axis_t>& out);

#endif // FIR_HPP
