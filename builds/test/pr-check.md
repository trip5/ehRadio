# PR Check Implementation Notes

This folder (`builds/test/`) holds the configuration for the automated PR compile check.

The workflow `.github/workflows/pr-check.yml` copies these files to the repo root and runs `pio build` before any PR to `dev` can be merged.

---

## What is checked automatically

### ✅ Implemented in `pr-check.yml`

| Check | How | Blocks merge? |
|---|---|---|
| **Branch guard** | Fail immediately if PR targets `ehradio` and actor is not repo owner | Yes |
| **Compile Environments**  | `pio run` | Yes |
| **Static analysis** (`cppcheck` via ReviewDog) | Posts inline review comments on changed lines | Yes (fails on any finding) |

### ✅ Enable in GitHub Settings (no workflow code needed)

- **Branch protection rules** — Settings → Branches → Add rule for `dev`:
  - Require status checks: `Branch check`, `Static analysis (cppcheck)`, `Compile firmware (test builds)`
  - Block force-push

- **GitHub CodeQL** — Settings → Security → Code scanning → Enable for C/C++.
  Catches buffer overflows, unsafe format strings, integer overflows, and more.
  Runs automatically on PRs once enabled. No workflow edits needed.
  Set to `security alerts: medium or higher`

---

## Automatic check with cppcheck - Step-by-step for Powershell 

Install on Windows: `winget install Cppcheck.Cppcheck`

Add to `PATH`: `[System.Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\Program Files\Cppcheck", [System.EnvironmentVariableTarget]::User)`

Refresh `PATH`: `$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "User") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "Machine");`

Check: `cppcheck --enable=warning,style,performance --suppress=missingIncludeSystem --suppress=unmatchedSuppression --suppress=syntaxError --inline-suppr --language=c++ --std=c++17 -I src -I src/core -I src/displays src/core src/displays/widgets src/main.cpp 2>&1`

As of `2026.04.04`, the codebase was verified clean locally (zero findings after suppressing Arduino SDK `syntaxError` false positives from `ESP_ARDUINO_VERSION_VAL()`). `fail_on_error: true` is already set in `pr-check.yml`.

If a future legitimate false positive needs to be suppressed, add `// cppcheck-suppress <id>` inline at the offending line in the source file. The `--inline-suppr` flag is already active.

---

## What is NOT checked automatically (manual review required)

These require human judgment. Use `.github/PULL_REQUEST_TEMPLATE.md` to remind contributors.
