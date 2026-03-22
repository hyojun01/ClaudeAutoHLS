# FIR Filter IP — Integration Guide

**IP Name:** `fir`
**Version:** 1.0
**Date:** 2026-03-21

This guide describes how to export the `fir` IP from AMD Vitis HLS and integrate it into an AMD Vivado block design for use on the xc7z020clg400-1 (Zynq-7020) or compatible device.

---

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| AMD Vitis HLS | 2023.1 or later | Required for synthesis and IP export |
| AMD Vivado | 2023.1 or later (must match Vitis HLS version) | Required for block design and bitstream |
| Xilinx Board Support Package (BSP) | Matching target board | Required for bare-metal software driver |
| C compiler | arm-none-eabi-gcc (Vitis SDK) or equivalent | For building bare-metal control software |

Ensure that Vitis HLS synthesis has been run successfully and the `reports/` directory contains a valid `csynth.rpt` before proceeding.

---

## Step 1: Run Synthesis in Vitis HLS

If synthesis has not yet been run, execute the C-synthesis TCL script from the IP source directory:

```bash
cd /home/hyojun/Claude_Auto_HLS/src/fir
vitis-run --tcl tcl/run_csynth.tcl
```

After successful synthesis, the project directory will contain a solution folder (e.g., `fir_proj/solution1/`) with the synthesized RTL and an IP export-ready structure.

---

## Step 2: Export the IP from Vitis HLS

Export the synthesized design as a Vivado-compatible IP-XACT package using the following TCL command sequence. This can be run interactively in the Vitis HLS Tcl console or added to the synthesis TCL script:

```tcl
# Open the existing project and solution
open_project fir_proj
open_solution solution1

# Export as IP-XACT for Vivado integration
# The exported IP will be placed under fir_proj/solution1/impl/ip/
export_design -format ip_catalog -description "45-tap Symmetric FIR Low-Pass Filter" -vendor "user" -version "1.0"

exit
```

The export produces a directory at:

```
src/fir/fir_proj/solution1/impl/ip/
```

This directory contains the `component.xml` (IP-XACT descriptor) and all RTL source files required by Vivado.

---

## Step 3: Add the IP Repository to Vivado

1. Open your Vivado project (or create a new one targeting xc7z020clg400-1).
2. In the Vivado GUI, go to **Tools → Settings → IP → Repository**.
3. Click the **+** button and navigate to the exported IP directory:
   ```
   <path_to_repo>/src/fir/fir_proj/solution1/impl/ip
   ```
4. Click **OK**. Vivado will scan the directory and register the `fir_v1_0` IP.
5. Click **Apply** and **OK** to close the Settings dialog.

Alternatively, add the repository path via TCL:

```tcl
set_property ip_repo_paths {<absolute_path>/src/fir/fir_proj/solution1/impl/ip} [current_project]
update_ip_catalog
```

---

## Step 4: Add the IP to a Block Design

1. Open or create a Block Design in Vivado (**Flow Navigator → IP Integrator → Create Block Design**).
2. Right-click in the canvas and select **Add IP**.
3. Search for `fir` in the IP catalog search bar and double-click `fir_v1_0` to instantiate it.
4. The IP block will appear on the canvas with the following visible ports:
   - `in_V` (AXI4-Stream slave)
   - `out_V` (AXI4-Stream master)
   - `s_axi_control` (AXI4-Lite slave, for `ap_ctrl_hs` registers)
   - `ap_clk` (clock input)
   - `ap_rst_n` (active-low synchronous reset)

---

## Step 5: Connect Interfaces

### Clock and Reset

Connect `ap_clk` to your system clock source (100 MHz recommended to meet timing). Connect `ap_rst_n` to the active-low peripheral reset from the Zynq processing system or a reset controller.

Using the **Run Connection Automation** wizard in Vivado will handle clock and reset connections automatically if a Zynq PS is present in the design.

### AXI4-Stream Data Interface

Connect the streaming ports to the adjacent IP in your data path:

| FIR IP Port   | Connect To                                        |
|---------------|--------------------------------------------------|
| `in_V`        | AXI4-Stream master of upstream IP (e.g., DMA S2MM, ADC IP) |
| `out_V`       | AXI4-Stream slave of downstream IP (e.g., DMA MM2S, DAC IP) |

Both ports use 16-bit TDATA, 2-bit TKEEP/TSTRB, and 1-bit TLAST. Ensure the connected IPs match these widths. If a width mismatch exists, insert an AXI4-Stream Data Width Converter IP between them.

### AXI4-Lite Control Interface

Connect `s_axi_control` to an AXI4-Lite master (typically the Zynq PS General Purpose AXI Master port, e.g., `M_AXI_GP0`). An AXI Interconnect or SmartConnect IP is typically used to fan out from the PS master to multiple IP slaves.

Assign a base address to the `s_axi_control` port in the **Address Editor** tab (e.g., `0x43C0_0000` with a 64 KB range). Record this base address — it is required for the software driver.

### Complete Connection Checklist

- [ ] `ap_clk` → 100 MHz clock net
- [ ] `ap_rst_n` → active-low peripheral reset
- [ ] `in_V` TDATA/TKEEP/TSTRB/TLAST/TVALID/TREADY → upstream AXI-Stream master
- [ ] `out_V` TDATA/TKEEP/TSTRB/TLAST/TVALID/TREADY → downstream AXI-Stream slave
- [ ] `s_axi_control` → AXI Interconnect → Zynq PS AXI master
- [ ] Base address assigned in Address Editor

---

## Step 6: Validate and Generate Bitstream

1. Click **Run Block Automation** and **Run Connection Automation** to resolve any remaining interface connections.
2. Click **Validate Design** (or press F6) to check for errors. Address any reported critical warnings before proceeding.
3. Right-click the block design in the Sources panel and select **Generate HDL Wrapper**.
4. Run **Generate Bitstream** from the Flow Navigator.

---

## Step 7: Software Driver — AXI4-Lite Register Map

The `fir` IP exposes the standard Vitis HLS `ap_ctrl_hs` register block on its `s_axi_control` port. All registers are 32 bits wide; only the bits listed below are used.

| Offset | Register Name | Bit(s) | Field Name  | Access | Description                                                   |
|--------|---------------|--------|-------------|--------|---------------------------------------------------------------|
| 0x00   | AP_CTRL       | 0      | `ap_start`  | W      | Write `1` to start one frame of filter execution              |
| 0x00   | AP_CTRL       | 1      | `ap_done`   | R      | Reads `1` when the current frame is complete; auto-clears     |
| 0x00   | AP_CTRL       | 2      | `ap_idle`   | R      | Reads `1` when the IP is idle and ready to accept `ap_start`  |
| 0x00   | AP_CTRL       | 3      | `ap_ready`  | R      | Reads `1` when the IP can accept the next `ap_start`          |
| 0x04   | GIE           | 0      | `gier`      | R/W    | Global interrupt enable; write `1` to enable interrupt output |
| 0x08   | IER           | 0      | `ie_done`   | R/W    | Interrupt enable for `ap_done` event                          |
| 0x08   | IER           | 1      | `ie_ready`  | R/W    | Interrupt enable for `ap_ready` event                         |
| 0x0C   | ISR           | 0      | `is_done`   | R/W1C  | Interrupt status for `ap_done`; write `1` to clear            |
| 0x0C   | ISR           | 1      | `is_ready`  | R/W1C  | Interrupt status for `ap_ready`; write `1` to clear           |

### Bare-Metal C Control Code

The following C snippet demonstrates polling-based control of the FIR IP from a Zynq ARM processor (no OS, bare-metal). Replace `FIR_BASE_ADDR` with the base address assigned in the Vivado Address Editor.

```c
#include <stdint.h>

/* Replace with the base address assigned in Vivado Address Editor */
#define FIR_BASE_ADDR   0x43C00000UL

/* Register offsets */
#define FIR_AP_CTRL     0x00
#define FIR_GIE         0x04
#define FIR_IER         0x08
#define FIR_ISR         0x0C

/* AP_CTRL bit masks */
#define AP_START        (1U << 0)
#define AP_DONE         (1U << 1)
#define AP_IDLE         (1U << 2)
#define AP_READY        (1U << 3)

/* Memory-mapped I/O helpers */
static inline void fir_write(uint32_t offset, uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)(FIR_BASE_ADDR + offset);
    *reg = value;
}

static inline uint32_t fir_read(uint32_t offset) {
    volatile uint32_t *reg = (volatile uint32_t *)(FIR_BASE_ADDR + offset);
    return *reg;
}

/*
 * fir_wait_idle() — Block until the FIR IP is idle (safe to start).
 * Call this before fir_start() if the IP may still be running.
 */
void fir_wait_idle(void) {
    while (!(fir_read(FIR_AP_CTRL) & AP_IDLE)) {
        /* spin */
    }
}

/*
 * fir_start() — Issue a single-frame start to the FIR IP.
 * The AXI-Stream data path must be ready to supply input samples
 * (TVALID asserted) before or immediately after this call.
 */
void fir_start(void) {
    fir_write(FIR_AP_CTRL, AP_START);
}

/*
 * fir_wait_done() — Block until the current frame is complete.
 * Returns when ap_done is asserted (one frame of TLAST-terminated data
 * has been fully processed and written to the output AXI-Stream).
 */
void fir_wait_done(void) {
    while (!(fir_read(FIR_AP_CTRL) & AP_DONE)) {
        /* spin */
    }
}

/*
 * Example usage — process one frame of N samples:
 *
 *   // 1. Ensure DMA is configured to stream N samples to fir in_V
 *   // 2. Ensure downstream DMA is configured to receive N samples from fir out_V
 *   fir_wait_idle();   // confirm IP is idle before starting
 *   fir_start();       // assert ap_start
 *   fir_wait_done();   // wait for ap_done (frame complete)
 *   // 3. Output data is now available in the downstream DMA buffer
 */
```

### Interrupt-Driven Operation (Optional)

To use interrupt-driven operation instead of polling:

1. Connect the IP's interrupt output port to the Zynq PS interrupt controller (PL-PS interrupt).
2. Write `1` to GIE (`0x04`) to enable the global interrupt output.
3. Write `1` to IER bit 0 (`0x08`) to enable the `ap_done` interrupt.
4. In the interrupt service routine, read ISR (`0x0C`) to determine the interrupt source, then write `1` to the corresponding ISR bit to clear it.

---

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---------|-------------|------------|
| IP does not start (ap_idle never clears) | `ap_start` not written, or AXI-Lite not connected | Verify base address and AXI connectivity; check that `fir_start()` executes |
| IP hangs indefinitely (ap_done never asserts) | Input AXI-Stream TLAST never arrives | Ensure the upstream DMA or data source asserts TLAST=1 on the final sample of the frame |
| Output data incorrect | Shift register state from previous frame | Reset the IP or flush with 44 zero samples before the first real frame |
| AXI-Lite read returns 0xDEADBEEF or garbage | Base address mismatch | Re-check the Vivado Address Editor assignment and recompile software |
| Timing violation at 100 MHz | Critical path too long after optimization | Re-run synthesis with tighter constraints or add pipeline registers |
