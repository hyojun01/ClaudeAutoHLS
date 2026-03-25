# cic_decimator — Integration Guide

## 1. Exporting from Vitis HLS

After synthesis, export the IP for Vivado:

```tcl
open_project proj_cic_decimator
open_solution sol1
export_design -format ip_catalog
close_project
```

The exported IP will be available as a Vivado IP catalog entry.

## 2. Vivado Block Design Integration

### Adding the IP
1. Open Vivado, create or open a block design
2. Add the `cic_decimator` IP from the IP catalog
3. The IP has two AXI-Stream ports and a clock/reset pair

### Port Connections

| IP Port | Connect To | Notes |
|---------|-----------|-------|
| `din` (AXI-Stream Slave) | Upstream data source (e.g., NCO, mixer, ADC interface) | 16-bit signed samples at full rate |
| `dout` (AXI-Stream Master) | Downstream consumer (e.g., compensation FIR, DMA) | 16-bit signed samples at 1/16 rate |
| `ap_clk` | System clock | 100 MHz (or up to 144 MHz) |
| `ap_rst_n` | System reset (active-low) | Synchronous reset |

### No Software Control Required
The IP uses `ap_ctrl_none` — it is free-running with no start/done handshake. It begins processing as soon as valid data appears on `din` and produces output on `dout` every 16 input samples. No AXI-Lite driver or software initialization is needed.

## 3. AXI-Stream Interface Details

### Input Stream (`din`)

| Signal | Width | Description |
|--------|-------|-------------|
| `din_TDATA` | 16 bits | Input sample (ap_int<16>, two's complement) |
| `din_TKEEP` | 2 bits | Byte enables (set to 0x3) |
| `din_TSTRB` | 2 bits | Byte qualifiers (set to 0x3) |
| `din_TLAST` | 1 bit | Frame boundary marker |
| `din_TVALID` | 1 bit | Data valid (driven by upstream) |
| `din_TREADY` | 1 bit | Backpressure (driven by CIC) |

### Output Stream (`dout`)

| Signal | Width | Description |
|--------|-------|-------------|
| `dout_TDATA` | 16 bits | Decimated output (ap_int<16>, two's complement) |
| `dout_TKEEP` | 2 bits | Always 0x3 |
| `dout_TSTRB` | 2 bits | Always 0x3 |
| `dout_TLAST` | 1 bit | OR of R=16 input TLASTs in the decimation block |
| `dout_TVALID` | 1 bit | Asserted every 16th input cycle |
| `dout_TREADY` | 1 bit | Backpressure from downstream |

### Timing Relationship

```
Input  (Fs):    |S0|S1|S2|...|S14|S15|S16|S17|...|S30|S31|
                 \_______________/     \_______________/
                        |                      |
Output (Fs/16):        |Y0|                   |Y1|
```

One output sample is produced for every 16 input samples. The pipeline latency is 5 clock cycles from the 16th input sample to the output.

## 4. Typical System Placement

### Digital Downconverter (DDC) Chain

```
ADC --> [Mixer/NCO] --> [CIC Decimator] --> [Comp FIR] --> [Baseband Processing]
         (Fs)             (Fs -> Fs/16)      (Fs/16)          (Fs/16)
```

The CIC decimator sits after the mixer/NCO, performing the bulk of the sample rate reduction. A compensation FIR filter downstream corrects the CIC's sinc^4 passband droop.

### Multi-Stage Decimation

For very high decimation ratios (e.g., 256x), cascade multiple CIC stages:
```
[CIC R=16] --> [CIC R=16] = 256x total decimation
```

Or combine CIC with halfband filters:
```
[CIC R=16] --> [Halfband /2] --> [Halfband /2] = 64x total
```

## 5. Design Constraints

### Clock Frequency
- **Minimum**: No minimum (combinational logic only)
- **Maximum**: 144.45 MHz (estimated from synthesis)
- **Recommended**: 100 MHz (10 ns period, 3.077 ns slack)

### Reset Behavior
- Synchronous active-low reset (`ap_rst_n`)
- All static registers (integrators, comb delays, counter, TLAST accumulator) are power-on initialized to 0
- After reset, the filter requires ~4 output samples (64 input samples) to settle from the transient

### Backpressure
- The IP supports AXI-Stream backpressure on both ports
- If `dout_TREADY` is deasserted, the pipeline stalls (no data loss)
- If `din_TVALID` is deasserted, the pipeline idles

## 6. Resource Cost Summary

| Resource | Count | Notes |
|----------|-------|-------|
| BRAM | 0 | All state in registers |
| DSP | 0 | Multiplier-free |
| FF | 393 | State registers + pipeline |
| LUT | 485 | Adders + control logic |

Total utilization on xc7z020: < 1% for all resource types.

## 7. Source Files

| File | Purpose |
|------|---------|
| `src/cic_decimator/src/cic_decimator.hpp` | Header: types, constants, prototype |
| `src/cic_decimator/src/cic_decimator.cpp` | Top function implementation |
| `src/cic_decimator/tb/tb_cic_decimator.cpp` | Testbench with 6 test cases |
| `src/cic_decimator/tcl/run_csim.tcl` | C-simulation script |
| `src/cic_decimator/tcl/run_csynth.tcl` | C-synthesis script |
| `src/cic_decimator/tcl/run_cosim.tcl` | Co-simulation script |
