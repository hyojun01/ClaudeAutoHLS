# Vitis HLS IP Design & Verification Environment

An automated environment for FPGA IP design, verification, and optimization using **AMD Vitis HLS**, orchestrated by **Claude Code**.

## Quick Start

### 1. Design a New IP

Create a folder for your IP and add the specification:

```bash
mkdir -p src/my_filter/
cp src/.template/instruction.md src/my_filter/instruction.md
```

Edit `src/my_filter/instruction.md` with your IP's requirements (I/O ports, data types, algorithm, target FPGA, etc.), then run:

```
/design-ip my_filter
```

Claude Code will design the IP, write testbenches, run simulation and synthesis, and generate documentation automatically.

### 2. Optimize an Existing IP

Add an optimization instruction document:

```bash
cp src/.template/optimization.md src/my_filter/optimization.md
```

Edit `src/my_filter/optimization.md` with your performance targets, then run:

```
/optimize-ip my_filter
```

Claude Code will apply HLS optimizations, re-verify correctness, and update documentation.

## Project Structure

```
├── CLAUDE.md                 # Claude Code project reference
├── .claude/
│   ├── agents/               # Agent definitions
│   │   ├── main-agent.md     # Lead engineer (orchestrates subagents)
│   │   ├── design-agent.md   # IP design (C++ implementation)
│   │   ├── synth-sim-agent.md# Synthesis & simulation (TB + TCL)
│   │   ├── optimization-agent.md # Optimization (pragmas)
│   │   ├── docs-agent.md     # Documentation generation
│   │   └── upgrade-agent.md  # Environment self-improvement
│   ├── commands/
│   │   ├── design-ip.md      # /design-ip command
│   │   ├── optimize-ip.md    # /optimize-ip command
│   │   └── upgrade-env.md    # /upgrade-env command
│   ├── skills/               # Domain knowledge
│   │   ├── hls-design.md     # HLS design patterns
│   │   ├── hls-synth-sim.md  # Synthesis & simulation reference
│   │   ├── hls-optimization.md # Optimization techniques
│   │   └── ip-documentation.md # Documentation standards
│   ├── rules/
│       ├── general.md        # Project rules
│       ├── hls-coding.md     # HLS C++ coding standards
│       └── upgrade-governance.md # Environment upgrade rules
│   └── upgrades/
│       ├── upgrade-log.md    # Complete upgrade history
│       └── backups/          # Pre-upgrade file backups
├── src/                      # IP source repositories
│   └── .template/            # Templates for new IPs
├── docs/                     # Generated IP documentation
└── scripts/templates/        # TCL & C++ boilerplate templates
```

## Agents

| Agent | Role | Responsibility |
|-------|------|---------------|
| **Main Agent** | Senior FPGA/HLS Lead | Plans, orchestrates, validates, triggers upgrades |
| **Design Agent** | HLS Design Engineer | Implements IP in C++ |
| **Synth-Sim Agent** | HLS Verification Engineer | Testbenches, C-sim, synthesis |
| **Optimization Agent** | HLS Optimization Engineer | Pragmas, code restructuring |
| **Docs Agent** | Documentation Engineer | IP documentation, changelogs |
| **Upgrade Agent** | Environment Improvement Engineer | Proposes environment config upgrades |

## Commands

| Command | Description |
|---------|-------------|
| `/design-ip <name>` | Full IP design pipeline (design → sim → synth → docs) |
| `/optimize-ip <name>` | Optimization pipeline (optimize → re-verify → update docs) |

## Instruction Document Format

### `instruction.md` (Required for `/design-ip`)

Must include: functional description, I/O port table, data types, algorithm description, target FPGA part and clock period, and test scenarios.

### `optimization.md` (Required for `/optimize-ip`)

Must include: optimization goals (latency/throughput/resources) with priorities, acceptable trade-offs, and constraints.

See `src/.template/` for complete templates.

## Prerequisites

- **AMD Vitis HLS** 2023.1 or later
- Source the Vitis HLS environment: `source /path/to/Vitis_HLS/settings64.sh`
- Claude Code CLI

## Running TCL Scripts Manually

If Vitis HLS is not in the automated environment, run scripts manually:

```bash
cd src/<ip_name>
vitis-run --tcl tcl/run_csim.tcl      # C-simulation
vitis-run --tcl tcl/run_csynth.tcl    # C-synthesis
vitis-run --tcl tcl/run_cosim.tcl     # Co-simulation
```
