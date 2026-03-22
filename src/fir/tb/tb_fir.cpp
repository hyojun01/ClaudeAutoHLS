#include <iostream>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include "../src/fir.hpp"

// ============================================================
// Constants
// ============================================================
const int NUM_SAMPLES = 256;
const double PI = 3.14159265358979323846;
const double FS = 100.0e6;   // 100 MHz sampling rate
const double F1 = 5.0e6;     // 5 MHz tone (passband)
const double F2 = 15.0e6;    // 15 MHz tone (stopband)
const double AMP = 0.3;      // Amplitude per tone (sum fits in [-1,1))

// Tolerance for sample comparison (2 LSBs of ap_fixed<16,1>)
const double TOLERANCE = 2.0 / 32768.0;

// ============================================================
// Golden Reference Coefficients (quantized to coeff_t)
// ============================================================
static const coeff_t golden_unique[NUM_UNIQUE_COEFFS] = {
    -0.00875854,  0.04141235,  0.01757812,  0.00823975,
    -0.00152588, -0.01113892, -0.01669312, -0.01473999,
    -0.00497437,  0.00930786,  0.02175903,  0.02578735,
     0.01751709, -0.00210571, -0.02609253, -0.04367065,
    -0.04370117, -0.01934814,  0.02899170,  0.09240723,
     0.15591431,  0.20275879,  0.22000122
};

// ============================================================
// Test: Two-Tone Sine Wave (5 MHz + 15 MHz @ 100 MHz)
// Verifies functional correctness, TLAST propagation,
// and frequency selectivity of the FIR filter.
// ============================================================
int test_two_tone() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Test: Two-Tone Sine Wave" << std::endl;
    std::cout << "  Input : " << AMP << "*sin(2pi*5MHz*t) + "
              << AMP << "*sin(2pi*15MHz*t)" << std::endl;
    std::cout << "  Expect: 5 MHz passes, 15 MHz attenuated" << std::endl;
    std::cout << "========================================" << std::endl;

    int errors = 0;

    // Build full 45-tap coefficient array from the 23 unique (quantized) values
    double full_coeffs[NUM_TAPS];
    for (int i = 0; i < NUM_UNIQUE_COEFFS; i++)
        full_coeffs[i] = (double)golden_unique[i];
    for (int i = 0; i < NUM_UNIQUE_COEFFS - 1; i++)
        full_coeffs[NUM_TAPS - 1 - i] = (double)golden_unique[i];

    // Generate input samples
    hls::stream<axis_t> in_stream("in_stream");
    hls::stream<axis_t> out_stream("out_stream");
    data_t input_samples[NUM_SAMPLES];

    for (int n = 0; n < NUM_SAMPLES; n++) {
        double val = AMP * sin(2.0 * PI * F1 * n / FS)
                   + AMP * sin(2.0 * PI * F2 * n / FS);
        data_t fixed_val = (data_t)val;
        input_samples[n] = fixed_val;

        axis_t word;
        word.data = 0;
        word.data.range() = fixed_val.range();
        word.keep = -1;
        word.strb = -1;
        word.last = (n == NUM_SAMPLES - 1) ? 1 : 0;
        in_stream.write(word);
    }

    // Run the HLS FIR filter
    fir(in_stream, out_stream);

    // ----------------------------------------------------------
    // Compare HLS output with double-precision golden reference
    // ----------------------------------------------------------
    data_t hls_outputs[NUM_SAMPLES];
    double golden_shift[NUM_TAPS] = {0};

    for (int n = 0; n < NUM_SAMPLES; n++) {
        // Golden reference: shift and convolve at double precision
        for (int i = NUM_TAPS - 1; i > 0; i--)
            golden_shift[i] = golden_shift[i - 1];
        golden_shift[0] = (double)input_samples[n];

        double ref_acc = 0.0;
        for (int k = 0; k < NUM_TAPS; k++)
            ref_acc += full_coeffs[k] * golden_shift[k];
        data_t ref_output = (data_t)ref_acc;

        // Read HLS output
        axis_t out_word = out_stream.read();
        data_t hls_output;
        hls_output.range() = out_word.data.range();
        hls_outputs[n] = hls_output;

        // Sample-by-sample comparison
        double diff = fabs((double)hls_output - (double)ref_output);
        if (diff > TOLERANCE) {
            if (errors < 20) {
                std::cout << "[FAIL] Sample " << std::setw(3) << n
                          << ": HLS=" << std::fixed << std::setprecision(6)
                          << (double)hls_output
                          << "  Ref=" << (double)ref_output
                          << "  Diff=" << std::scientific << diff
                          << std::endl;
            }
            errors++;
        }

        // TLAST check on last sample
        if (n == NUM_SAMPLES - 1) {
            if (out_word.last != 1) {
                std::cout << "[FAIL] TLAST not asserted on last sample"
                          << std::endl;
                errors++;
            } else {
                std::cout << "[PASS] TLAST correctly propagated on last sample"
                          << std::endl;
            }
        }
    }

    if (errors == 0) {
        std::cout << "[PASS] All " << NUM_SAMPLES
                  << " samples match golden reference (tol="
                  << TOLERANCE << ")" << std::endl;
    } else {
        std::cout << "[FAIL] " << errors
                  << " sample(s) exceeded tolerance" << std::endl;
    }

    // ----------------------------------------------------------
    // Frequency-domain analysis (DFT at 5 MHz and 15 MHz)
    // Skip first NUM_TAPS samples (filter warmup)
    // ----------------------------------------------------------
    int warmup = NUM_TAPS;
    double in_5r = 0, in_5i = 0, in_15r = 0, in_15i = 0;
    double out_5r = 0, out_5i = 0, out_15r = 0, out_15i = 0;

    for (int n = warmup; n < NUM_SAMPLES; n++) {
        double ph5  = 2.0 * PI * F1 * n / FS;
        double ph15 = 2.0 * PI * F2 * n / FS;
        double in_val  = (double)input_samples[n];
        double out_val = (double)hls_outputs[n];

        in_5r  += in_val  * cos(ph5);
        in_5i  += in_val  * sin(ph5);
        in_15r += in_val  * cos(ph15);
        in_15i += in_val  * sin(ph15);

        out_5r  += out_val * cos(ph5);
        out_5i  += out_val * sin(ph5);
        out_15r += out_val * cos(ph15);
        out_15i += out_val * sin(ph15);
    }

    double in_mag5  = sqrt(in_5r  * in_5r  + in_5i  * in_5i);
    double in_mag15 = sqrt(in_15r * in_15r + in_15i * in_15i);
    double out_mag5  = sqrt(out_5r  * out_5r  + out_5i  * out_5i);
    double out_mag15 = sqrt(out_15r * out_15r + out_15i * out_15i);

    double gain_5  = (in_mag5  > 0) ? out_mag5  / in_mag5  : 0;
    double gain_15 = (in_mag15 > 0) ? out_mag15 / in_mag15 : 0;

    std::cout << std::endl;
    std::cout << "---- Frequency Response Analysis ----" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  5 MHz : In=" << in_mag5  << "  Out=" << out_mag5
              << "  Gain=" << gain_5  << std::endl;
    std::cout << " 15 MHz : In=" << in_mag15 << "  Out=" << out_mag15
              << "  Gain=" << gain_15 << std::endl;

    if (gain_5 > 0.5) {
        std::cout << "[PASS] 5 MHz passes through filter (gain="
                  << gain_5 << " > 0.5)" << std::endl;
    } else {
        std::cout << "[FAIL] 5 MHz unexpectedly attenuated (gain="
                  << gain_5 << ")" << std::endl;
        errors++;
    }

    if (gain_15 < 0.1) {
        std::cout << "[PASS] 15 MHz attenuated by filter (gain="
                  << gain_15 << " < 0.1)" << std::endl;
    } else {
        std::cout << "[FAIL] 15 MHz not sufficiently attenuated (gain="
                  << gain_15 << ")" << std::endl;
        errors++;
    }

    return errors;
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testbench: 45-Tap Symmetric FIR Filter" << std::endl;
    std::cout << "========================================" << std::endl;

    int total_errors = 0;
    total_errors += test_two_tone();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    if (total_errors == 0) {
        std::cout << "  ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "  " << total_errors << " TEST(S) FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return total_errors;
}
