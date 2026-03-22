#include "fft.hpp"

// ============================================================
// Twiddle Factor ROM (precomputed: W_256^k = cos(2*pi*k/256) - j*sin(2*pi*k/256))
// 128 complex values for k = 0..127
// Shared across all butterfly_stage instantiations
// ============================================================

// Twiddle factor real parts: cos(2*pi*k/256), k=0..127
static const tw_t tw_real[HALF_FFT_SIZE] = {
     0.9999694824, 0.9996988187, 0.9987954562, 0.9972904567, 0.9951847267, 0.9924795346, 0.9891765100, 0.9852776424,
     0.9807852804, 0.9757021300, 0.9700312532, 0.9637760658, 0.9569403357, 0.9495281806, 0.9415440652, 0.9329927988,
     0.9238795325, 0.9142097557, 0.9039892931, 0.8932243012, 0.8819212643, 0.8700869911, 0.8577286100, 0.8448535652,
     0.8314696123, 0.8175848132, 0.8032075315, 0.7883464276, 0.7730104534, 0.7572088465, 0.7409511254, 0.7242470830,
     0.7071067812, 0.6895405447, 0.6715589548, 0.6531728430, 0.6343932842, 0.6152315906, 0.5956993045, 0.5758081914,
     0.5555702330, 0.5349976199, 0.5141027442, 0.4928981922, 0.4713967368, 0.4496113297, 0.4275550934, 0.4052413140,
     0.3826834324, 0.3598950365, 0.3368898534, 0.3136817404, 0.2902846773, 0.2667127575, 0.2429801799, 0.2191012402,
     0.1950903220, 0.1709618888, 0.1467304745, 0.1224106752, 0.0980171403, 0.0735645636, 0.0490676743, 0.0245412285,
     0.0000000000,-0.0245412285,-0.0490676743,-0.0735645636,-0.0980171403,-0.1224106752,-0.1467304745,-0.1709618888,
    -0.1950903220,-0.2191012402,-0.2429801799,-0.2667127575,-0.2902846773,-0.3136817404,-0.3368898534,-0.3598950365,
    -0.3826834324,-0.4052413140,-0.4275550934,-0.4496113297,-0.4713967368,-0.4928981922,-0.5141027442,-0.5349976199,
    -0.5555702330,-0.5758081914,-0.5956993045,-0.6152315906,-0.6343932842,-0.6531728430,-0.6715589548,-0.6895405447,
    -0.7071067812,-0.7242470830,-0.7409511254,-0.7572088465,-0.7730104534,-0.7883464276,-0.8032075315,-0.8175848132,
    -0.8314696123,-0.8448535652,-0.8577286100,-0.8700869911,-0.8819212643,-0.8932243012,-0.9039892931,-0.9142097557,
    -0.9238795325,-0.9329927988,-0.9415440652,-0.9495281806,-0.9569403357,-0.9637760658,-0.9700312532,-0.9757021300,
    -0.9807852804,-0.9852776424,-0.9891765100,-0.9924795346,-0.9951847267,-0.9972904567,-0.9987954562,-0.9996988187
};

// Twiddle factor imaginary parts: -sin(2*pi*k/256), k=0..127
static const tw_t tw_imag[HALF_FFT_SIZE] = {
    -0.0000000000,-0.0245412285,-0.0490676743,-0.0735645636,-0.0980171403,-0.1224106752,-0.1467304745,-0.1709618888,
    -0.1950903220,-0.2191012402,-0.2429801799,-0.2667127575,-0.2902846773,-0.3136817404,-0.3368898534,-0.3598950365,
    -0.3826834324,-0.4052413140,-0.4275550934,-0.4496113297,-0.4713967368,-0.4928981922,-0.5141027442,-0.5349976199,
    -0.5555702330,-0.5758081914,-0.5956993045,-0.6152315906,-0.6343932842,-0.6531728430,-0.6715589548,-0.6895405447,
    -0.7071067812,-0.7242470830,-0.7409511254,-0.7572088465,-0.7730104534,-0.7883464276,-0.8032075315,-0.8175848132,
    -0.8314696123,-0.8448535652,-0.8577286100,-0.8700869911,-0.8819212643,-0.8932243012,-0.9039892931,-0.9142097557,
    -0.9238795325,-0.9329927988,-0.9415440652,-0.9495281806,-0.9569403357,-0.9637760658,-0.9700312532,-0.9757021300,
    -0.9807852804,-0.9852776424,-0.9891765100,-0.9924795346,-0.9951847267,-0.9972904567,-0.9987954562,-0.9996988187,
    -1.0000000000,-0.9996988187,-0.9987954562,-0.9972904567,-0.9951847267,-0.9924795346,-0.9891765100,-0.9852776424,
    -0.9807852804,-0.9757021300,-0.9700312532,-0.9637760658,-0.9569403357,-0.9495281806,-0.9415440652,-0.9329927988,
    -0.9238795325,-0.9142097557,-0.9039892931,-0.8932243012,-0.8819212643,-0.8700869911,-0.8577286100,-0.8448535652,
    -0.8314696123,-0.8175848132,-0.8032075315,-0.7883464276,-0.7730104534,-0.7572088465,-0.7409511254,-0.7242470830,
    -0.7071067812,-0.6895405447,-0.6715589548,-0.6531728430,-0.6343932842,-0.6152315906,-0.5956993045,-0.5758081914,
    -0.5555702330,-0.5349976199,-0.5141027442,-0.4928981922,-0.4713967368,-0.4496113297,-0.4275550934,-0.4052413140,
    -0.3826834324,-0.3598950365,-0.3368898534,-0.3136817404,-0.2902846773,-0.2667127575,-0.2429801799,-0.2191012402,
    -0.1950903220,-0.1709618888,-0.1467304745,-0.1224106752,-0.0980171403,-0.0735645636,-0.0490676743,-0.0245412285
};

// ============================================================
// Bit-reverse lookup for 8-bit indices (256-point FFT)
// ============================================================
static const unsigned char bit_rev_table[FFT_SIZE] = {
      0, 128,  64, 192,  32, 160,  96, 224,  16, 144,  80, 208,  48, 176, 112, 240,
      8, 136,  72, 200,  40, 168, 104, 232,  24, 152,  88, 216,  56, 184, 120, 248,
      4, 132,  68, 196,  36, 164, 100, 228,  20, 148,  84, 212,  52, 180, 116, 244,
     12, 140,  76, 204,  44, 172, 108, 236,  28, 156,  92, 220,  60, 188, 124, 252,
      2, 130,  66, 194,  34, 162,  98, 226,  18, 146,  82, 210,  50, 178, 114, 242,
     10, 138,  74, 202,  42, 170, 106, 234,  26, 154,  90, 218,  58, 186, 122, 250,
      6, 134,  70, 198,  38, 166, 102, 230,  22, 150,  86, 214,  54, 182, 118, 246,
     14, 142,  78, 206,  46, 174, 110, 238,  30, 158,  94, 222,  62, 190, 126, 254,
      1, 129,  65, 193,  33, 161,  97, 225,  17, 145,  81, 209,  49, 177, 113, 241,
      9, 137,  73, 201,  41, 169, 105, 233,  25, 153,  89, 217,  57, 185, 121, 249,
      5, 133,  69, 197,  37, 165, 101, 229,  21, 149,  85, 213,  53, 181, 117, 245,
     13, 141,  77, 205,  45, 173, 109, 237,  29, 157,  93, 221,  61, 189, 125, 253,
      3, 131,  67, 195,  35, 163,  99, 227,  19, 147,  83, 211,  51, 179, 115, 243,
     11, 139,  75, 203,  43, 171, 107, 235,  27, 155,  91, 219,  59, 187, 123, 251,
      7, 135,  71, 199,  39, 167, 103, 231,  23, 151,  87, 215,  55, 183, 119, 247,
     15, 143,  79, 207,  47, 175, 111, 239,  31, 159,  95, 223,  63, 191, 127, 255
};

// ============================================================
// Read input stream with bit-reversal reordering (merged into single loop)
// ============================================================
static void read_and_reorder(hls::stream<axis_pkt>& in,
                             acc_t out_re[FFT_SIZE], acc_t out_im[FFT_SIZE]) {
    READ_AND_REORDER:
    for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        axis_pkt pkt = in.read();
        data_t re, im;
        re.range() = pkt.data.range(31, 16);
        im.range() = pkt.data.range(15, 0);
        int rev_idx = bit_rev_table[i];
        out_re[rev_idx] = re;
        out_im[rev_idx] = im;
    }
}

// ============================================================
// Template butterfly stage — one per FFT stage, enables DATAFLOW
// Separate input/output arrays eliminate in-place memory dependency
// ============================================================
template<int STAGE>
void butterfly_stage(acc_t in_re[FFT_SIZE], acc_t in_im[FFT_SIZE],
                     acc_t out_re[FFT_SIZE], acc_t out_im[FFT_SIZE]) {
    // Compile-time constants from template parameter
    const int half = 1 << STAGE;
    const int step = 1 << (STAGE + 1);
    const int tw_stride = HALF_FFT_SIZE >> STAGE;

    BUTTERFLY_LOOP:
    for (int j = 0; j < HALF_FFT_SIZE; j++) {
        #pragma HLS PIPELINE II=1
        // Index computation — all divisions/modulos by powers of 2 (optimized to shifts/masks)
        int group = j >> STAGE;
        int pos = j & (half - 1);
        int even_idx = group * step + pos;
        int odd_idx = even_idx + half;
        int tw_idx = pos * tw_stride;

        // Load twiddle factor
        tw_t wr = tw_real[tw_idx];
        tw_t wi = tw_imag[tw_idx];

        // Load odd element from input
        acc_t odd_r = in_re[odd_idx];
        acc_t odd_i = in_im[odd_idx];

        // Complex multiply W * X[odd] — split for BIND_OP pipelining
        acc_t m1 = wr * odd_r;
        acc_t m2 = wi * odd_i;
        acc_t m3 = wr * odd_i;
        acc_t m4 = wi * odd_r;
        // Pipeline multiplies across 3 clock cycles to meet 10 ns timing
        #pragma HLS BIND_OP variable=m1 op=mul impl=dsp latency=2
        #pragma HLS BIND_OP variable=m2 op=mul impl=dsp latency=2
        #pragma HLS BIND_OP variable=m3 op=mul impl=dsp latency=2
        #pragma HLS BIND_OP variable=m4 op=mul impl=dsp latency=2

        acc_t prod_r = m1 - m2;
        acc_t prod_i = m3 + m4;

        // Load even element from input
        acc_t even_r = in_re[even_idx];
        acc_t even_i = in_im[even_idx];

        // Butterfly: write to separate output arrays (no read-modify-write conflict)
        out_re[even_idx] = even_r + prod_r;
        out_im[even_idx] = even_i + prod_i;
        out_re[odd_idx]  = even_r - prod_r;
        out_im[odd_idx]  = even_i - prod_i;
    }
}

// ============================================================
// Write output stream from separate real/imag arrays
// ============================================================
static void write_output(acc_t in_re[FFT_SIZE], acc_t in_im[FFT_SIZE],
                         hls::stream<axis_pkt>& out) {
    WRITE_LOOP:
    for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        axis_pkt pkt;

        // Truncate acc_t back to data_t for output packing
        data_t real_out = (data_t)in_re[i];
        data_t imag_out = (data_t)in_im[i];

        // Pack real[31:16] and imag[15:0] into 32-bit word
        ap_uint<32> packed;
        packed.range(31, 16) = real_out.range();
        packed.range(15, 0)  = imag_out.range();

        pkt.data = packed;
        pkt.keep = 0xF;
        pkt.strb = 0xF;
        pkt.last = (i == FFT_SIZE - 1) ? 1 : 0;
        out.write(pkt);
    }
}

// ============================================================
// Top-Level Function — DATAFLOW across 10 stages
// ============================================================
void fft(
    hls::stream<axis_pkt>& in,
    hls::stream<axis_pkt>& out
) {
    // Interface pragmas
    #pragma HLS INTERFACE axis port=in
    #pragma HLS INTERFACE axis port=out
    #pragma HLS INTERFACE s_axilite port=return

    // DATAFLOW: each butterfly stage runs as an independent hardware module
    // with PIPO buffers between stages for frame-level pipelining
    #pragma HLS DATAFLOW

    // Intermediate buffers between stages (9 pairs for 10 stages)
    // Use LUTRAM instead of BRAM for PIPO buffers to reduce BRAM usage
    acc_t buf0_re[FFT_SIZE], buf0_im[FFT_SIZE];
    acc_t buf1_re[FFT_SIZE], buf1_im[FFT_SIZE];
    acc_t buf2_re[FFT_SIZE], buf2_im[FFT_SIZE];
    acc_t buf3_re[FFT_SIZE], buf3_im[FFT_SIZE];
    acc_t buf4_re[FFT_SIZE], buf4_im[FFT_SIZE];
    acc_t buf5_re[FFT_SIZE], buf5_im[FFT_SIZE];
    acc_t buf6_re[FFT_SIZE], buf6_im[FFT_SIZE];
    acc_t buf7_re[FFT_SIZE], buf7_im[FFT_SIZE];
    acc_t buf8_re[FFT_SIZE], buf8_im[FFT_SIZE];
    #pragma HLS BIND_STORAGE variable=buf0_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf0_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf1_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf1_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf2_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf2_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf3_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf3_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf4_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf4_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf5_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf5_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf6_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf6_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf7_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf7_im type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf8_re type=ram_s2p impl=lutram
    #pragma HLS BIND_STORAGE variable=buf8_im type=ram_s2p impl=lutram

    read_and_reorder(in, buf0_re, buf0_im);
    butterfly_stage<0>(buf0_re, buf0_im, buf1_re, buf1_im);
    butterfly_stage<1>(buf1_re, buf1_im, buf2_re, buf2_im);
    butterfly_stage<2>(buf2_re, buf2_im, buf3_re, buf3_im);
    butterfly_stage<3>(buf3_re, buf3_im, buf4_re, buf4_im);
    butterfly_stage<4>(buf4_re, buf4_im, buf5_re, buf5_im);
    butterfly_stage<5>(buf5_re, buf5_im, buf6_re, buf6_im);
    butterfly_stage<6>(buf6_re, buf6_im, buf7_re, buf7_im);
    butterfly_stage<7>(buf7_re, buf7_im, buf8_re, buf8_im);
    write_output(buf8_re, buf8_im, out);
}
