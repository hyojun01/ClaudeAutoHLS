#ifndef CFAR_DETECTOR_HPP
#define CFAR_DETECTOR_HPP

#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// ============================================================
// Window Geometry (compile-time constants)
// ============================================================
const int REF_CELLS_PER_SIDE   = 16;   // Reference cells per side (power of 2)
const int GUARD_CELLS_PER_SIDE = 2;    // Guard cells per side
const int WINDOW_SIZE = 2 * REF_CELLS_PER_SIDE + 2 * GUARD_CELLS_PER_SIDE + 1;  // 37
const int CUT_INDEX   = REF_CELLS_PER_SIDE + GUARD_CELLS_PER_SIDE;               // 18
const int EDGE_CELLS  = REF_CELLS_PER_SIDE + GUARD_CELLS_PER_SIDE;               // 18

// Division via bit-shift (REF_CELLS_PER_SIDE must be power of 2)
const int LOG2_REF_PER_SIDE = 4;    // log2(16)
const int LOG2_TOTAL_REF    = 5;    // log2(32)
const int ALPHA_FRAC_BITS   = 8;    // Fractional bits in alpha encoding (ufixed<16,8>)

// ============================================================
// Data Types
// ============================================================
typedef ap_uint<32>  power_t;        // Input power (magnitude-squared), unsigned
typedef ap_uint<48>  sum_t;          // Running sum accumulator (32 + log2(32) + margin)
typedef ap_uint<16>  alpha_raw_t;    // Threshold scaling factor, raw bit encoding
typedef ap_uint<64>  detect_out_t;   // Output word: {reserved, detect, cut_power}

// CFAR mode encoding
const ap_uint<2> MODE_CA = 0;
const ap_uint<2> MODE_GO = 1;
const ap_uint<2> MODE_SO = 2;

// ============================================================
// Top Function Prototype
// ============================================================
void cfar_detector(
    hls::stream<ap_axiu<32,0,0,0>>& power_in,
    hls::stream<ap_axiu<64,0,0,0>>& detect_out,
    ap_uint<16> alpha,
    ap_uint<2>  cfar_mode,
    ap_uint<16> num_range_cells
);

#endif // CFAR_DETECTOR_HPP
