# The patches: how to drive them

This folder rebuilds chosen archives out of ESP-IDF - `liblwip.a` always, the mbedTLS ones on request - with a
few sdkconfig values changed, and hands them to the linker in place of the ones shipped inside the PlatformIO
framework package. Nothing under the packages tree is modified; the archives are built here and *prepended* to
the library search path at link time.

| file | what it is |
|---|---|
| [`make_patch.py`](make_patch.py) | the one you run. Clones ESP-IDF at the exact version the installed libs package was built from, applies the whitelist, builds only the archives needed, harvests them here |
| [`pre_patch.py`](pre_patch.py) | runs before every firmware build. Checks whether what is in this folder still matches the installed package, and stays silent when there is no patch |
| [`post_patch.py`](post_patch.py) | runs after every firmware build. Prepends the folder to `LIBPATH` and prints `Patch linked: ...` once |
| `patch-info.txt` | written by the maker, read by the two hooks. IDF version and commit, the sha of the `sdkconfig` it started from, the changes it applied, the toolchain, the archive sha |
| `<mcu>/` | one directory per MCU: `esp32s3/liblwip.a`, and whatever else that target's run produced |

Both hooks key off SCons `COMMAND_LINE_TARGETS` and skip a run whose only targets are `buildfs`/`uploadfs`: packing
LittleFS never links, so there is nothing to check and nothing to shadow. A combined `-t upload -t uploadfs` still
runs them, because the firmware is linked in that run.

## 0. What gets patched, and why each archive exists

| archive | produced by | what it changes | needed? |
|---|---|---|---|
| `liblwip.a` | always | the TCP numbers: `TCP_WND_DEFAULT`, `TCP_RECVMBOX_SIZE`, `TCP_SND_BUF_DEFAULT`, `MAX_ACTIVE_TCP` | **yes**, this is the streaming patch |
| `libmbedcrypto.a`, `libmbedx509.a`, `libmbedtls.a` | the default run (`--no-mbedtls` skips them) | `MBEDTLS_EXTERNAL_MEM_ALLOC=y` - mbedTLS puts its record buffers and context in PSRAM instead of internal DRAM | yes on 8.0.0: a TLS handshake wants ~40 KB contiguous and the internal pool's largest block sits at 17-30 KB |
| `libesp_netif.a` | `--with-netif` | rebuilds ESP-IDF's network interface layer from the same sdkconfig | **no**, and it is not in the folder. See below |

`esp_netif` is the layer between the WiFi driver's events and lwIP: it owns the netif glue, the DHCP client
and server, and the DNS plumbing. Some `CONFIG_LWIP_*` values are compiled *into* it as well as into lwIP, and
for those keys a rebuilt `liblwip.a` would have to be matched by a rebuilt `libesp_netif.a` or the two would
disagree. None of the keys this script changes are among them - a window or a mailbox size is only read inside
lwIP itself - so it is an opt-in escape hatch rather than part of the patch. If it were needed and were not
rebuilt, the failure would not be slowness; it would be the network not coming up at all.

## 1. One-time check

You need `git`, a network connection the first time (it clones ESP-IDF), and the PlatformIO packages for the
target already downloaded. Nothing else: the script creates its own throwaway Python venv in the temp
directory, installs `cmake`, `ninja` and ESP-IDF's `requirements.core.txt` into it, and deletes the venv when
it finishes. The xtensa compiler is **reused** from `packages/toolchain-xtensa-esp-elf`, so only one copy of
the toolchain ever exists.

It reads the ESP-IDF version from each libs package's own `versions.txt`
(`framework-arduinoespressif32-esp32s3-libs/versions.txt`), not from `platformio.ini`. That file was written
by whoever built the package, so it is the only honest answer to "which IDF are we matching".

## 2. The run

Print the help first; it lists every knob with its current default:

```
python builds/patch/make_patch.py --help
```

**With no `--target`, every installed libs package is patched**, in one invocation that shares a single
ESP-IDF clone between them:

```
python builds/patch/make_patch.py
```

That is the normal command. `--target` narrows it to the MCUs you name, and then only those:

```
python builds/patch/make_patch.py --target esp32s3
python builds/patch/make_patch.py --target esp32 --target esp32s3
```

**The mbedTLS archives are part of the default run**, because this codebase needs them: HTTPS on espressif32
8.0.0 fails without TLS buffers in PSRAM. So `make_patch.py` alone rebuilds all four archives, and
`--no-mbedtls` is the opt-out for the rare case where only the lwIP numbers are wanted.

| option | default | use |
|---|---|---|
| `--target MCU` | every installed libs package | which MCUs to patch; repeatable |
| `--wnd` | **32768** | `CONFIG_LWIP_TCP_WND_DEFAULT`, the TCP receive window. Capped at 65535, see §4 |
| `--recvmbox` | **32** | `CONFIG_LWIP_TCP_RECVMBOX_SIZE`, received segments queued per socket |
| `--sndbuf` | 8192 | `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`, send buffer (uploads, HTTP responses) |
| `--max-active` | 32 | `CONFIG_LWIP_MAX_ACTIVE_TCP`, how many sockets may exist |
| `--mbedtls` / `--no-mbedtls` | **on** | rebuild the three mbedTLS archives with `MBEDTLS_EXTERNAL_MEM_ALLOC=y`. `--no-mbedtls` builds `liblwip.a` only, and deletes the mbedTLS archives already in the folder, so the build links the package's own again |
| `--with-netif` | off | also rebuild `libesp_netif.a`. Not needed for the numbers we change - see §0 |
| `--yes`, `-y` | off | do not ask before the existing archives in `<mcu>/` are replaced |
| `--idf-path` | none | reuse an existing ESP-IDF checkout instead of cloning |
| `--temp-dir` | system temp | where the clone and build tree live |
| `--jobs` | core count | `ninja -j` |
| `--keep-temp` | off | do not delete the tree when finished |

The window and the mailbox are a pair: 32768 bytes is 23 segments at MSS 1436, so a 16-message mailbox would
only queue 23 KB and quietly become the ceiling. The script warns when the two disagree
(`WARNING: ... the mailbox, not the window, will be the ceiling`), which is why the defaults are 32768 and 32
together.

Two targets must be on the same ESP-IDF version; if the installed packages disagree the script refuses rather
than building two different patches in one go.

**Replacing what is already there.** The folder's contents *are* the patch - `post_patch.py` prepends `<mcu>/` to
`LIBPATH` and the linker takes every `*.a` in it - so a run that rebuilt only some of the archives would leave
the rest shadowing the package copies while its own note had stopped mentioning them. Every run therefore:

- before the clone, lists what is about to be replaced and asks `Existing patches will be removed. Confirm?
  [y/N]:`. Answering no exits without touching anything; `--yes` skips the question;
- after the build has produced its archives, deletes the `*.a` files and the note it did not just write.

The order is deliberate: asking first means a long build is not started for nothing, and deleting only at the end
means a failed clone or a failed ninja leaves the previous patch exactly as it was. Nothing but `*.a` and
`patch-info.txt` is ever removed, so anything else kept in the folder survives.

One consequence worth knowing: `--no-mbedtls` now really does un-patch mbedTLS. Those three archives are deleted,
the build links the package's own allocator, and the internal heap is back to the 17-30 KB largest block that
fails a ~40 KB TLS handshake - so the script prints a warning when it removes them.

## 3. Reading the output

Every line is prefixed with the step that produced it:

```
[find]    packages in D:\VSCode\PlatformIO\packages
[find]    targets: esp32, esp32s3 (auto-detected)
[find]    esp32s3: ESP-IDF v5.5.5 b774170ff46
[config]  esp32s3: base sdkconfig from framework-arduinoespressif32-esp32s3-libs
[config]    CONFIG_LWIP_TCP_WND_DEFAULT: 5760 -> 32768
[config]    CONFIG_LWIP_TCP_RECVMBOX_SIZE: 6 -> 32
[tool]    toolchain 14.2.0 from toolchain-xtensa-esp-elf
[temp]    working in C:\Users\...\Temp\ehradio-lwip
```

Then `[build]` lines from ninja, and finally the archives land in `<mcu>/`. When the values you asked for
already match the package's sdkconfig you get `nothing to change; this is a stock rebuild` - useful as a
control run.

`patch-info.txt` is the record. Read it when you are unsure what a build actually linked:

```
idf        v5.5.5 b774170ff46
sdkconfig  sha256 3f2a...    (the package's own file, before patching)
lwip       sha256 31f1...
changes    CONFIG_LWIP_TCP_WND_DEFAULT 5760 -> 32768
           CONFIG_LWIP_TCP_RECVMBOX_SIZE 6 -> 32
           CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC off -> on
extra_archives = mbedcrypto, mbedtls, mbedx509
```

Booleans are written `off -> on`, because `y` and `# KEY is not set` are sdkconfig spellings rather than English;
`absent` means the key was not in the package's sdkconfig at all. Both sides are kept deliberately, since
`off -> on` and `absent -> on` are different events. The same `changes` string is what `pre_patch.py` prints on
every build, so each `pio run` log states the mbedTLS move in plain words.

The note and the folder cannot disagree: `harvest()` writes what it built and then deletes every other archive
in the folder, so after a run the note explains exactly what is in there. A `libesp_netif.a` left over from an
earlier `--with-netif` run, or the mbedTLS archives after a `--no-mbedtls` run, are removed rather than silently
linked.

`[config] verified against config\sdkconfig.h: 6 value(s) as asked` is a read-back of the header the compiler
actually included, not of what was asked for: an enabled symbol is `#define KEY value` there and a disabled one
is simply absent. It runs after ninja, and when it disagrees the archive is **not** harvested - the folder keeps
the previous patch - and the report lists every mismatch with the line the file holds for that key. The throwaway
tree is kept in that case rather than deleted along with the evidence. `generated` in the note is the same
read-back, so a stale archive can be spotted from the record alone.

## 4. What you may and may not change

The archives are linked against the *other* precompiled IDF archives, which were built with the package's own
sdkconfig. So only values that do not change a type or struct layout shared across those archives are safe.
That is what the two lists at the top of the script are:

- `WHITELIST` - the keys it will change, all of them plain integers inside lwIP's own objects;
- `BLACKLIST` - keys it refuses to let change, and asserts are untouched afterwards:
  `CONFIG_LWIP_WND_SCALE` (turns `tcpwnd_size_t` from `u16_t` into `u32_t`, which corrupts every `tcp_pcb`),
  `CONFIG_LWIP_MAX_SOCKETS` (sizes lwIP's `fd_set`), `CONFIG_LWIP_PBUF_LINK_HLEN` (pbuf layout),
  and the IPv6 / SO_REUSE switches.

A request for a window above 65535 is refused with that reason in the message, because window scaling is the
only way to express it and window scaling is an ABI break. 65535 is the ceiling; 32 KB is the sensible top.

## 5. Verifying a build picked it up

The next `pio run` that links firmware should print, once, near the end:

```
Patch linked: liblwip.a 31f1... (4 archive(s) from builds/patch/esp32s3: liblwip.a, libmbedcrypto.a, libmbedtls.a, libmbedx509.a)
```

and the pre-build hook prints a one-liner naming the archive sha and the changes it records. If they are
missing on a firmware build, the pre-build hook found no patch for that MCU or refused a stale one, and it says which.
A `buildfs` or `uploadfs` log holds neither line, and that is the correct log for it.

Beware of one trap: **flash size does not change** when you swap these archives. Do not use it as evidence.
Use the log line, or the map file:

```
findstr /C:"builds/patch/esp32s3/liblwip.a" .pio\build\<env>\firmware.map
```

Thousands of matches means the linker took the objects from here. Two or three means it did not.

## 6. When it has to be re-run

- **After a platform upgrade.** A new framework version ships a new libs package with a new `versions.txt`
  and a different `sdkconfig`. `pre_patch.py` compares both against `patch-info.txt` and refuses the stale
  archive rather than linking an archive built for a different IDF. Re-run the maker.
- **After changing a number.** Obvious, but the archive holds the value, not the ini.
- **Never** as part of a normal build. This is a deliberate, occasional step - a build takes minutes and
  needs network the first time.

## 7. Two known rough edges

- On Windows a failed previous run can leave a read-only directory in the temp root; the script clears the
  read-only bits and retries, and reuses an existing clone when it is complete.
- The script does not use `idf.py`, because that insists on `IDF_TOOLS_PATH/python_env` and a constraints
  file it would have to find at a path that does not exist outside a real IDF install. It drives `cmake` and
  `ninja` directly with `-DPYTHON_DEPS_CHECKED=1`, which is why it needs its own venv and nothing else.

## 8. What the numbers are for

The receive window sets a throughput ceiling against latency, because throughput is window / round-trip time:

| window | 20 ms RTT | 50 ms RTT | 100 ms RTT |
|---|---|---|---|
| 5760 (stock in the package) | 2.3 Mbit/s | 0.9 | 0.5 |
| 16384 (`--wnd 16384`) | 6.6 | 2.6 | 1.3 |
| 32768 (`--wnd 32768`) | 13.1 | 5.2 | 2.6 |
| 65535 (ceiling without window scaling) | 26.2 | 10.5 | 5.2 |

What the traffic needs: the 24-bit lounge capture averaged about 1.06 Mbit/s (531 KB in four seconds), Radio
Paradise's FLAC is 1.44 Mbit/s, and the 6.1 Mbit stream is 763 KB/s. **16 KB would carry everything we play today** -
2.6 Mbit/s even at a pessimistic 50 ms round trip, against a stream that needs half of that. The default is still
32768 with `--recvmbox 32`, which is what a 6.1 Mbit stream needs at 50 ms: it fits at 20 ms, needs the window
doubled at 50 ms, and cannot be served at 100 ms by any legal window, because 65535 is the maximum without window
scaling and window scaling is an ABI break (`tcpwnd_size_t` is `u16_t` in every other precompiled archive).

The mailbox has to rise with the window: 32 KB / 1436-byte segments is 23 segments, so a 16-segment mailbox would
queue 23 KB and quietly become the ceiling instead. That is why `32768` pairs with `--recvmbox 32`, the pair
measured on hardware to hold a buffer that used to empty out.

Past lwIP the wall for a 6.1 Mbit stream is elsewhere: on VS1053 builds it is the SPI bus (6.7 MHz is 838 KB/s,
and 763 KB/s is 91% of it with nothing else on the bus), and on I2S builds it is the software decoder at
96 kHz/24-bit.
