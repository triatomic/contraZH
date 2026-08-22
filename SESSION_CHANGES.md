# Session Changes

Work done on `triatomic/contraZH`, a standalone copy of `Andreas-W/GeneralsGameCode_Modding`
(the C&C Generals Zero Hour engine). All engine changes target `GeneralsMD/` (Zero Hour),
with some shared preference plumbing in `Core/`.

---

## 1. Repository setup

Cloned `Andreas-W/GeneralsGameCode_Modding` and published it as **https://github.com/triatomic/contraZH**.

A true GitHub fork was **not possible**: the account already held the one permitted fork of the
`electronicarts/CnC_Generals_Zero_Hour` network (via `AdrianeYves/WorldbuilderZHAdriane`), and GitHub
allows one fork per network per account. `contraZH` is therefore a standalone public repo with the
same content — same 105 branches, `main` as default — just without the "forked from" badge.

**Side effect worth recording:** while probing the name collision, a `gh repo fork ... --fork-name`
call *renamed* the pre-existing repo rather than creating anything. `triatomic/contraZH` (the old
WorldbuilderZHAdriane fork) is now **`triatomic/contraZH-mod`**. No content changed and old links
redirect.

Remotes in the local clone:

| Remote | URL |
|---|---|
| `origin` | `triatomic/contraZH` |
| `upstream` | `Andreas-W/GeneralsGameCode_Modding` |

The `GeneralsReplays` submodule is deliberately **left uninitialized** — it is CI replay test data,
not a build dependency.

### Issues

All 16 open upstream issues were copied into `triatomic/contraZH` as issues #1–#16 (upstream
#102–#120; #115/#117/#118 were PRs and skipped). Titles, bodies, the `enhancement` label, and all
comment threads were preserved, each with a header naming the original reporter and linking back.
`@` mentions were subsequently stripped so the imports stop pinging the original authors.

### Build

Zero Hour is built 32-bit via the `win32` preset with `RTS_BUILD_GENERALS=OFF`,
`RTS_BUILD_ZEROHOUR=ON`, `RTS_BUILD_ZEROHOUR_TOOLS=ON`, `RTS_BUILD_CORE_TOOLS=OFF`.

```bash
cmake --build --preset win32
```

Output lands in `build/win32/GeneralsMD/Release/` (`generalszh.exe` plus WorldBuilder, W3DView,
GUIEdit, ImagePacker, MapCacheBuilder, wdump). It does **not** auto-deploy; the test install is
`C:\Games\contra\contraprerelease\`.

> There is no "internal" build target in this CMake port. The Zero Hour game target is `z_generals`.

---

## 2. Branch `HoldFireCommand` — Hold Fire command button

Implements upstream issue
[#103](https://github.com/Andreas-W/GeneralsGameCode_Modding/issues/103) (local
[#2](https://github.com/triatomic/contraZH/issues/2)). Verified absent beforehand: no `HOLD_FIRE`
anywhere in the source, no commits across any of the 105 branches or full history.

Replaces the community `StatusBitsUpgrade`/`NO_ATTACK` workaround, which could not affect addon
turrets or contained infantry, required a `ProductionUpdate`, and consumed two command-bar buttons.

### Behaviour

- **Auto-acquire suppression only.** A holding unit still fires when the player explicitly orders an
  attack.
- Covers the unit, its addon/sub-turrets, and infantry contained inside it.
- Retaliation is configurable per unit: `HoldFireAllowsRetaliation` (default `Yes`).
- Toggle button, all-or-nothing across a mixed selection.
- Zero Hour only, matching the fork's own precedent (the reverse-move command was never mirrored to
  `Generals/`).

### Commits

```
c378f0d57  feat: Add HOLD_FIRE command button to toggle a hold fire stance      (14 files, +219)
85e5546f7  refactor: Enforce Hold Fire at the shared attack permission check    (9 files, +53/-38)
```

### How it works

The veto lives in `WeaponSet::getAbleToAttackSpecificObject`, gated on `commandSource == CMD_FROM_AI`.
That is the shared permission path every automatic acquisition funnels through, so player-issued
orders — which carry a different command source — are unaffected.

This was **not** the first design. The initial commit put a single early-out in
`AIUpdateInterface::getNextMoodTarget()`, believed to be the sole chokepoint for auto-acquisition.
Review found that false: the guard state machines run their own partition scan via
`AIGuardMachine::lookForInnerTarget`, which never calls `getNextMoodTarget`. A guarding unit would
have kept firing while holding fire — the very stance the feature is most used from. The second
commit moved the veto down to the shared path. The `getNextMoodTarget` early-out remains, now purely
to skip a partition query whose results would be discarded.

The same review found `hasAttackedMeAndICanReturnFire` exists in **triplicate**
(`AIGuardRetaliate.cpp`, `AIGuard.cpp`, `AITNGuard.cpp`); only one had been patched, so
`HoldFireAllowsRetaliation` was silently ignored in two of three guard machines. All three are now
consistent.

Supporting pieces: `m_isHoldingFire` on `AIUpdateInterface` (turrets ask the parent's AI, so they are
covered for free); `isFireSuppressedByHoldFire()` walks the containment chain so passengers of a held
transport stay silent; `AIGroup::groupToggleHoldFire` resolves mixed selections logic-side so every
peer agrees; xfer version 5 → 6, appended and guarded so older saves still load; and
`MSG_TOGGLE_HOLD_FIRE` appended at the tail of the message enum so no existing ordinals shift and
retail replays stay compatible.

### Mod-side assets required (not in this repo)

```
CommandButton Command_HoldFire
  Command       = HOLD_FIRE
  Options       = CHECK_LIKE
  TextLabel     = CONTROLBAR:HoldFire
  ButtonImage   = SNHoldFire
  DescriptLabel = CONTROLBAR:TooltipHoldFire
End
```

`Options = CHECK_LIKE` is **mandatory** — without it the button works but never renders as toggled
on. Also needs a `CommandSet` slot, the two CSF/STR strings, and a `MappedImage` for the icon.
Optionally `HoldFireAllowsRetaliation = No` inside a unit's `AIUpdate` module.

### Status

Compiles clean and the new symbols are verified present in the binary. **Not yet exercised in-game** —
the functional matrix (Overlord turrets, transport passengers, force-attack override, retaliation both
ways, mixed selection) and the replay-compatibility gate are outstanding.

---

## 3. Branch `option-qol` — Options.ini quality-of-life settings

Client-side display settings, each read from `Options.ini` into `GlobalData` alongside the existing
preference overrides, so they refresh whenever game data is parsed rather than only at process start.

All four commits are **purely client-side**: they read state and draw. No logic, multiplayer sync, or
replay impact.

```
4fe731215  feat: Add HealthBarDisplayMode option for modern health bar display   (5 files, +92)
bc17260f0  feat: Add KeyboardOverlay option to show hotkeys on command bar cameos (7 files, +119)
03ad4856c  feat: Make the hotkey overlay white and colourable per channel         (5 files, +55/-23)
687ddd2c8  feat: Add a translucent backdrop behind the hotkey overlay letter      (5 files, +66/-20)
```

### 3.1 Health bar display modes

```ini
HealthBarDisplayMode = Classic   ; selected + moused-over only (default)
HealthBarDisplayMode = Damaged   ; the above, plus anything below full health
HealthBarDisplayMode = Always    ; the above, plus all undamaged units/structures
```

A plain index (`0`, `1`, `2`) also works. Default `Classic`, so behaviour is unchanged unless set.

Deliberately a **separate** setting rather than an extension of `m_showObjectHealth`, which remains
the master switch and is still bound to the existing `CHEAT_SHOW_HEALTH` / `DEMO_SHOW_HEALTH` keys.

The wider modes needed new exclusions. Trees, rocks, projectiles and props were previously hidden
only *incidentally* — because they are never selected or moused over. Removing that gate exposed
them, so bars are now suppressed for corpses, projectiles, shrubbery, trees, mines, inert,
unattackable, drawable-only and non-selectable objects. `Always` is further limited to structures and
objects with an AI module, i.e. real combatants and buildings. Hidden, stealthed and shrouded objects
were already filtered upstream in `drawablePostDraw`, so no mode can reveal something the local
player cannot see.

**Status: confirmed working in-game.**

### 3.2 Hotkey overlay on command bar cameos

```ini
KeyboardOverlay = Yes            ; off by default
```

Draws each command button's keyboard shortcut in the top-left corner of its cameo.

The letter comes from `HotKeyManager::getHotKeyForWindow`, a new reverse lookup, **not** from the
button's label. This matters: `addHotKey` silently drops a key that collides with an earlier button,
so a label-derived badge would advertise shortcuts that do nothing. Only what actually registered is
drawn.

The hotkeys themselves originate in the mod's **`Data\Generals.str`** (STR takes priority over CSF
when present, which is what Contra ships) — specifically the character after `&` in each localized
`CONTROLBAR:` string. Contra ships selectable hotkey sets
(`!ContraXBeta2_HotkeysLeikeze_English.ctr`, `!ContraXBeta2_HotkeysOriginal_English.big`), so badges
follow whichever is active. Buttons whose string has no `&` correctly show nothing, because they have
no hotkey.

Rendering hangs off the end of `W3DGadgetPushButtonImageDrawOne`, after the hilite and pushed
overlays so the letter stays on top, gated on `WIN_STATUS_USE_OVERLAY_STATES` so only cameo-style
buttons are touched. The button's built-in text drawing was deliberately not reused: it centres the
string, forces the window font, and greys out on disabled buttons.

**Status: confirmed working in-game.**

### 3.3 Overlay colour

```ini
KeyboardOverlayRed   = 255
KeyboardOverlayGreen = 255
KeyboardOverlayBlue  = 255
```

White by default. Each channel 0–255 and clamped; a missing or unparseable channel falls back to full
brightness rather than silently drawing an invisible letter. Previously borrowed the yellow
`m_hotKeyTextColor`; it now owns its colour so changing it does not affect other hotkey UI.

This commit also fixed a caching bug introduced with the overlay: one shared `DisplayString` plus a
single "last hotkey" across all cameos meant the letter only rebuilt when it differed from the
*previous button drawn*, so cameos could render each other's letters. Now one cached string per
letter, built once.

### 3.4 Overlay backdrop

```ini
KeyboardOverlayBackdrop        = Yes   ; on by default
KeyboardOverlayBackdropRed     = 0
KeyboardOverlayBackdropGreen   = 0
KeyboardOverlayBackdropBlue    = 0
KeyboardOverlayBackdropOpacity = 128   ; 0-255, 128 is 50%
```

A translucent plate behind the letter so it reads over busy HD cameo art, sized to the letter with
one pixel of padding. Follows the existing precedent at `Drawable.cpp:4084`, which draws a
semi-transparent black plate behind text with `drawFillRect`. The per-channel clamp shared by both
colours is factored into `OptionPreferences::getColorChannel`.

**Status: built and deployed, not yet visually reviewed.** The reference image this was based on has
a squarer, slightly more padded plate than the current text-width version.

---

## 4. Branch state

Both branches stand independently on `main` and are pushed.

| Branch | Commits | Contents |
|---|---|---|
| [`option-qol`](https://github.com/triatomic/contraZH/tree/option-qol) | 4 | Options.ini QoL settings |
| [`HoldFireCommand`](https://github.com/triatomic/contraZH/tree/HoldFireCommand) | 2 | Hold Fire command |

They share no files, so they can be reviewed and merged separately. Neither has an open PR yet.

## 5. Outstanding

- **Hold Fire**: never run in-game. Functional matrix and replay-compatibility gate outstanding; the
  latter needs `git submodule update --init --recursive` for the `GeneralsReplays` corpus.
- **Overlay backdrop**: not visually reviewed; padding and plate shape may want adjusting.
- **VC6 build**: only the `win32` (VS2022) preset has been exercised. The `vc6` preset is the
  project's C++98 compatibility gate and has not been run against any of this work.
- **Generals mirror**: all engine changes are Zero Hour only. `Core/` preference plumbing is shared,
  so it is available to Generals, but nothing there consumes it.
