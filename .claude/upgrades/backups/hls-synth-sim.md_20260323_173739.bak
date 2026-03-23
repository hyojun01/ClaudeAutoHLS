# Skill: HLS Synthesis & Simulation

## Vitis HLS TCL Command Reference

### Project Management
```tcl
open_project <project_name>        ;# Create or open a project
set_top <function_name>            ;# Set the top-level function
add_files <file> [-cflags "<flags>"] ;# Add source files
add_files -tb <file> [-cflags "<flags>"] ;# Add testbench files
open_solution <solution_name> -flow_target vivado ;# Create/open a solution
close_project
```

### Configuration
```tcl
set_part {<part_number>}           ;# Set FPGA part (e.g., xc7z020clg400-1)
create_clock -period <ns> -name default ;# Set clock period
config_interface -m_axi_alignment_byte_size 64 ;# AXI alignment
config_compile -pipeline_loops 0   ;# Disable automatic loop pipelining
```

### Execution
```tcl
csim_design                         ;# Run C-simulation
csim_design -clean                  ;# Clean before C-sim
csynth_design                       ;# Run C-synthesis
cosim_design                        ;# Run co-simulation
cosim_design -trace_level all       ;# Co-sim with full waveform
export_design -format ip_catalog    ;# Export as Vivado IP
export_design -format xo            ;# Export as Vitis .xo file
```

### Solution Comparison
```tcl
# Create multiple solutions with different optimizations
open_solution "baseline" -flow_target vivado
set_part {xc7z020clg400-1}
create_clock -period 10 -name default
csynth_design

open_solution "optimized" -flow_target vivado
set_part {xc7z020clg400-1}
create_clock -period 10 -name default
set_directive_pipeline "top_func/LOOP1" -II 1
csynth_design
```

## Common FPGA Part Numbers

| Part | Family | Typical Use |
|------|--------|------------|
| `xc7z020clg400-1` | Zynq-7000 | Low-cost embedded (PYNQ-Z2) |
| `xc7z045ffg900-2` | Zynq-7000 | Mid-range embedded |
| `xczu3eg-sbva484-1-i` | Zynq UltraScale+ | Ultra96 |
| `xczu9eg-ffvb1156-2-e` | Zynq UltraScale+ | ZCU102 |
| `xcu250-figd2104-2L-e` | Alveo U250 | Data center |
| `xcu280-fsvh2892-2L-e` | Alveo U280 | HBM data center |
| `xcvu9p-flgb2104-2-i` | Virtex UltraScale+ | High-performance |

## Synthesis Report Interpretation

### Key Report Files (after `csynth_design`)
```
proj_<name>/sol1/syn/report/
├── <top_func>_csynth.rpt          ;# Main synthesis report
├── csynth.xml                      ;# Machine-readable synthesis data
└── <top_func>_csynth.xml          ;# Detailed XML report
```

### What to Extract from Reports

**Timing**:
- Target clock period vs. estimated clock period
- If estimated > target → timing violation (need to relax clock or optimize critical path)

**Latency**:
- Min/max latency in clock cycles
- Check if latency is deterministic (min == max) or variable

**Initiation Interval (II)**:
- II = number of cycles before the next input can be accepted
- II = 1 means one new input per cycle (maximum throughput)
- II > target usually means memory port conflict or dependency issue

**Resource Utilization**:
- BRAM_18K: block RAM usage
- DSP: DSP slice usage (multipliers, MAC units)
- FF: flip-flop usage
- LUT: look-up table usage
- Compare against target device limits

### Common Synthesis Warnings and Solutions

| Warning | Cause | Solution |
|---------|-------|----------|
| "II violation" | Memory port conflict | Partition arrays, add more ports |
| "Unable to schedule" | Data dependency | Restructure code, break dependency chain |
| "Loop with variable bound" | Variable trip count | Use `ap_uint` for loop bounds or add `#pragma HLS LOOP_TRIPCOUNT` |
| "Unsupported: dynamic alloc" | `new`/`malloc` used | Replace with static arrays |
| "Uninitialized variable" | Read before write | Initialize all variables |

## Testbench Best Practices

### Golden Reference Pattern
```cpp
// Compare HLS function output against a simple C++ reference
void golden_reference(int in[], int out[], int n) {
    for (int i = 0; i < n; i++)
        out[i] = in[i] * 2;  // Simple, obviously-correct implementation
}

int main() {
    int in[N], hw_out[N], sw_out[N];
    // Initialize inputs
    for (int i = 0; i < N; i++) in[i] = i;

    // Run HLS function
    hls_function(in, hw_out, N);

    // Run golden reference
    golden_reference(in, sw_out, N);

    // Compare
    int errors = 0;
    for (int i = 0; i < N; i++) {
        if (hw_out[i] != sw_out[i]) {
            std::cerr << "Mismatch at " << i << std::endl;
            errors++;
        }
    }
    return errors;
}
```

### File-Based Test Vectors
```cpp
// For complex IPs, read test vectors from files
int main() {
    std::ifstream fin("test_input.dat");
    std::ifstream fref("test_expected.dat");
    // Read, execute, compare
}
```

### Tolerance-Based Comparison (for fixed-point / floating-point)
```cpp
template<typename T>
bool approx_equal(T a, T b, double tolerance = 1e-4) {
    return std::abs((double)a - (double)b) < tolerance;
}
```

## Complete TCL Script Template

```tcl
#!/usr/bin/env tclsh
# ============================================================
# Vitis HLS Build Script: <ip_name>
# ============================================================

set IP_NAME    "<ip_name>"
set TOP_FUNC   "<ip_name>"
set FPGA_PART  "xc7z020clg400-1"
set CLK_PERIOD 10

# --- Project Setup ---
open_project proj_${IP_NAME}
set_top ${TOP_FUNC}

add_files     src/${IP_NAME}.cpp
add_files     src/${IP_NAME}.hpp
add_files -tb tb/tb_${IP_NAME}.cpp

# --- Solution: Baseline ---
open_solution "sol1" -flow_target vivado
set_part ${FPGA_PART}
create_clock -period ${CLK_PERIOD} -name default

# --- Run Flows ---
csim_design -clean
csynth_design
# cosim_design  ;# Uncomment for co-simulation
# export_design -format ip_catalog ;# Uncomment to export IP

close_project
exit
```

## Report Parser Tool

A Python script at `scripts/parse_hls_report.py` provides structured report parsing from `csynth.xml`.

### Subcommands

| Subcommand | Purpose | Input | Output |
|------------|---------|-------|--------|
| `parse`    | Extract all metrics from a csynth.xml | One XML path | JSON with design, timing, latency, resources, interfaces |
| `compare`  | Diff two reports (before/after) | Two XML paths | JSON with deltas, speedups, resource changes |
| `check`    | Flag anomalies | One XML path + thresholds | JSON with categorized warnings/errors |

### Key JSON Fields for Agents

| Agent Need | JSON Path |
|-----------|-----------|
| Timing met? | `timing.met` |
| Clock slack | `timing.slack_ns` |
| Worst-case latency | `latency.worst_cycles` |
| Pipeline II per loop | `loops[N].pipeline_ii` |
| LUT utilization | `resources.lut.util_pct` |
| DSP utilization | `resources.dsp.util_pct` |
| Port protocol | `interfaces[N].protocol` |
| Design style | `latency.pipeline_type` |

### Handling Special Cases

- **Streaming designs**: latency values may be `null` (from XML `"undef"`) — this is expected for auto-rewinding loop pipelines with unbounded trip count
- **Dataflow designs**: `latency.dataflow_throughput_cycles` contains the pipeline throughput in cycles; `loops` array may be empty at top level
- **Resource zero-available**: URAM on parts without URAM reports `available: 0` and `util_pct: 0.0`
