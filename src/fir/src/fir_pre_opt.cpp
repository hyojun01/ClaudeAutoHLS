#include "fir.hpp"

// ============================================================
// Filter Coefficients (symmetric — only half + center stored)
// Full 45-tap filter: h[k] = coeffs[k] for k=0..22,
//                     h[k] = coeffs[44-k] for k=23..44
// ============================================================
static const coeff_t coeffs[NUM_UNIQUE_COEFFS] = {
    -0.00875854,  0.04141235,  0.01757812,  0.00823975,
    -0.00152588, -0.01113892, -0.01669312, -0.01473999,
    -0.00497437,  0.00930786,  0.02175903,  0.02578735,
     0.01751709, -0.00210571, -0.02609253, -0.04367065,
    -0.04370117, -0.01934814,  0.02899170,  0.09240723,
     0.15591431,  0.20275879,  0.22000122
};

// ============================================================
// Top-Level Function: 45-Tap Symmetric FIR Filter
// ============================================================
void fir(hls::stream<axis_t>& in, hls::stream<axis_t>& out) {
    // AXI-Stream interfaces for input and output data
    #pragma HLS INTERFACE axis port=in
    #pragma HLS INTERFACE axis port=out
    // AXI-Lite control interface for start/stop/done
    #pragma HLS INTERFACE s_axilite port=return

    // Shift register for input samples (persistent across calls)
    static data_t shift_reg[NUM_TAPS] = {};
    #pragma HLS ARRAY_PARTITION variable=shift_reg complete

    // Fully partition coefficients for maximum read bandwidth
    #pragma HLS ARRAY_PARTITION variable=coeffs complete

    bool last = false;

    MAIN_LOOP:
    while (!last) {
        #pragma HLS PIPELINE II=1

        // Read input sample from AXI-Stream
        axis_t in_word = in.read();
        data_t sample;
        sample.range() = in_word.data.range();
        last = in_word.last;

        // Shift register: shift old samples right, insert new at index 0
        SHIFT_LOOP:
        for (int i = NUM_TAPS - 1; i > 0; i--) {
            shift_reg[i] = shift_reg[i - 1];
        }
        shift_reg[0] = sample;

        // Symmetric FIR: pre-add sample pairs that share a coefficient,
        // then multiply by the shared coefficient
        acc_t acc = 0;

        SYM_MAC_LOOP:
        for (int i = 0; i < NUM_UNIQUE_COEFFS - 1; i++) {
            // Natural-width multiply (16×17) maps to 1 DSP48 each
            acc += coeffs[i] * (shift_reg[i] + shift_reg[NUM_TAPS - 1 - i]);
        }
        // Center tap has no symmetric pair
        acc += coeffs[NUM_UNIQUE_COEFFS - 1] * shift_reg[NUM_UNIQUE_COEFFS - 1];

        // Truncate accumulator to output data type
        data_t result = (data_t)acc;

        // Write output with TLAST propagated from input
        axis_t out_word;
        out_word.data = 0;
        out_word.data.range() = result.range();
        out_word.keep = -1; // All bytes valid
        out_word.strb = -1;
        out_word.last = last ? 1 : 0;
        out.write(out_word);
    }
}
