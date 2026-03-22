# /generate-spec — IP Specification Generation Pipeline

## Usage
/generate-spec <domain> [ip_name]

## Arguments
- <domain>: dsp | comm | radar | image | dl
- [ip_name]: (optional) specific IP from the domain catalog

## Flow
1. If no ip_name: present domain's IP catalog from `.claude/skills/domain-catalog.md`,
   ask user to select
2. Read templates:
   - `scripts/templates/mailbox/proposal.md` for proposal format
   - `src/.template/instruction.md` for instruction format
   - `src/.template/optimization.md` for optimization format (if applicable)
3. Generate proposal.md and draft instruction.md -> write to mailbox/queue/<ip_name>/
4. Present proposal to user for review
5. Wait for user decision (approve / reject / modify)
6. On approval: user copies approved files from queue/ to mailbox/approved/<ip_name>/
