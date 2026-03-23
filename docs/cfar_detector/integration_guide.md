# Integration Guide: CFAR Detector

## Prerequisites
- Vivado Design Suite 2023.x or later
- Vitis HLS-generated IP exported as Vivado IP

## Export from Vitis HLS

Using TCL:
```tcl
open_project proj_cfar_detector
open_solution sol1
export_design -format ip_catalog -output cfar_detector_ip.zip
```

Or from the Vitis HLS GUI: Solution → Export RTL → IP Catalog.

## Add to Vivado Project

1. In Vivado, open **Settings → IP → Repository**
2. Add the directory containing the exported IP
3. In the Block Design, click **Add IP** and search for `cfar_detector`
4. Place the IP and connect interfaces

## Connect Interfaces

### Clock and Reset
- Connect `ap_clk` to the system clock (100 MHz recommended)
- Connect `ap_rst_n` to the system active-low reset

### AXI-Lite Control (ctrl)
- Connect to an AXI Interconnect driven by the processing system (PS)
- Assign an address range (minimum 4 KB)
- Register map:

| Offset | Register | Width | Description |
|--------|----------|-------|-------------|
| 0x00 | Control | 32 | Bit 0: ap_start, Bit 1: ap_done, Bit 2: ap_idle |
| 0x10 | alpha | 16 | Threshold scaling factor (ufixed<16,8> encoding) |
| 0x18 | cfar_mode | 2 | 0=CA, 1=GO, 2=SO |
| 0x20 | num_range_cells | 16 | Sweep length (37–65535) |

### AXI-Stream Data
- **power_in**: Connect to upstream data source (e.g., magnitude-squared block or DMA)
  - 32-bit data, TLAST marks end of sweep
- **detect_out**: Connect to downstream consumer (e.g., DMA to PS or detection post-processing)
  - 64-bit data: bits [31:0] = CUT power, bit [32] = detect flag

### Typical Block Design

```
PS (AXI-Lite) ──→ AXI Interconnect ──→ [ctrl] cfar_detector [power_in] ←── AXI DMA (S2MM)
                                                              [detect_out] ──→ AXI DMA (MM2S)
```

## Software Driver

### Basic Operation Sequence

```c
#include "xcfar_detector.h"  // Auto-generated driver

XCfar_detector cfar;
XCfar_detector_Config *cfg = XCfar_detector_LookupConfig(XPAR_CFAR_DETECTOR_0_DEVICE_ID);
XCfar_detector_CfgInitialize(&cfar, cfg);

// Configure
XCfar_detector_Set_alpha(&cfar, 0x0400);         // alpha = 4.0
XCfar_detector_Set_cfar_mode(&cfar, 0);           // CA-CFAR
XCfar_detector_Set_num_range_cells(&cfar, 256);   // 256 range cells

// Start processing (ensure DMA transfers are configured first)
XCfar_detector_Start(&cfar);

// Poll for completion
while (!XCfar_detector_IsDone(&cfar));
```

### Alpha Encoding

The `alpha` register stores a 16-bit value interpreted as `ufixed<16,8>`:
- Integer part: bits [15:8] (0–255)
- Fractional part: bits [7:0] (resolution: 1/256 ≈ 0.004)
- Encoding: `register_value = (uint16_t)(alpha_float * 256.0)`
- Example: alpha=4.5 → 0x0480, alpha=10.0 → 0x0A00

### Output Unpacking

```c
uint64_t result = dma_buffer[i];
uint32_t cut_power = (uint32_t)(result & 0xFFFFFFFF);
int detected = (result >> 32) & 1;
```
