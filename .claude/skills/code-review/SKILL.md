---
name: code-review
description: Code review. Use it when the user asks for a review, an audit or a quality check.
---

# Code review

- Read the changes (`git diff`).
- Check: logic bugs, error handling, edge cases, security, readability, compliance with the conventions in `CLAUDE.md`.
- Run the analysis (`clang-tidy`) on the changed files if the command is defined.
- List problems by severity: blocking, important, cosmetic.
- Do not modify the code unless explicitly asked.
