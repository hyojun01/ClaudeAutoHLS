# ============================================================
# C-Synthesis Script for fir
# Run from: src/fir/
# Usage: vitis-run --tcl tcl/run_csynth.tcl
# ============================================================

open_project proj_fir
set_top fir

# Add source files
add_files src/fir.cpp
add_files src/fir.hpp

# Open solution
open_solution "sol1" -flow_target vivado

# Set FPGA part and clock
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

# Run C-Synthesis
csynth_design

exit
