#include "cfar_detector.hpp"

// ============================================================
// Top-Level Function: CA/GO/SO-CFAR Detector
// ============================================================
void cfar_detector(
    hls::stream<ap_axiu<32,0,0,0>>& power_in,
    hls::stream<ap_axiu<64,0,0,0>>& detect_out,
    ap_uint<16> alpha,
    ap_uint<2>  cfar_mode,
    ap_uint<16> num_range_cells
) {
    // --- Interface pragmas ---
    #pragma HLS INTERFACE axis port=power_in
    #pragma HLS INTERFACE axis port=detect_out
    #pragma HLS INTERFACE s_axilite port=alpha bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=cfar_mode bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=num_range_cells bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return bundle=ctrl

    // --- Sliding window shift register (fully registered) ---
    power_t window[WINDOW_SIZE];
    #pragma HLS ARRAY_PARTITION variable=window complete

    // Clear shift register at start of each sweep
    INIT_WINDOW:
    for (int j = 0; j < WINDOW_SIZE; j++) {
        #pragma HLS UNROLL
        window[j] = 0;
    }

    // Running sums for lagging and leading reference cells
    sum_t sum_lagging = 0;
    sum_t sum_leading = 0;

    // Total iterations: CUT_INDEX fill cycles to align output cell 0 with input sample 0,
    // then num_range_cells output cycles (including drain at end)
    int total_iter = (int)num_range_cells + CUT_INDEX;

    // --- Main processing loop (pipelined II=1) ---
    MAIN_LOOP:
    for (int i = 0; i < total_iter; i++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS LOOP_TRIPCOUNT min=55 max=65553

        // (a) Read one power sample, or insert zero during drain phase
        power_t new_sample;
        if (i < (int)num_range_cells) {
            ap_axiu<32,0,0,0> in_word = power_in.read();
            new_sample = (power_t)in_word.data;
        } else {
            new_sample = 0;
        }

        // (b) Save boundary values BEFORE shift (used for running sum update)
        //     lag_exit:   element leaving lagging ref into guard zone
        //     lead_enter: element leaving guard zone into leading ref
        //     win_exit:   element exiting the window entirely
        power_t lag_exit   = window[REF_CELLS_PER_SIDE - 1];
        power_t lead_enter = window[REF_CELLS_PER_SIDE + 2 * GUARD_CELLS_PER_SIDE];
        power_t win_exit   = window[WINDOW_SIZE - 1];

        // (c) Shift register right by 1, insert new sample at index 0
        SHIFT_REG:
        for (int j = WINDOW_SIZE - 1; j > 0; j--) {
            window[j] = window[j - 1];
        }
        window[0] = new_sample;

        // (d) Incrementally update running sums
        sum_lagging = sum_lagging + (sum_t)new_sample - (sum_t)lag_exit;
        sum_leading = sum_leading + (sum_t)lead_enter - (sum_t)win_exit;

        // (e-h) Output phase: produce one result after the fill period
        if (i >= CUT_INDEX) {
            int cell_index = i - CUT_INDEX;

            // (e) Read CUT power from window center
            power_t cut_power = window[CUT_INDEX];

            // (f) Select noise estimate based on CFAR mode
            sum_t noise_sum;
            if (cfar_mode == MODE_GO) {
                // Greatest-Of: use the larger side average (suppresses clutter-edge false alarms)
                noise_sum = (sum_leading > sum_lagging) ? sum_leading : sum_lagging;
            } else if (cfar_mode == MODE_SO) {
                // Smallest-Of: use the smaller side average (better sensitivity at edges)
                noise_sum = (sum_leading < sum_lagging) ? sum_leading : sum_lagging;
            } else {
                // Cell-Averaging (mode 0, or reserved mode 3 treated as CA)
                noise_sum = sum_leading + sum_lagging;
            }

            // Threshold = (noise_sum * alpha_raw) >> shift_amount
            // This computes: (noise_avg) * alpha_float, where
            //   noise_avg = noise_sum / num_ref_cells  (via >> LOG2_REF)
            //   alpha_float = alpha_raw / 256           (via >> ALPHA_FRAC_BITS)
            ap_uint<64> product = (ap_uint<64>)noise_sum * (ap_uint<16>)alpha;
            ap_uint<64> threshold;
            if (cfar_mode == MODE_GO || cfar_mode == MODE_SO) {
                // Single-side: divide by REF_CELLS_PER_SIDE * alpha_scale
                threshold = product >> (LOG2_REF_PER_SIDE + ALPHA_FRAC_BITS);  // >> 12
            } else {
                // Both-sides: divide by 2*REF_CELLS_PER_SIDE * alpha_scale
                threshold = product >> (LOG2_TOTAL_REF + ALPHA_FRAC_BITS);     // >> 13
            }

            // (g) Detection decision with edge suppression
            //     First and last EDGE_CELLS outputs have incomplete reference windows
            bool valid_cell = (cell_index >= EDGE_CELLS) &&
                              (cell_index < (int)num_range_cells - EDGE_CELLS);
            ap_uint<1> detect = (valid_cell && ((ap_uint<64>)cut_power > threshold))
                                ? (ap_uint<1>)1 : (ap_uint<1>)0;

            // (h) Pack output: {reserved[63:33], detect[32], cut_power[31:0]}
            ap_axiu<64,0,0,0> out_word;
            out_word.data = ((ap_uint<64>)detect << 32) | (ap_uint<64>)cut_power;
            out_word.keep = -1;  // All 8 bytes valid
            out_word.strb = -1;
            out_word.last = (cell_index == (int)num_range_cells - 1) ? 1 : 0;

            detect_out.write(out_word);
        }
    }
}
