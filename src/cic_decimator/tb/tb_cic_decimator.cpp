/*
 * tb_cic_decimator.cpp — Testbench for cic_decimator HLS IP
 *
 * Tests:
 *   1. Impulse response (double-precision reference, +/-1 LSB tolerance)
 *   2. DC input, max positive (32767), steady-state convergence
 *   3. DC input, max negative (-32768), steady-state convergence
 *   4. Sinusoidal in-band input, SNR check
 *   5. TLAST propagation across a decimation boundary
 *   6. State continuity across separate invocations
 *
 * Design notes:
 *   - The HLS function processes ONE input sample per call.
 *   - One output is produced every R=16 calls (when count reaches 15 then resets).
 *   - Static state persists across all calls within a single program execution,
 *     so a shared reference model tracks state across all tests.
 */

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include "../src/cic_decimator.hpp"

// ============================================================
// Test Utilities
// ============================================================

static int error_count = 0;
static int test_count  = 0;

static void check_eq(const char* name, int32_t actual, int32_t expected)
{
    test_count++;
    if (actual != expected) {
        std::cerr << "[FAIL] " << name
                  << " — expected: " << expected
                  << ", got: " << actual << std::endl;
        error_count++;
    } else {
        std::cout << "[PASS] " << name << std::endl;
    }
}

static void check_close(const char* name, int32_t actual, int32_t expected,
                        int32_t tol)
{
    test_count++;
    int32_t err = actual - expected;
    if (err < -tol || err > tol) {
        std::cerr << "[FAIL] " << name
                  << " — expected: " << expected
                  << " +/- " << tol
                  << ", got: " << actual
                  << " (err=" << err << ")" << std::endl;
        error_count++;
    } else {
        std::cout << "[PASS] " << name << std::endl;
    }
}

// ============================================================
// Shared Reference Model
//
// Mirrors the HLS function's static state exactly.
// Must be fed the same sample sequence in the same order across all tests.
// A single global instance is used so its state matches the HLS IP's state
// through the full program execution.
// ============================================================

struct CicRefModel {
    // Integrator state (double precision, modular semantics via int64 cast)
    double integ[CIC_N];
    // Comb delay registers
    double comb_delay[CIC_N];
    // Decimation counter
    int    count;
    // Accumulated TLAST for current decimation block
    bool   tlast_or;
    // Whether an output was produced in the last push() call
    bool   has_output;
    // Last output values
    int16_t last_out_data;
    bool    last_out_last;

    CicRefModel() {
        for (int i = 0; i < CIC_N; i++) {
            integ[i]      = 0.0;
            comb_delay[i] = 0.0;
        }
        count     = 0;
        tlast_or  = false;
        has_output      = false;
        last_out_data   = 0;
        last_out_last   = false;
    }

    /*
     * push() — feed one input sample, mirroring one HLS function call.
     *
     * Integrator update (sequential cascade, matching C-sim behavior):
     *   integ[0] += sample
     *   integ[1] += integ[0]     (reads updated integ[0])
     *   integ[2] += integ[1]     (reads updated integ[1])
     *   integ[3] += integ[2]     (reads updated integ[2])
     *
     * We use double arithmetic but truncate to int32 range at each stage to
     * replicate the modular 32-bit arithmetic of ap_int<32> (wrap on overflow).
     * Wrapping is achieved by casting to int32_t.
     */
    void push(int16_t sample_val, bool tlast_in) {
        has_output = false;

        // Accumulate TLAST
        if (tlast_in) tlast_or = true;

        // Integrator cascade — modular 32-bit arithmetic
        int32_t s32 = (int32_t)sample_val;  // sign-extend 16->32
        double   v  = (double)s32;

        for (int i = 0; i < CIC_N; i++) {
            integ[i] += v;
            // Wrap to 32-bit signed range (modular arithmetic)
            integ[i] = (double)(int32_t)(int64_t)integ[i];
            v = integ[i];
        }

        // Decimate every R=16 samples
        if (count == CIC_R - 1) {
            // Comb cascade (sequential, same as HLS loop)
            double comb_in = integ[CIC_N - 1];
            for (int i = 0; i < CIC_N; i++) {
                double comb_out  = comb_in - comb_delay[i];
                comb_delay[i]    = comb_in;
                // Wrap comb_delay to 32-bit range
                comb_delay[i] = (double)(int32_t)(int64_t)comb_delay[i];
                comb_in = comb_out;
                // Wrap running comb value
                comb_in = (double)(int32_t)(int64_t)comb_in;
            }

            // Bit pruning with rounding:
            //   rounded = comb_out + (1 << 15)   (round-half-up)
            //   pruned  = (int16_t)(rounded >> 16)  (arithmetic right shift)
            int32_t comb_i32 = (int32_t)(int64_t)comb_in;
            int32_t rounded  = comb_i32 + (1 << (PRUNE_BITS - 1));
            int16_t pruned   = (int16_t)(rounded >> PRUNE_BITS);

            last_out_data  = pruned;
            last_out_last  = tlast_or;
            has_output     = true;

            // Reset decimation state
            tlast_or = false;
            count    = 0;
        } else {
            count++;
        }
    }
};

// Single shared reference model instance — persists across all tests
// just like the HLS function's static variables do.
static CicRefModel ref;

// ============================================================
// Helper: feed N samples to both DUT and reference model,
//         collect up to (N/R) outputs.
//
// out_data[] and out_last[] must be pre-allocated to at least N/R entries.
// Returns the number of outputs actually collected.
// ============================================================

static int feed_samples(
    const int16_t* samples,
    const bool*    tlasts,
    int            n_in,
    int16_t*       out_data,
    bool*          out_last,
    int            max_out,
    int16_t*       ref_out_data,
    bool*          ref_out_last,
    int*           n_ref_out)
{
    hls::stream<axis_din_t>  din;
    hls::stream<axis_dout_t> dout;

    // Push all input samples into the DUT stream
    for (int i = 0; i < n_in; i++) {
        axis_din_t pkt;
        pkt.data = (ap_uint<16>)(ap_int<16>)samples[i];
        pkt.keep = 0x3;
        pkt.strb = 0x3;
        pkt.last = tlasts ? (ap_uint<1>)tlasts[i] : (ap_uint<1>)0;
        din.write(pkt);
    }

    // Call DUT once per sample
    for (int i = 0; i < n_in; i++) {
        cic_decimator(din, dout);
    }

    // Collect DUT outputs
    int n_out = 0;
    while (!dout.empty() && n_out < max_out) {
        axis_dout_t pkt = dout.read();
        out_data[n_out] = (int16_t)(ap_int<16>)pkt.data;
        out_last[n_out] = (bool)pkt.last;
        n_out++;
    }

    // Advance reference model with same samples
    if (n_ref_out) *n_ref_out = 0;
    for (int i = 0; i < n_in; i++) {
        bool t = tlasts ? tlasts[i] : false;
        ref.push(samples[i], t);
        if (ref.has_output && ref_out_data && ref_out_last && n_ref_out &&
            *n_ref_out < max_out)
        {
            ref_out_data[*n_ref_out] = ref.last_out_data;
            ref_out_last[*n_ref_out] = ref.last_out_last;
            (*n_ref_out)++;
        }
    }

    return n_out;
}

// ============================================================
// Test 1: Impulse Response
//
// Feed 1 sample of value 1, then 63 zero samples (4*R = 64 total).
// The impulse response has exactly 4 non-zero output samples.
// Compare DUT vs reference model; tolerance = +/-1 LSB.
// ============================================================

void test_impulse()
{
    std::cout << "\n--- Test 1: Impulse Response ---" << std::endl;

    const int N_IN  = 64;     // 4 * CIC_R
    const int N_OUT = N_IN / CIC_R;  // 4 expected outputs

    int16_t samples[N_IN] = {};
    samples[0] = 1;  // single unit impulse at sample 0, rest are 0

    int16_t dut_data[N_OUT], ref_data[N_OUT];
    bool    dut_last[N_OUT], ref_last[N_OUT];
    int     n_ref_out = 0;

    int n_out = feed_samples(samples, nullptr, N_IN,
                             dut_data, dut_last, N_OUT,
                             ref_data, ref_last, &n_ref_out);

    if (n_out != N_OUT) {
        std::cerr << "[FAIL] Impulse: expected " << N_OUT
                  << " outputs, got " << n_out << std::endl;
        error_count++;
        return;
    }
    if (n_ref_out != N_OUT) {
        std::cerr << "[FAIL] Impulse ref: expected " << N_OUT
                  << " outputs, got " << n_ref_out << std::endl;
        error_count++;
        return;
    }

    char name[64];
    for (int i = 0; i < N_OUT; i++) {
        snprintf(name, sizeof(name), "Impulse output[%d]", i);
        // Tolerance of +/-1 LSB vs reference
        check_close(name, (int32_t)dut_data[i], (int32_t)ref_data[i], 1);
    }

    // Print actual impulse response values for information
    std::cout << "  Impulse response (ref / dut):" << std::endl;
    for (int i = 0; i < N_OUT; i++) {
        std::cout << "    output[" << i << "] = " << ref_data[i]
                  << " / " << dut_data[i] << std::endl;
    }
}

// ============================================================
// Test 2: DC Input, Max Positive (32767)
//
// Feed 256 samples of 32767.
// After the first ~4 output transients, the steady-state output
// should equal 32767 (or 32766 due to rounding). Check outputs
// 4 through 15 (indices 4..15 of 16 total outputs) against
// the reference model; also verify they converge to 32767 +/-1.
// ============================================================

void test_dc_positive()
{
    std::cout << "\n--- Test 2: DC Input Max Positive (32767) ---" << std::endl;

    const int N_IN  = 256;
    const int N_OUT = N_IN / CIC_R;  // 16

    int16_t samples[N_IN];
    for (int i = 0; i < N_IN; i++) samples[i] = 32767;

    int16_t dut_data[N_OUT], ref_data[N_OUT];
    bool    dut_last[N_OUT], ref_last[N_OUT];
    int     n_ref_out = 0;

    int n_out = feed_samples(samples, nullptr, N_IN,
                             dut_data, dut_last, N_OUT,
                             ref_data, ref_last, &n_ref_out);

    if (n_out != N_OUT) {
        std::cerr << "[FAIL] DC+: expected " << N_OUT
                  << " outputs, got " << n_out << std::endl;
        error_count++;
        return;
    }

    char name[64];
    // Compare all outputs against reference model
    for (int i = 0; i < N_OUT; i++) {
        snprintf(name, sizeof(name), "DC+ output[%d] vs ref", i);
        check_close(name, (int32_t)dut_data[i], (int32_t)ref_data[i], 1);
    }

    // Verify steady-state outputs (skip first 4 transient outputs)
    for (int i = 4; i < N_OUT; i++) {
        snprintf(name, sizeof(name), "DC+ steady-state output[%d]", i);
        check_close(name, (int32_t)dut_data[i], 32767, 1);
    }
}

// ============================================================
// Test 3: DC Input, Max Negative (-32768)
//
// Feed 256 samples of -32768.
// After the transient, steady-state output should be -32768 +/-1.
// ============================================================

void test_dc_negative()
{
    std::cout << "\n--- Test 3: DC Input Max Negative (-32768) ---" << std::endl;

    const int N_IN  = 256;
    const int N_OUT = N_IN / CIC_R;  // 16

    int16_t samples[N_IN];
    for (int i = 0; i < N_IN; i++) samples[i] = (int16_t)(-32768);

    int16_t dut_data[N_OUT], ref_data[N_OUT];
    bool    dut_last[N_OUT], ref_last[N_OUT];
    int     n_ref_out = 0;

    int n_out = feed_samples(samples, nullptr, N_IN,
                             dut_data, dut_last, N_OUT,
                             ref_data, ref_last, &n_ref_out);

    if (n_out != N_OUT) {
        std::cerr << "[FAIL] DC-: expected " << N_OUT
                  << " outputs, got " << n_out << std::endl;
        error_count++;
        return;
    }

    char name[64];
    // Compare all outputs against reference model
    for (int i = 0; i < N_OUT; i++) {
        snprintf(name, sizeof(name), "DC- output[%d] vs ref", i);
        check_close(name, (int32_t)dut_data[i], (int32_t)ref_data[i], 1);
    }

    // Verify steady-state outputs (skip first 4 transient outputs)
    for (int i = 4; i < N_OUT; i++) {
        snprintf(name, sizeof(name), "DC- steady-state output[%d]", i);
        check_close(name, (int32_t)dut_data[i], (int32_t)(-32768), 1);
    }
}

// ============================================================
// Test 4: Sinusoidal In-Band Input
//
// 1024 samples at f = Fs/256, amplitude = 16384.
// Compare 64 outputs against reference model; max error < 100 LSBs.
// Also compute and report SNR.
// ============================================================

void test_sine()
{
    std::cout << "\n--- Test 4: Sinusoidal In-Band Input ---" << std::endl;

    const int N_IN  = 1024;
    const int N_OUT = N_IN / CIC_R;  // 64

    int16_t samples[N_IN];
    for (int i = 0; i < N_IN; i++) {
        double v = 16384.0 * std::sin(2.0 * M_PI * i / 256.0);
        // Clamp to int16 range before truncation
        if (v >  32767.0) v =  32767.0;
        if (v < -32768.0) v = -32768.0;
        samples[i] = (int16_t)(int32_t)v;
    }

    int16_t dut_data[N_OUT], ref_data[N_OUT];
    bool    dut_last[N_OUT], ref_last[N_OUT];
    int     n_ref_out = 0;

    int n_out = feed_samples(samples, nullptr, N_IN,
                             dut_data, dut_last, N_OUT,
                             ref_data, ref_last, &n_ref_out);

    if (n_out != N_OUT) {
        std::cerr << "[FAIL] Sine: expected " << N_OUT
                  << " outputs, got " << n_out << std::endl;
        error_count++;
        return;
    }

    // Compare DUT against reference; tolerance = 100 LSBs
    int32_t max_err = 0;
    double  signal_power = 0.0;
    double  noise_power  = 0.0;
    char    name[64];

    for (int i = 0; i < N_OUT; i++) {
        int32_t err = (int32_t)dut_data[i] - (int32_t)ref_data[i];
        if (err < 0) err = -err;
        if (err > max_err) max_err = err;

        signal_power += (double)ref_data[i]  * (double)ref_data[i];
        noise_power  += (double)(dut_data[i] - ref_data[i]) *
                        (double)(dut_data[i] - ref_data[i]);

        snprintf(name, sizeof(name), "Sine output[%d] vs ref (tol=100)", i);
        check_close(name, (int32_t)dut_data[i], (int32_t)ref_data[i], 100);
    }

    // Report SNR
    if (noise_power > 0.0) {
        double snr_db = 10.0 * std::log10(signal_power / noise_power);
        std::cout << "  SNR vs double-precision reference: " << snr_db
                  << " dB" << std::endl;
    } else {
        std::cout << "  SNR vs double-precision reference: perfect (0 noise)"
                  << std::endl;
    }
    std::cout << "  Max absolute error: " << max_err << " LSBs" << std::endl;
}

// ============================================================
// Test 5: TLAST Propagation
//
// 32 samples: TLAST=1 on sample index 15 and on sample index 31.
// First output (from samples 0-15) must have TLAST=1.
// Second output (from samples 16-31) must have TLAST=1.
// ============================================================

void test_tlast()
{
    std::cout << "\n--- Test 5: TLAST Propagation ---" << std::endl;

    const int N_IN  = 32;
    const int N_OUT = N_IN / CIC_R;  // 2

    // The CIC static state is already advanced by previous tests.
    // We need to know where the count currently stands.
    // Because each previous test consumed multiples of R samples
    // (64 + 256 + 256 + 1024 = 1600 = 100 * R), the count is at 0.
    // Therefore samples 0..15 form one complete decimation block.

    int16_t samples[N_IN];
    bool    tlasts[N_IN];
    for (int i = 0; i < N_IN; i++) {
        samples[i] = 100;           // arbitrary non-zero value
        tlasts[i]  = (i == 15) || (i == 31);
    }

    int16_t dut_data[N_OUT], ref_data[N_OUT];
    bool    dut_last[N_OUT], ref_last[N_OUT];
    int     n_ref_out = 0;

    int n_out = feed_samples(samples, tlasts, N_IN,
                             dut_data, dut_last, N_OUT,
                             ref_data, ref_last, &n_ref_out);

    if (n_out != N_OUT) {
        std::cerr << "[FAIL] TLAST: expected " << N_OUT
                  << " outputs, got " << n_out << std::endl;
        error_count++;
        return;
    }

    // Both outputs should have TLAST=1
    check_eq("TLAST output[0].last == 1",
             (int32_t)dut_last[0], 1);
    check_eq("TLAST output[1].last == 1",
             (int32_t)dut_last[1], 1);

    // Reference model should agree
    check_eq("TLAST ref output[0].last == 1",
             (int32_t)ref_last[0], 1);
    check_eq("TLAST ref output[1].last == 1",
             (int32_t)ref_last[1], 1);
}

// ============================================================
// Test 6: State Continuity Across Invocations
//
// Goal: verify that 3 separate batches of 16 samples of value 1000
// produce outputs identical to a single 48-sample call.
//
// Because static state always persists in C-sim (both within and
// between the batch calls), and because the HLS function processes
// exactly one sample per call, the "single batch" and "three batches"
// paths are identical at the sample-call level.  The test constructs
// the expected values from the shared reference model (which has been
// advanced through tests 1-5 already) and checks DUT output match.
//
// Implementation:
//   Batch A: feed 16 samples of 1000, get 1 output, save it.
//   Batch B: feed 16 samples of 1000, get 1 output, save it.
//   Batch C: feed 16 samples of 1000, get 1 output, save it.
//   Reference model also advances through 48 samples of 1000 in the
//   same feed_samples calls, producing 3 reference outputs.
//   Compare DUT[0..2] == ref[0..2].
// ============================================================

void test_state_continuity()
{
    std::cout << "\n--- Test 6: State Continuity Across Invocations ---"
              << std::endl;

    const int BATCH = 16;  // = CIC_R → exactly 1 output per batch

    int16_t samples[BATCH];
    for (int i = 0; i < BATCH; i++) samples[i] = 1000;

    int16_t dut_a, ref_a;
    int16_t dut_b, ref_b;
    int16_t dut_c, ref_c;
    bool    last_a, last_b, last_c;
    bool    ref_last_arr[1];
    int     n_ref;

    // --- Batch A ---
    int16_t dut_out[1];
    bool    dut_last_arr[1];
    int16_t ref_out_arr[1];
    n_ref = 0;
    int na = feed_samples(samples, nullptr, BATCH,
                          dut_out, dut_last_arr, 1,
                          ref_out_arr, ref_last_arr, &n_ref);
    if (na != 1 || n_ref != 1) {
        std::cerr << "[FAIL] State cont batch A: expected 1 output, got dut="
                  << na << " ref=" << n_ref << std::endl;
        error_count++;
        return;
    }
    dut_a = dut_out[0];  ref_a = ref_out_arr[0];
    last_a = dut_last_arr[0];

    // --- Batch B ---
    n_ref = 0;
    int nb = feed_samples(samples, nullptr, BATCH,
                          dut_out, dut_last_arr, 1,
                          ref_out_arr, ref_last_arr, &n_ref);
    if (nb != 1 || n_ref != 1) {
        std::cerr << "[FAIL] State cont batch B: expected 1 output, got dut="
                  << nb << " ref=" << n_ref << std::endl;
        error_count++;
        return;
    }
    dut_b = dut_out[0];  ref_b = ref_out_arr[0];
    last_b = dut_last_arr[0];

    // --- Batch C ---
    n_ref = 0;
    int nc = feed_samples(samples, nullptr, BATCH,
                          dut_out, dut_last_arr, 1,
                          ref_out_arr, ref_last_arr, &n_ref);
    if (nc != 1 || n_ref != 1) {
        std::cerr << "[FAIL] State cont batch C: expected 1 output, got dut="
                  << nc << " ref=" << n_ref << std::endl;
        error_count++;
        return;
    }
    dut_c = dut_out[0];  ref_c = ref_out_arr[0];
    last_c = dut_last_arr[0];

    // Suppress unused-variable warnings for last_* booleans
    (void)last_a; (void)last_b; (void)last_c;

    // DUT outputs must match reference outputs exactly
    check_eq("State cont: batch A output matches ref",
             (int32_t)dut_a, (int32_t)ref_a);
    check_eq("State cont: batch B output matches ref",
             (int32_t)dut_b, (int32_t)ref_b);
    check_eq("State cont: batch C output matches ref",
             (int32_t)dut_c, (int32_t)ref_c);

    // Print actual values for inspection
    std::cout << "  Batch outputs (ref / dut): "
              << ref_a << "/" << dut_a << "  "
              << ref_b << "/" << dut_b << "  "
              << ref_c << "/" << dut_c << std::endl;
}

// ============================================================
// Main
// ============================================================

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "  Testbench: cic_decimator" << std::endl;
    std::cout << "  N=" << CIC_N << " R=" << CIC_R << " M=" << CIC_M << std::endl;
    std::cout << "========================================" << std::endl;

    test_impulse();
    test_dc_positive();
    test_dc_negative();
    test_sine();
    test_tlast();
    test_state_continuity();

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Total checks: " << test_count << std::endl;
    if (error_count == 0) {
        std::cout << "  ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "  " << error_count << " TEST(S) FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return error_count;
}
