# Cross-Platform CI Fixes (July 2026)

How the six-platform **Build** workflow went from "Windows-only" to all
green, in the order the fixes landed. All changes shipped on the
`fix/cross-platform-ci` branch (PR #20 → `heros-shade`), commits
`0ac25c9a9c` → `b34b9b7d1a`.

**Why it mattered:** CI artifacts are the mod's distribution channel.
Every push builds Linux, Windows, macOS (x86_64 + arm64), iOS, and
Android and uploads per-platform artifacts; a red job means no build for
that platform. Starting point: run `28562704319` — five of six jobs red,
three independent root causes, with two more classes of failure hiding
behind them.

The recurring theme: **the fork's code had only ever been compiled by
MSVC.** Upstream builds green on all platforms because none of the
fork-side code (funchook, the mod API, hook anchors) exists there.

---

## 1. MSVC-only `__declspec(noinline)`

**Failed:** Linux GCC, Android Clang, macOS arm64
**Symptom:** `d_s_room.cpp:34: error: expected constructor, destructor, or
type conversion before '(' token` (GCC) / `'__declspec' attributes are not
enabled` (Clang)

The `dStage_onRoomActorsReady()` hook anchor was declared
`__declspec(noinline)` — MSVC-only syntax. GCC and Clang spell it
`__attribute__((noinline))`.

**Fix:** `DUSK_NOINLINE` macro in `include/global.h` (MSVC → `__declspec`,
GCC/Clang → `__attribute__`, else empty), used in `src/d/d_s_room.cpp`.
On MSVC it expands to the identical tokens, so Windows was unaffected.

**Rule going forward:** no bare MSVC-isms in fork code. Use
`DUSK_NOINLINE`, or guard with `#ifdef _MSC_VER` / `#ifdef _MSVC_LANG`
(see `d_menu_fmap.h` for a properly guarded `__declspec(property)`).

## 2. funchook built for the wrong CPU (macOS x86_64)

**Failed:** macOS x86_64
**Symptom:** `funchook_arm64.c: use of undeclared identifier
'FUNCHOOK_ARM64_REG_X9'` (and ~30 more) while compiling with
`-arch x86_64`

funchook selects its backend source from `CMAKE_SYSTEM_PROCESSOR` — the
**host** CPU. The macOS x86_64 job cross-compiles on an arm64
`macos-latest` runner via `CMAKE_OSX_ARCHITECTURES=x86_64`, so funchook
compiled its arm64 backend with an x86_64 compiler.

**Fix:** in `CMakeLists.txt`, before `FetchContent_MakeAvailable(funchook)`:
when `APPLE` and exactly one `CMAKE_OSX_ARCHITECTURES` value is set, pin
`FUNCHOOK_CPU` to the target arch (`x86_64` → `x86`, `arm64*` → `arm64`).
x86_64 then takes the same x86/distorm path Windows and Linux use.

## 3. iOS configure error: empty entitlements variable

**Failed:** iOS (at CMake configure, before compiling anything)
**Symptom:** `set_target_properties called with incorrect number of
arguments` at the Apple bundle-properties block

`DUSK_ENTITLEMENTS` is only set in the macOS branch of the resource-dir
selection. On iOS it is empty, so
`XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${DUSK_ENTITLEMENTS}` expanded to
a property name with no value — an odd argument count, which is a hard
configure error. (Introduced 2026-04-24 by `e25a1f3ef6`; iOS CI had been
broken since.)

**Fix:** set that property in a separate `set_target_properties` call
guarded by `if (DUSK_ENTITLEMENTS)`.

## 4. `RTLD_DEEPBIND` does not exist on Android

**Failed:** Android
**Symptom:** `mod_loader.cpp: error: use of undeclared identifier
'RTLD_DEEPBIND'`

`RTLD_DEEPBIND` is a glibc extension. Android defines `__linux__`, but
Bionic has no such flag, so the mod loader's
`#if defined(__linux__)` branch didn't compile.

**Fix:** gate on `#if defined(__linux__) && !defined(__ANDROID__)`;
Android uses the plain `RTLD_LAZY | RTLD_LOCAL` path.

## 5. iOS link error: missing `__clear_cache`

**Failed:** iOS (final app link)
**Symptom:** `Undefined symbols for architecture arm64: "___clear_cache",
referenced from: _funchook_prepare/_funchook_install/_funchook_uninstall`

funchook's code patcher calls `__builtin___clear_cache`, which lowers to
an external `__clear_cache` call. macOS's compiler-rt ships that symbol;
**iOS's does not** (Apple's supported API is `sys_icache_invalidate`).

**Fix:** `src/dusk/ios/clear_cache_shim.c` — an iOS-only TU bridging
`__clear_cache(start, end)` to `sys_icache_invalidate`, added in the
`if (IOS)` block of `CMakeLists.txt`.

**Runtime caveat:** this makes iOS *link*. Real devices enforce W^X (no
writable+executable pages without a JIT entitlement), so
`funchook_install` is expected to fail at runtime on-device. The
engine-source half of the mod works; runtime hook patching on iOS is an
open design question, not a build problem.

## 6. sccache served a stale precompiled header (Linux)

**Failed:** Linux — on code that had passed one run earlier, twice in a
row (rerun included)
**Symptom:** `d_s_room.cpp:34: error: 'DUSK_NOINLINE' does not name a
type` — i.e. the *fixed* file failing as if `global.h` didn't contain the
new macro

The CI wraps compiles in sccache (GitHub-Actions cache backend), and the
`dusk` target used a CMake precompiled header whose include chain
(`cmake_pch.hxx` → `dusk_pch.hpp` → `dolzel*.pch`) transitively contains
`global.h`. GCC `.gch` snapshots record include guards, so a **stale**
`.gch` (built from the pre-fix `global.h`) silently suppresses re-reading
the real header in every TU. sccache keys on textually-preprocessed
content while the real compile consumes whatever `.gch` is on disk — a
non-hermetic combination; once the poisoned artifact was in the cache,
every rerun restored it.

**Fix (both parts):**

- `SCCACHE_GHA_VERSION: "2"` in `.github/workflows/build.yml` — salts the
  cache namespace, evicting all poisoned entries at once. **Bump this
  number again any time the sccache cache needs a full reset.**
- `CMAKE_DISABLE_PRECOMPILE_HEADERS: "ON"` in the `x-linux-ci` preset —
  removes the unsound GCC-PCH-under-sccache combination permanently.
  Per-TU sccache caching still works and is keyed soundly. Local dev
  presets keep PCH for build speed.

## 7. Missing includes the PCH had been masking (30 files)

**Failed:** Linux, once PCH was off
**Symptom:** a long tail of `'u8' was not declared`, `'getSettings': is
not a member of 'dusk'`, `'FILE': undeclared identifier`, …

With the PCH gone, every header and TU that had silently leaned on it
for declarations stopped compiling. These were latent bugs in the code,
not artifacts of the CI change — headers are supposed to include what
they use.

**Method:** rather than discovering one file per 20-minute CI cycle, the
local **MSVC** build was flipped to no-PCH
(`cmake -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON`) and swept with
`ninja -k 0` (keep going past errors), which enumerates every broken TU
in one pass. Missing-include errors are compiler-agnostic, so the MSVC
sweep predicts the GCC failures. Error count converged
1143 → 325 → 66 → 22 → 3 → 0 across five quick incremental sweeps, ending
in a clean build **and link** of `dusk.exe` without PCH. Then the local
config was restored to normal PCH mode.

**Typical gaps** (see commit `dae45ee055` for the full 31-file list):

| Missing | Provides |
|---|---|
| `dolphin/types.h` | `u8` / `u32` / `s16` |
| `<os.h>` | `OSGetTime`, `OSTime`, `OS_LANGUAGE_*`, `OSPanic` |
| `dusk/settings.h` | `dusk::getSettings`, `DiscVerificationState`, `GameLanguage` |
| `m_Do/m_Do_audio.h` | `mDoAud_seStartMenu` (UI sound calls) |
| `global.h` | `VERSION_*` constants, `CRASH` |
| `<cstdio>` / `<cstdarg>` / `<cstdlib>` / `<initializer_list>` | `FILE`, `va_start`, `abort`, `std::initializer_list` |
| `Z2AudioLib/Z2SeMgr.h` | `Z2SE_*` sound IDs |
| `dusk/gx_helper.h` | `TGXTexObj` |
| `f_op/f_op_actor_mng.h` | `fopAcM_*` |
| `<card.h>` / `<vi.h>` / `<dvd.h>` | `CARD_GCIFOLDER`, `VISetWindowSize`, `DVDDiskID` |

Plus one `TARGET_PC`-guarded forward declaration of `J3DVertexData` in
`J3DModelLoader.h`.

## 8. libstdc++ transitive-include stragglers

**Failed:** Linux (the last red job)
**Symptom:** `hook_system.cpp:143: error: 'remove_if' is not a member of
'std'`

The one class of gap the MSVC sweep structurally cannot catch: MS STL
leaks `<algorithm>` through other standard headers; libstdc++ does not.

**Fix:** `#include <algorithm>` in `src/dusk/hook_system.cpp` and (found
by grep, preemptively) `src/dusk/imgui/ImGuiSaveEditor.cpp`.

---

## Debugging workflow that worked

```bash
# job list + per-job conclusions for a run
gh run view <run-id> --repo vertechs-dev/dusk \
  --json jobs --jq '.jobs[] | "\(.databaseId) \(.conclusion) \(.name)"'

# real errors only (skip the offsetof/validity warning noise)
gh run view --repo vertechs-dev/dusk --job <job-id> --log-failed \
  | grep -E "error:|FAILED:|CMake Error"

# local no-PCH sweep to predict Linux include failures on Windows
cmake -B build\windows-msvc-relwithdebinfo -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
ninja -C build\windows-msvc-relwithdebinfo -k 0 dusk.exe   # collect ALL errors
cmake -B build\windows-msvc-relwithdebinfo -UCMAKE_DISABLE_PRECOMPILE_HEADERS  # restore
```

## Lessons / standing rules

1. **Write portable fork code.** GCC, Clang, and AppleClang all compile
   every engine-side line now; `DUSK_NOINLINE` exists so nobody
   reintroduces a bare `__declspec`.
2. **Headers include what they use.** Linux CI builds without PCH and is
   now the hygiene enforcer — a TU that leans on the PCH will go red
   there even though local MSVC/dev builds (PCH on) won't notice.
3. **Don't trust `std::` names you didn't include.** MSVC's transitive
   includes are generous; libstdc++'s are not.
4. **If Linux fails on code that "can't fail":** suspect the sccache
   cache before the code. The reset knob is `SCCACHE_GHA_VERSION` in
   `build.yml`.
5. **Cross-compiles need explicit arch pins** for dependencies that sniff
   the host (`FUNCHOOK_CPU`).

## Distribution notes

- The all-green run's artifacts (per platform) are the distributable
  builds; after merging, the `heros-shade` run produces the canonical
  set.
- The Android APK artifact is **unsigned**; sign before distribution.
- The iOS IPA needs its file structure rearranged for sideloaders, and
  runtime hook patching is expected to be blocked by W^X (see fix 5).
- For a clean version string in the window title (`Dusk v1.1.0 [D3D12]`),
  tag the release commit `v1.1.0` and build from a clean tree — see the
  `git describe` logic at the top of `CMakeLists.txt`.
