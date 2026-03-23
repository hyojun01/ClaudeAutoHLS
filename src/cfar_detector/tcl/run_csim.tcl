# ============================================================
# C-Simulation Script for cfar_detector
# ============================================================

open_project proj_cfar_detector
set_top cfar_detector

# Add source files
add_files src/cfar_detector.cpp
add_files src/cfar_detector.hpp

# Add testbench files
add_files -tb tb/tb_cfar_detector.cpp

# Open solution
open_solution "sol1" -flow_target vivado

# Set FPGA part and clock
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

# Run C-Simulation
csim_design

exit
