## Description

<!-- What does this PR change, and why? -->

## Type of change

- [ ] Bug fix
- [ ] New feature / new hardware support
- [ ] Refactor / cleanup
- [ ] Documentation only

## Checklist

Before submitting, please confirm:

**Build & Test**
- [ ] The code compiles without errors or warnings (the automated compile check will verify this)
- [ ] Tested on physical hardware (please describe hardware in description above)

Confirm (if applicable):

**Code Check**
- [ ] No `delay()` added inside WebSocket message handlers or WiFi event callbacks
- [ ] New settings written to NVS via `saveValue()`, not direct `config.store.*` assignment
- [ ] New `#include` paths use the local convention (`"battery.h"` not `"core/battery.h"` and `"../libraries/I2S_Audio/Audio.h"` not `"libraries/I2S_Audio/Audio.h"`)
- [ ] New `.cpp` modules follow the existing class pattern with a global instance declared `extern` in the header (not C-style free functions)
- [ ] New user-visible settings are configurable from the Web UI, not locked behind a `#define`
- [ ] Use `snprintf` instead of `sprintf` without size bound
- [ ] `code-summary.md` updated if behavior, module flow, config flow, storage keys, WebUI logic, or build logic changed
