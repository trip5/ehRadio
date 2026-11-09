# Zoo Code / Roo Code Project Rules

## Primary Guidance Files

Before making any code changes, read and follow these project files:

- **[`.github/ai-coding-guide.md`](../.github/ai-coding-guide.md)** — AI coding rules, constraints, and project structure
- **[`.github/code-summary.md`](../.github/code-summary.md)** — Codebase summary (the ehRadio "Bible")
- **[`.github/code-issues.md`](../.github/code-issues.md)** — Known issues and gotchas: read it to check whether something is related to a known issue

## Quick Reference

The `.github/` directory is the single source of truth for all AI assistant rules and project documentation. Always check there first when making changes to this project.

## Write restrictions

- **Never add, edit or remove entries in `.github/code-issues.md`** unless the user asks for it in the message you are working on. It records issues that are unfixable now — not fixes underway, not fixes just completed, and not gotchas the assistant discovered while working.
- The `(ALL FIXED)` markers and fix notes that file's own header mentions are for the user to add, not for the assistant.
- **Never edit `.github/ai-coding-guide.md`** unless the message you are working on asks for it (it is Rule #3 there). It is a short rule sheet, not documentation: an addition is one or two lines, and explanation, history or subsystem detail belongs in `code-summary.md`.
- Durable rules and lessons found while working go in this file or in `ai-coding-guide.md`; technical facts go in `code-summary.md`.
