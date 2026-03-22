# ============================================================
# Co-Simulation Script for fir
# Run from: src/fir/
# Prerequisite: run_csynth.tcl must be executed first
# Usage: vitis-run --tcl tcl/run_cosim.tcl
# ============================================================

open_project proj_fir
set_top fir

# Add source files
add_files src/fir.cpp
add_files src/fir.hpp

# Add testbench files
add_files -tb tb/tb_fir.cpp

# Open solution
open_solution "sol1" -flow_target vivado

# Set FPGA part and clock
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

# Run Co-Simulation
cosim_design

exit
