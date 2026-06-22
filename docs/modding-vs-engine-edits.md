# Two Ways to Build: the Mod API vs. Editing Engine Source

The TP Combat project is built **two ways at once**:

1. **Through the Dusk mod API** — compiled into a `.dusk` archive that the
   loader picks up at startup. Hooks, polls, ImGui panels, bundled resources.
   This is the surface documented in [`modding.md`](modding.md).
2. **By editing the game's engine source directly** — real changes to the
   Dusk C++ source on our custom fork, built into the `dusk.exe` / `dusk.dll`
   we ship alongside the mod.

This document explains *why* the project uses both, *which* approach owns
*what*, and how to decide where a new change belongs. It is the orientation
note for anyone — human or agent — touching either repo.

## It used to be API-only

The original convention was strict: **TP Combat had to be a standalone
`.dusk` that loaded on stock Dusk, with zero engine changes** (or, at most,
ABI-safe `#if TARGET_PC` inline accessors that didn't shift any class
layout). The reasoning is captured in the now-superseded header of
[`TP-Combat-Distribution.md`](../../tp-combat-mod/docs/TP-Combat-Distribution.md):

> ⚠️ **SUPERSEDED (2026-06-18).** This doc analyzes shipping TP Combat as a
> standalone `.dusk` that runs on *vanilla* Dusk, which required keeping every
> engine change ABI-safe (inline, no-data, no-virtuals accessors). **That
> constraint is dropped.** TP Combat is now its own **custom Dusk fork**
> (`dusk` branch `heros-shade`, pinned at v1.0.0) shipped *together with* its
> engine build — engine-source edits are permitted for new work; existing
> API-based code stays as-is.

That standalone constraint bought portability (the `.dusk` ran on any Dusk
build), but it cost a lot of friction. Several features could only be
approximated mod-side — re-synthesizing engine code paths through hooks,
clamping private fields we couldn't read cleanly, working around symbols
that `dusk.lib` didn't export. The friction came to a head during the
v1.3.1 engine migration, which broke a cluster of mod-integration surfaces
(shop input, Mods-tab buttons, roll-while-targeting) that only the API
relied on.

## Why it changed: ease of use and customization

On **2026-06-18** the project reverted to a known-good **v1.0.0 base** and
adopted a new philosophy (full rationale in
[`revert-to-v1.0.0-and-custom-fork-philosophy-design.md`](../../tp-combat-mod/docs/superpowers/specs/2026-06-18-revert-to-v1.0.0-and-custom-fork-philosophy-design.md)):

- TP Combat targets a **custom Dusk fork**, not stock Dusk. Distribution is
  **the mod + our Dusk build together**, not a drop-in `.dusk` for arbitrary
  Dusk versions.
- Because the fork is **PC-only**, the constraints that drove the old
  workarounds no longer bind new work: the PowerPC-layout-matching rule and
  the "ABI-safe inline accessors only" rule are **lifted for new code**.
- New features and fixes **may edit engine source directly** — real hooks,
  accessors, behavioral edits, even struct-layout changes — whenever that is
  cleaner than fighting the mod API.

The net effect is *ease of use* (a private field becomes a one-line
accessor instead of an offset hack) and *more customization* (behaviors the
API can't reach are now fair game).

## The split today

> **Don't fix what isn't broken.** The engine-edit freedom is a tool for
> *new* additions and for cases the API genuinely can't serve — not a mandate
> to rewrite working code.

| | Mod API (`.dusk`) | Engine source (fork) |
|---|---|---|
| **Lives in** | [`tp-combat-mod/`](../../tp-combat-mod/) `src/` + `res/` | [`dusk/`](../) engine source, branch `heros-shade` |
| **Ships as** | `tp_combat.dusk` archive | `dusk.exe` + `dusk.dll` |
| **Holds** | The **bulk of the combat system** — stun gauge, hit-counter hitstun, per-enemy stat/animation tuning, the shop, evasion i-frames, placements, HUD, audio, etc. | Engine-level changes that the API can't express cleanly: new hookable entry points, `TARGET_PC` accessors, room-load actor hooks, behavioral combat fixes, debug overlays. |
| **Built with** | [`modding.md`](modding.md) (`add_dusk_mod`, `dusk::hook*`, `DuskModAPI`) | [`building.md`](building.md) (CMake presets, `dusk_game` static lib) |

**The bulk of the combat system deliberately stays on the API** so that all
the existing modding keeps working — the hook/poll adapters, the Mods-tab
tuning UI, the bundled `placements.json`, and the save sidecar are all
mod-side. Engine edits are reserved for the cases where reaching through the
API would mean re-implementing engine internals.

Examples of changes that have already gone into **engine source** on the
fork rather than the mod:

- `dStage_onRoomActorsReady` — a per-room "actors are ready" hook the
  placement system uses to spawn at room-load with no fixed delay.
- `TARGET_PC` accessors on `daE_OC_c` (`getSphsAt`, `getMorf`,
  `getWaitTimerRef`, `getDamageCooldownRef`) and on `dKantera_icon_c`
  (`getGaugePane` / `getParentPane`) that expose private fields the combat
  hooks need.
- Combat fixes (roll 1-frame cancel, tear-of-light glow latch) and a
  Level Info debug overlay.

Everything else — and that is most of the project — is mod-side hooks and
polls against those entry points.

## Deciding where a change goes

When you pick up a new feature or fix, ask in order:

1. **Can a hook or poll on an existing engine symbol do it?** Then it's
   mod-side. Add it under `tp-combat-mod/src/` and wire it into `mod.cpp`.
   This is the default — prefer it.
2. **Does the API path require re-implementing engine internals, reading a
   private field by offset, or a symbol `dusk.lib` doesn't export?** Then a
   small **engine-source** change (an accessor, a new hookable function) is
   the clean fix. Make it on the `dusk` fork.
3. **Is it a genuine engine behavior change** (the vanilla code path itself
   is wrong for our mod)? Then edit engine source directly.

Two-repo changes (a mod feature that needs a new engine accessor) land as
**two PRs — the engine PR first**, because the mod build links against the
fork. See [`GitWorkflow.md`](../../GitWorkflow.md).

## See also

- [`modding.md`](modding.md) — the Dusk mod API: exports, hooks, resources,
  ImGui, inter-mod services. The surface the mod-side bulk is built on.
- [`building.md`](building.md) — building the engine (and, transitively, the
  mod, which links the `dusk_game` static lib).
- [`TP-Combat-Overview.md`](../../tp-combat-mod/docs/TP-Combat-Overview.md) —
  what the combat mod actually does, feature by feature.
- [`TP-Combat-Distribution.md`](../../tp-combat-mod/docs/TP-Combat-Distribution.md)
  — the historical ABI/standalone analysis (kept as reference for the old
  constraint).
- [`GitWorkflow.md`](../../GitWorkflow.md) — how changes move through the two
  repos via PRs.
