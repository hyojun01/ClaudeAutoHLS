#include <iostream>
#include <cmath>
#include <cstdlib>
#include "../src/fft.hpp"

// ============================================================
// Test Utilities
// ============================================================
int error_count = 0;

// Pack real and imag into AXI-Stream packet
axis_pkt make_pkt(data_t real_val, data_t imag_val, bool last) {
    axis_pkt pkt;
    ap_uint<32> packed;
    packed.range(31, 16) = real_val.range();
    packed.range(15, 0)  = imag_val.range();
    pkt.data = packed;
    pkt.keep = 0xF;
    pkt.strb = 0xF;
    pkt.last = last ? 1 : 0;
    return pkt;
}

// Unpack AXI-Stream packet to real and imag
void unpack_pkt(axis_pkt pkt, data_t& real_val, data_t& imag_val) {
    real_val.range() = pkt.data.range(31, 16);
    imag_val.range() = pkt.data.range(15, 0);
}

// ============================================================
// Test 1: All-zeros input → all-zeros output
// ============================================================
void test_zeros() {
    std::cout << "--- Test 1: All-zeros input ---" << std::endl;

    hls::stream<axis_pkt> in_stream("in_zeros");
    hls::stream<axis_pkt> out_stream("out_zeros");

    // Feed 256 zero samples
    for (int i = 0; i < FFT_SIZE; i++) {
        in_stream.write(make_pkt((data_t)0.0, (data_t)0.0, i == FFT_SIZE - 1));
    }

    // Run FFT
    fft(in_stream, out_stream);

    // Verify all outputs are zero (within tolerance)
    bool all_zero = true;
    bool tlast_ok = true;
    for (int k = 0; k < FFT_SIZE; k++) {
        axis_pkt pkt = out_stream.read();
        data_t re, im;
        unpack_pkt(pkt, re, im);
        double mag = sqrt((double)re * (double)re + (double)im * (double)im);
        if (mag > 0.001) {
            if (all_zero) {
                std::cerr << "  First non-zero at bin " << k << ": mag=" << mag << std::endl;
            }
            all_zero = false;
        }
        // Verify TLAST signaling
        if (k == FFT_SIZE - 1 && pkt.last != 1) {
            std::cerr << "[FAIL] TLAST not asserted on last output sample" << std::endl;
            tlast_ok = false;
            error_count++;
        }
        if (k < FFT_SIZE - 1 && pkt.last != 0) {
            std::cerr << "[FAIL] TLAST asserted early on sample " << k << std::endl;
            tlast_ok = false;
            error_count++;
        }
    }

    if (all_zero) {
        std::cout << "[PASS] All-zeros input produces all-zeros output" << std::endl;
    } else {
        std::cerr << "[FAIL] All-zeros input produced non-zero output" << std::endl;
        error_count++;
    }
    if (tlast_ok) {
        std::cout << "[PASS] TLAST signaling correct" << std::endl;
    }
}

// ============================================================
// Test 2: Three-tone sine wave — verify frequency peaks
//   Tones at exact FFT bins to avoid spectral leakage:
//     Bin 10 → f = 10 * 100MHz / 256 = 3.90625 MHz
//     Bin 32 → f = 32 * 100MHz / 256 = 12.5 MHz
//     Bin 64 → f = 64 * 100MHz / 256 = 25.0 MHz
//   Amplitudes kept small (0.003) so FFT output fits in ap_fixed<16,1>
// ============================================================
void test_three_tones() {
    std::cout << "--- Test 2: Three-tone sine wave ---" << std::endl;

    hls::stream<axis_pkt> in_stream("in_tones");
    hls::stream<axis_pkt> out_stream("out_tones");

    // Tone parameters
    const int tone_bins[] = {10, 32, 64};
    const double amplitudes[] = {0.003, 0.003, 0.003};
    const int num_tones = 3;

    // Generate composite sine wave and feed to input stream
    std::cout << "  Generating 3-tone input signal..." << std::endl;
    for (int n = 0; n < FFT_SIZE; n++) {
        double x = 0.0;
        for (int t = 0; t < num_tones; t++) {
            x += amplitudes[t] * sin(2.0 * M_PI * tone_bins[t] * n / FFT_SIZE);
        }
        data_t real_val = (data_t)x;
        data_t imag_val = (data_t)0.0;
        in_stream.write(make_pkt(real_val, imag_val, n == FFT_SIZE - 1));
    }

    // Run FFT
    fft(in_stream, out_stream);

    // Read output and compute magnitudes
    double mag[FFT_SIZE];
    for (int k = 0; k < FFT_SIZE; k++) {
        axis_pkt pkt = out_stream.read();
        data_t re, im;
        unpack_pkt(pkt, re, im);
        double re_d = (double)re;
        double im_d = (double)im;
        mag[k] = sqrt(re_d * re_d + im_d * im_d);
    }

    // Compute noise floor (average magnitude of non-tone bins in first half, excluding DC)
    double noise_sum = 0.0;
    int noise_count = 0;
    for (int k = 1; k < HALF_FFT_SIZE; k++) {
        bool is_tone = false;
        for (int t = 0; t < num_tones; t++) {
            if (k == tone_bins[t]) is_tone = true;
        }
        if (!is_tone) {
            noise_sum += mag[k];
            noise_count++;
        }
    }
    double noise_floor = (noise_count > 0) ? noise_sum / noise_count : 0.0;

    // Verify tone bins have significantly higher magnitude than noise floor
    const double snr_threshold = 10.0;
    for (int t = 0; t < num_tones; t++) {
        int bin = tone_bins[t];
        double expected_mag = amplitudes[t] * HALF_FFT_SIZE;
        double ratio = (noise_floor > 0) ? mag[bin] / noise_floor : mag[bin];

        std::cout << "  Bin " << bin << ": magnitude=" << mag[bin]
                  << " (expected~" << expected_mag << ")"
                  << " noise_floor=" << noise_floor
                  << " ratio=" << ratio << std::endl;

        if (mag[bin] > noise_floor * snr_threshold && mag[bin] > 0.01) {
            std::cout << "[PASS] Tone at bin " << bin
                      << " detected (SNR ratio=" << ratio << ")" << std::endl;
        } else {
            std::cerr << "[FAIL] Tone at bin " << bin
                      << " NOT detected (SNR ratio=" << ratio << ")" << std::endl;
            error_count++;
        }
    }

    // Print relevant frequency bins for inspection
    std::cout << "  Frequency spectrum (selected bins):" << std::endl;
    for (int k = 0; k < HALF_FFT_SIZE; k++) {
        bool near_tone = false;
        for (int t = 0; t < num_tones; t++) {
            if (abs(k - tone_bins[t]) <= 2) near_tone = true;
        }
        if (near_tone || k == 0 || mag[k] > noise_floor * 3.0) {
            std::cout << "    Bin " << k << ": " << mag[k] << std::endl;
        }
    }
}

// ============================================================
// Test 3: DC-only input — verify peak at bin 0
// ============================================================
void test_dc() {
    std::cout << "--- Test 3: DC-only input ---" << std::endl;

    hls::stream<axis_pkt> in_stream("in_dc");
    hls::stream<axis_pkt> out_stream("out_dc");

    // Feed constant value (DC = 0.003) for all 256 samples
    data_t dc_val = (data_t)0.003;
    for (int i = 0; i < FFT_SIZE; i++) {
        in_stream.write(make_pkt(dc_val, (data_t)0.0, i == FFT_SIZE - 1));
    }

    // Run FFT
    fft(in_stream, out_stream);

    // Read output and find peak bin
    double mag[FFT_SIZE];
    int peak_bin = 0;
    double peak_mag = 0.0;
    for (int k = 0; k < FFT_SIZE; k++) {
        axis_pkt pkt = out_stream.read();
        data_t re, im;
        unpack_pkt(pkt, re, im);
        double re_d = (double)re;
        double im_d = (double)im;
        mag[k] = sqrt(re_d * re_d + im_d * im_d);
        if (mag[k] > peak_mag) {
            peak_mag = mag[k];
            peak_bin = k;
        }
    }

    std::cout << "  Peak at bin " << peak_bin << " with magnitude " << peak_mag << std::endl;
    std::cout << "  Expected: bin 0, magnitude ~ " << 0.003 * FFT_SIZE << std::endl;

    if (peak_bin == 0 && peak_mag > 0.1) {
        std::cout << "[PASS] DC input produces peak at bin 0" << std::endl;
    } else {
        std::cerr << "[FAIL] DC input peak at bin " << peak_bin
                  << " (expected bin 0)" << std::endl;
        error_count++;
    }

    // Verify non-DC bins are near zero
    double max_non_dc = 0.0;
    for (int k = 1; k < FFT_SIZE; k++) {
        if (mag[k] > max_non_dc) max_non_dc = mag[k];
    }
    if (max_non_dc < peak_mag * 0.01) {
        std::cout << "[PASS] Non-DC bins are negligible (max=" << max_non_dc << ")" << std::endl;
    } else {
        std::cerr << "[FAIL] Non-DC bins too large (max=" << max_non_dc << ")" << std::endl;
        error_count++;
    }
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testbench: FFT (256-point Radix-2 DIT)" << std::endl;
    std::cout << "========================================" << std::endl;

    test_zeros();
    test_three_tones();
    test_dc();

    std::cout << "========================================" << std::endl;
    if (error_count == 0) {
        std::cout << "  ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "  " << error_count << " TEST(S) FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return error_count;
}
