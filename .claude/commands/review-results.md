# /review-results — Review HLS Build Results

## Usage
/review-results <ip_name>

## Flow
1. Read mailbox/results/<ip_name>/status.md
2. Read mailbox/results/<ip_name>/synthesis_summary.md
3. Read mailbox/results/<ip_name>/issues.md (if exists)
4. Analyze: did it meet targets? Any issues?
5. Read `scripts/templates/mailbox/feedback.md` for feedback format
6. Write feedback to mailbox/feedback/<ip_name>/feedback.md
7. If optimization needed:
   a. Read `src/.template/optimization.md` for optimization format
   b. Read `scripts/templates/mailbox/proposal.md` for proposal format
   c. Generate optimization proposal.md and draft optimization.md
   d. Write to mailbox/queue/<ip_name>/ for user approval
8. If upgrade triggers detected: note patterns for environment improvement
