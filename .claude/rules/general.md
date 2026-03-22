# General Project Rules

## File Organization
- Every IP lives in its own directory under `src/<ip_name>/`
- Documentation for each IP lives under `docs/<ip_name>/`
- Never modify the user's `instruction.md` or `optimization.md` files
- Always create subdirectories (`src/`, `tb/`, `tcl/`, `reports/`) if they don't exist

## Naming Conventions
- IP names: lowercase with underscores (e.g., `fir_filter`, `axi_dma`)
- File names: match IP name (e.g., `fir_filter.hpp`, `fir_filter.cpp`)
- Testbench files: prefix with `tb_` (e.g., `tb_fir_filter.cpp`)
- TCL scripts: prefix with `run_` (e.g., `run_csim.tcl`)
- Top-level function name: must match IP name exactly

## Agent Workflow Rules
- The main agent always plans before delegating
- Subagents only act on explicit instructions from the main agent
- Every subagent output must be reviewed by the main agent before proceeding
- Maximum 3 iterations per step before escalating to the user
- Document every decision and its rationale

## Quality Gates
1. **Design Gate**: Code compiles, follows coding standards, interfaces are correct
2. **Simulation Gate**: C-simulation passes (return code = 0), all test cases pass
3. **Synthesis Gate**: Synthesis completes without errors, meets timing
4. **Optimization Gate**: Functional equivalence maintained, targets met or explained
5. **Documentation Gate**: All sections complete, numbers match reports
6. **Upgrade Gate**: All environment modifications are user-approved, backed up, and logged

## Version Control
- Backup files before optimization: `<n>_pre_opt.hpp/.cpp`
- Backup environment files before upgrades: `.claude/upgrades/backups/`
- Document all changes in `changelog.md`
- Each completed design/optimization cycle increments the version
- Environment upgrades are logged in `.claude/upgrades/upgrade-log.md`

## Environment Self-Improvement
- The environment can be upgraded during or after design/optimization sessions
- The main agent detects upgrade triggers; the upgrade agent generates proposals
- **No environment file is ever modified without explicit user approval**
- All upgrades must include rollback instructions and file backups
- Maximum 5 upgrade proposals per session to prevent upgrade fatigue
- See `.claude/rules/upgrade-governance.md` for full governance rules

## Mailbox Protocol

When the user provides an IP name for /design-ip or /optimize-ip:
1. First check if mailbox/approved/<ip_name>/ contains instruction.md
2. If found, copy to src/<ip_name>/instruction.md (create dir structure if needed)
3. Similarly for optimization.md
4. Proceed with the standard /design-ip or /optimize-ip pipeline
5. After completion, write results to mailbox/results/<ip_name>/ using the
   templates in scripts/templates/mailbox/:
   - status.md (from scripts/templates/mailbox/status.md)
   - synthesis_summary.md (from scripts/templates/mailbox/synthesis_summary.md)
   - issues.md (from scripts/templates/mailbox/issues.md) — only if problems occurred

### Template Rules
- ALWAYS read the corresponding template in scripts/templates/mailbox/ before
  writing any result file
- Generated documents must follow the template's section structure exactly

## Communication
- Agents communicate via structured plans and reports
- Use specific file paths in all references
- Quote line numbers when discussing code issues
- Include actual numbers from reports, never estimates or placeholders
