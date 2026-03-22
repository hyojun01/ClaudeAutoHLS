# ============================================================
# C-Synthesis Script for fft (256-point Radix-2 DIT FFT)
# ============================================================

set IP_NAME    "fft"
set TOP_FUNC   "fft"
set FPGA_PART  "xc7z020clg400-1"
set CLK_PERIOD 10

# --- Project Setup ---
open_project proj_${IP_NAME}
set_top ${TOP_FUNC}

add_files     src/${IP_NAME}.cpp
add_files     src/${IP_NAME}.hpp

# --- Solution ---
open_solution "sol1" -flow_target vivado
set_part ${FPGA_PART}
create_clock -period ${CLK_PERIOD} -name default

# --- Run C-Synthesis ---
csynth_design

exit
