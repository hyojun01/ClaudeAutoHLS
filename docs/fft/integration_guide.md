# Integration Guide: FFT (256-Point Radix-2 DIT)

## Prerequisites

- Vivado Design Suite 2023.1 or later
- Vitis HLS 2025.1 (for re-synthesis or modification)
- Target board with Zynq-7000 (xc7z020clg400-1) or compatible FPGA

## Export from Vitis HLS

To export the IP for use in Vivado:

```tcl
open_project proj_fft
open_solution sol1
export_design -format ip_catalog -output ./ip_export
```

This generates a packaged IP in Vivado IP Catalog format.

## Add to Vivado Project

1. Open your Vivado project
2. Go to **Settings → IP → Repository** and add the exported IP directory
3. In the Block Design, click **Add IP** and search for `fft`
4. The IP will appear with AXI-Stream and AXI-Lite interfaces

## Connect Interfaces

### AXI-Lite Control

Connect the `s_axi_control` interface to the Zynq PS AXI GP port (via AXI Interconnect):
- This interface is used to start the IP and check done/idle status
- Register map: AP_CTRL at offset 0x00

### AXI-Stream Input

Connect `in_r` to your data source (e.g., AXI-Stream DMA, ADC interface, or another HLS IP):
- 32-bit data width
- TLAST must be asserted on the 256th sample

### AXI-Stream Output

Connect `out_r` to your data sink (e.g., AXI-Stream DMA or another processing block):
- 32-bit data width
- TLAST is asserted on the 256th output sample

### Clock and Reset

- Connect `ap_clk` to the system clock (target: 100 MHz, current max: ~43 MHz without optimization)
- Connect `ap_rst_n` to an active-low synchronous reset

## Software Driver

### Start the IP

```c
#include "xfft.h"  // Auto-generated driver header

XFft fft_inst;
XFft_Config *cfg = XFft_LookupConfig(XPAR_FFT_0_DEVICE_ID);
XFft_CfgInitialize(&fft_inst, cfg);

// Start processing
XFft_Start(&fft_inst);

// Wait for completion
while (!XFft_IsDone(&fft_inst));
```

### Data Transfer with DMA

```c
// Configure DMA to send 256 samples (256 × 4 bytes = 1024 bytes)
XAxiDma_SimpleTransfer(&dma, (UINTPTR)input_buffer, 1024, XAXIDMA_DMA_TO_DEVICE);

// Start FFT
XFft_Start(&fft_inst);

// Configure DMA to receive 256 samples
XAxiDma_SimpleTransfer(&dma, (UINTPTR)output_buffer, 1024, XAXIDMA_DEVICE_TO_DMA);

// Wait for both transfers to complete
while (XAxiDma_Busy(&dma, XAXIDMA_DMA_TO_DEVICE));
while (XAxiDma_Busy(&dma, XAXIDMA_DEVICE_TO_DMA));
```

### Data Format

Pack input samples in software:
```c
uint32_t pack_complex(int16_t real, int16_t imag) {
    return ((uint32_t)(uint16_t)real << 16) | (uint16_t)imag;
}

void unpack_complex(uint32_t packed, int16_t *real, int16_t *imag) {
    *real = (int16_t)(packed >> 16);
    *imag = (int16_t)(packed & 0xFFFF);
}
```

Note: The fixed-point format is Q1.15 (1 integer bit, 15 fractional bits). To convert from floating-point: `int16_t fixed = (int16_t)(float_val * 32768.0f)`.
