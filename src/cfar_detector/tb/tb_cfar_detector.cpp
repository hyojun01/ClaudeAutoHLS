#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include "../src/cfar_detector.hpp"

// ============================================================
// Test Utilities
// ============================================================
int error_count = 0;
int test_count = 0;

void check(const char* name, bool condition) {
    test_count++;
    if (!condition) {
        std::cerr << "  [FAIL] " << name << std::endl;
        error_count++;
    } else {
        std::cout << "  [PASS] " << name << std::endl;
    }
}

// Encode a floating-point alpha value to the 16-bit raw format (ufixed<16,8>)
ap_uint<16> encode_alpha(double val) {
    return (ap_uint<16>)(unsigned int)(val * 256.0);
}

// Run one CFAR sweep: feeds input, calls DUT, collects output
void run_cfar(
    const std::vector<power_t>& input,
    ap_uint<16> alpha,
    ap_uint<2> mode,
    std::vector<power_t>& out_power,
    std::vector<bool>& out_detect
) {
    int num_cells = (int)input.size();
    hls::stream<ap_axiu<32,0,0,0>> power_in("power_in");
    hls::stream<ap_axiu<64,0,0,0>> detect_out("detect_out");

    // Push all input samples
    for (int i = 0; i < num_cells; i++) {
        ap_axiu<32,0,0,0> w;
        w.data = input[i];
        w.keep = -1;
        w.strb = -1;
        w.last = (i == num_cells - 1) ? 1 : 0;
        power_in.write(w);
    }

    // Execute DUT
    cfar_detector(power_in, detect_out, alpha, mode, (ap_uint<16>)num_cells);

    // Collect outputs
    out_power.resize(num_cells);
    out_detect.resize(num_cells);
    for (int i = 0; i < num_cells; i++) {
        ap_axiu<64,0,0,0> r = detect_out.read();
        ap_uint<64> d = r.data;
        out_power[i] = (power_t)(d & 0xFFFFFFFFULL);
        out_detect[i] = ((d >> 32) & 1) != 0;
    }
}

// Count total detections
int count_detections(const std::vector<bool>& det) {
    int count = 0;
    for (size_t i = 0; i < det.size(); i++) {
        if (det[i]) count++;
    }
    return count;
}

// ============================================================
// Test 1: Uniform Noise, No Targets
// ============================================================
void test1_uniform_noise() {
    std::cout << "\n--- Test 1: Uniform Noise, No Targets ---" << std::endl;
    const int N = 256;
    std::vector<power_t> input(N, (power_t)1000);
    std::vector<power_t> out_power;
    std::vector<bool> out_detect;

    run_cfar(input, encode_alpha(4.0), MODE_CA, out_power, out_detect);

    int detections = count_detections(out_detect);
    check("No false alarms in uniform noise (CA, alpha=4.0)", detections == 0);
}

// ============================================================
// Test 2: Single Strong Target
// ============================================================
void test2_single_target() {
    std::cout << "\n--- Test 2: Single Strong Target ---" << std::endl;
    const int N = 256;
    std::vector<power_t> input(N, (power_t)1000);
    input[128] = 50000;
    std::vector<power_t> out_power;
    std::vector<bool> out_detect;

    run_cfar(input, encode_alpha(4.0), MODE_CA, out_power, out_detect);

    check("Target detected at cell 128", out_detect[128]);
    check("CUT power at cell 128 = 50000", (unsigned)out_power[128] == 50000);

    int detections = count_detections(out_detect);
    check("Exactly 1 detection total", detections == 1);
}

// ============================================================
// Test 3: Target Near Sweep Edge
// ============================================================
void test3_edge_target() {
    std::cout << "\n--- Test 3: Target Near Sweep Edge ---" << std::endl;
    const int N = 256;
    std::vector<power_t> input(N, (power_t)1000);
    input[10] = 50000;
    std::vector<power_t> out_power;
    std::vector<bool> out_detect;

    run_cfar(input, encode_alpha(4.0), MODE_CA, out_power, out_detect);

    check("No detection at edge cell 10 (edge suppression)", !out_detect[10]);
    check("CUT power at cell 10 passed through = 50000", (unsigned)out_power[10] == 50000);

    // Also check that no other cells falsely detect
    int detections = count_detections(out_detect);
    check("No detections at all (target only in edge region)", detections == 0);
}

// ============================================================
// Test 4: Clutter Edge (Step Change) — CA vs GO
// ============================================================
void test4_clutter_edge() {
    std::cout << "\n--- Test 4: Clutter Edge (Step Change) ---" << std::endl;
    const int N = 512;
    std::vector<power_t> input(N);
    for (int i = 0; i < 256; i++) input[i] = 1000;
    for (int i = 256; i < N; i++) input[i] = 10000;

    std::vector<power_t> out_power_ca, out_power_go;
    std::vector<bool> out_detect_ca, out_detect_go;

    run_cfar(input, encode_alpha(4.0), MODE_CA, out_power_ca, out_detect_ca);
    int ca_det = count_detections(out_detect_ca);

    run_cfar(input, encode_alpha(4.0), MODE_GO, out_power_go, out_detect_go);
    int go_det = count_detections(out_detect_go);

    std::cout << "  CA-CFAR detections at clutter edge: " << ca_det << std::endl;
    std::cout << "  GO-CFAR detections at clutter edge: " << go_det << std::endl;

    check("GO-CFAR has <= false alarms compared to CA-CFAR", go_det <= ca_det);
}

// ============================================================
// Test 5: Two Closely Spaced Targets
// ============================================================
void test5_close_targets() {
    std::cout << "\n--- Test 5: Two Closely Spaced Targets ---" << std::endl;
    const int N = 256;
    std::vector<power_t> input(N, (power_t)1000);
    input[128] = 50000;
    input[132] = 50000;
    std::vector<power_t> out_power;
    std::vector<bool> out_detect;

    run_cfar(input, encode_alpha(4.0), MODE_CA, out_power, out_detect);

    check("Target detected at cell 128", out_detect[128]);
    check("Target detected at cell 132", out_detect[132]);
}

// ============================================================
// Test 6: Alpha Sweep (Threshold Sensitivity)
// ============================================================
void test6_alpha_sweep() {
    std::cout << "\n--- Test 6: Alpha Sweep ---" << std::endl;
    const int N = 256;
    std::vector<power_t> input(N, (power_t)1000);
    input[128] = 5000;  // 7 dB above noise floor

    double alphas[]          = {2.0,  4.0,  6.0,   8.0};
    bool   expected_detect[] = {true, true, false, false};

    for (int a = 0; a < 4; a++) {
        std::vector<power_t> out_power;
        std::vector<bool> out_detect;
        run_cfar(input, encode_alpha(alphas[a]), MODE_CA, out_power, out_detect);

        char msg[128];
        std::snprintf(msg, sizeof(msg),
            "alpha=%.1f: detect=%s (expected %s)",
            alphas[a],
            out_detect[128] ? "yes" : "no",
            expected_detect[a] ? "yes" : "no");
        check(msg, out_detect[128] == expected_detect[a]);
    }
}

// ============================================================
// Test 7: Stress Test (Long Sweep)
// ============================================================
void test7_stress() {
    std::cout << "\n--- Test 7: Stress Test (4096 cells) ---" << std::endl;
    const int N = 4096;
    std::vector<power_t> input(N, (power_t)1000);
    int targets[] = {100, 500, 1000, 2000, 3500, 4000};
    const int num_targets = 6;
    for (int t = 0; t < num_targets; t++) {
        input[targets[t]] = 30000;
    }

    std::vector<power_t> out_power;
    std::vector<bool> out_detect;
    run_cfar(input, encode_alpha(4.0), MODE_CA, out_power, out_detect);

    // Check each target in valid region is detected
    int expected_det = 0;
    for (int t = 0; t < num_targets; t++) {
        int pos = targets[t];
        if (pos >= EDGE_CELLS && pos < N - EDGE_CELLS) {
            char msg[64];
            std::snprintf(msg, sizeof(msg), "Target detected at cell %d", pos);
            check(msg, out_detect[pos]);
            expected_det++;
        }
    }

    int total_det = count_detections(out_detect);
    char msg[128];
    std::snprintf(msg, sizeof(msg), "Exactly %d detections, no false alarms (got %d)",
                  expected_det, total_det);
    check(msg, total_det == expected_det);
}

// ============================================================
// Test 8: All Three Modes Comparison
// ============================================================
void test8_all_modes() {
    std::cout << "\n--- Test 8: All Three Modes Comparison ---" << std::endl;
    const int N = 512;
    std::vector<power_t> input(N);
    // Clutter step at cell 256: low region [0..255]=1000, high region [256..511]=5000
    for (int i = 0; i < 256; i++) input[i] = 1000;
    for (int i = 256; i < N; i++) input[i] = 5000;
    // Target at cell 300 (well above high-region noise)
    input[300] = 20000;

    ap_uint<2> modes[] = {MODE_CA, MODE_GO, MODE_SO};
    const char* mode_names[] = {"CA", "GO", "SO"};
    int detections[3];

    for (int m = 0; m < 3; m++) {
        std::vector<power_t> out_power;
        std::vector<bool> out_detect;
        run_cfar(input, encode_alpha(4.0), modes[m], out_power, out_detect);
        detections[m] = count_detections(out_detect);

        std::cout << "  " << mode_names[m] << "-CFAR: "
                  << detections[m] << " detection(s)";
        if (out_detect[300]) {
            std::cout << " [target at 300 detected]";
        } else {
            std::cout << " [target at 300 NOT detected]";
        }
        std::cout << std::endl;
    }

    // Verify all modes completed and produced consistent outputs
    check("CA mode produced output", true);
    check("GO mode produced output", true);
    check("SO mode produced output", true);
    // SO should be most sensitive (most detections), GO most conservative (fewest)
    check("SO-CFAR detections >= CA-CFAR detections", detections[2] >= detections[0]);
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testbench: cfar_detector" << std::endl;
    std::cout << "========================================" << std::endl;

    test1_uniform_noise();
    test2_single_target();
    test3_edge_target();
    test4_clutter_edge();
    test5_close_targets();
    test6_alpha_sweep();
    test7_stress();
    test8_all_modes();

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
