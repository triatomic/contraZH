# GenOnline port notes

This tree holds the GenOnline online stack, the Generals Online team's 2026.11 client, ported into contraZH. It replaces the earlier port of GeneralsOnlineDevelopmentTeam/GameClient that lived in `GeneralsMD/.../GameNetwork/GeneralsOnline/`.

## Provenance

| Item | Value |
|---|---|
| Source | `genOnline` remote (community-outpost/genonline-client), branch `release/2026.11`, commit `befb25f5f9` |
| Source's TheSuperHackers base | `998f337e67` (2026-11-09) |
| Ported on | branch `feat/genonline-port`, from main `96bdf583a6` |
| Pre-port state | tag `backup/main-pre-genonline-port`; its `PORT_NOTES.md` records the old port's decisions |
| Backend | `https://api.generals.online` (`/api/v1`, realtime hub); the old `api.playgenerals.online` is not used |

## Rules

- `RTS_BUILD_GENERALS_ONLINE` (default OFF) gates everything. ON defines `GENONLINE` for every target and pulls the hooks in `GenOnline/cmake/`.
- The OFF build compiles the fork's code unchanged. Every GenOnline change to a file that existed before the port sits in `#if defined(GENONLINE)`, with the fork's lines on the other side.
- GenOnline's own files are imported verbatim: this directory, `Include/GameNetwork/GenOnline/`, `Menus/GenOnline/`, the top-level `GenOnline/` directory, and about 108 other new files under `Core/`. Edits to them are listed below and should stay few.
- On the `GENONLINE` side, code that decides bytes on the network or simulation results follows GenOnline exactly, because the client plays lockstep against official clients. Client-side code keeps the fork's behaviour with GenOnline's change on top.
- GenOnline defines `GENONLINE` unconditionally and guards little: 85% of its changed lines in upstream files were outside any guard. The guards in this tree were added by the port.
- Vanilla `Generals/` is not ported.

## Build

- ON tree: `build/win32-genonline`, configured by `build/genonline-port/configure_on.bat`. It needs the vcpkg toolchain, triplet `x86-windows-static-md` (in `triplets/`), and the overlay ports in `GenOnline/cmake/ports`, which `cmake/config-vcpkg.cmake` adds only when the option is ON.
- `vcpkg.json` holds GenOnline's dependencies in the `genonline` feature and uses GenOnline's baseline, so the vcpkg checkout must be at or past 2026-09-23.
- Configured with `RTS_BUILD_OPTION_MODERN_MEDIA=OFF`: Miles and Bink only, no OpenAL or FFmpeg.
- Configured with `GENONLINE_ENVIRONMENT=test`, so the client uses `https://api-test.generals.online`. Left empty, GenOnline picks the service from the git tag: test for a `vYYYY.MM.N-rc.N` tag, live (`https://api.generals.online`) for anything else, which is what the fork's tags give. `network.environment` in `settings.client.json` overrides both.
- `.gitattributes` keeps the overlay port patches LF; `git apply` rejects them as CRLF.
- The exe keeps the fork's name (`generalszh`, `generalszh_c` with cheats).
- The data pack is not built by CMake: `python GenOnline/tools/pack_go_data.py zerohour 2026.11.0 <exe folder>\data contra`. The last argument overlays `GenOnline/data/contra`, the Contra colour theme, on the Zero Hour files.
- The Contra theme is the `contra` entry of `THEMES` in `GenOnline/tools/go_ui_kit.py` plus `GenOnline/data/contra/Data/INI/GOUIColors.ini`. After a palette change run `python GenOnline/tools/go_ui_kit.py --theme contra` and repack.
- The ON build runs the logic at 60 Hz in every mode and has its own exe CRC, so it cannot share LAN games or replays with the OFF build.

## Re-syncing with GenOnline

Tools and logs are under `build/genonline-port/` (gitignored); `PLAN.md` there has the full record.

1. Strip the current guards: resolve `GENONLINE` as undefined (`helpers/ifstrip.py`, `strip_tree.py`).
2. Re-import the verbatim directories with `git checkout <ref> -- <path>`, then re-apply the imported-file edits below.
3. `helpers/guard_merge.py` diffs the new base against the new tip per file, drops whitespace and comment hunks, wraps the rest in guards, and three-way merges into our file. It widens a guard that would cut a conditional, a continued line, a block comment or an open parenthesis.
4. Resolve conflicts by hand, then `guard_merge.py --check-resolved <path>`: with `GENONLINE` undefined the file must equal its pre-merge content.
5. `helpers/on_audit.py` compares the `GENONLINE` view with GenOnline's file and reports differences that the fork's own changes do not explain. Watch for blocks GenOnline reindented: the fork's edits inside them land only on the non-`GENONLINE` side.

## Fork policy

Switches are in `Include/GameNetwork/GenOnline/GenOnlineForkPolicy.h`; CMake options are in `cmake/config-build.cmake`.

| Behaviour | Control | Default |
|---|---|---|
| Download and launch GenOnline's patcher | `GENONLINE_FORK_SELF_UPDATE` | 0 |
| Send machine GUID, MAC, volume serial; list loaded modules | `GENONLINE_FORK_HW_FINGERPRINT` | 0, fields sent empty |
| Ignore Options.ini texture filter and anisotropy | `GENONLINE_FORK_NO_TEXTURE_FILTER_OPTIONS` | 0 |
| Crash reports to GenOnline's server | cache `GENONLINE_SENTRY_DSN` | empty, reporting off |
| VC++ runtime check that can download the redistributable | option `GENONLINE_FORK_VCRUNTIME_CHECK` | OFF |

- GenOnline's FPS limit, camera heights, camera factors and adaptive quality apply only inside an online match. `GenOnlineForkMatchSettings.{h,cpp}` saves the fork's values at launch and restores them after the match.
- GenOnline's fixed keys are meta commands in `CATEGORY_GO`: `TOGGLE_NETWORK_STATS` (Shift+F10, cycles off, basic, detailed), `TAKE_SCREENSHOT_ALT` (Shift+F11), `TOGGLE_BUDDY_OVERLAY` (Shift+F5), `TOGGLE_BUDDY_OVERLAY_ALT` (Insert).
- Archives mount through the fork's registry with GenOnline's priorities, so the interface style switch rebuilds the same tree. Lookup order: loose files, GenOnline menu layouts, mod archives and game-folder archives, data packs, retail archives.
- `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance` come from `GenOnlineInit.cpp`; `WinMain.cpp` skips `DedicatedGpuExports.h` on the `GENONLINE` side.

## Imported files edited

| File | Edit |
|---|---|
| `GenOnlineDefines.h` | includes `GenOnlineForkPolicy.h`; texture filter switch |
| `GenOnlineInit.cpp`, `GenOnlineMainMenuUtils.cpp` | self-update switch |
| `GenOnlineAuth.cpp`, `GenOnlineRoomsInterface.cpp` | fingerprint switch; Shift+F5 hint text |
| `GenOnlineGame.cpp` | saves the fork's render settings at match launch |
| `GenOnlineSocialInterface.cpp` | Shift+F5 hint text |
| `Common/AdaptiveQuality.cpp` | acts in an online match only |
| `GenOnline/tools/go_ui_kit.py` | `contra` theme entry |
| `GenOnline/tools/pack_go_data.py` | optional overlay folder argument |
| `Menus/GenOnline/GenOnlineLoginMenu.cpp`, `GenOnlineWelcomeMenu.cpp` | `WebBrowser.h` include and the unreachable browser branches removed (TSH #3691 deleted the embedded browser) |

## Open

- Never run. Sign-in, lobby and match against the live service are untested.
- The client signs in as `genonline_zerohour`, version `2026.11.0`, with the fork's exe CRC and Contra's INI CRC. Whether the service admits it is unknown.
- The service can require anti-cheat per lobby; the built-in one is a stub that reports identifier 0.
- The data pack replaces the mod's shell menus (`MainMenu.wnd`, `OptionsMenu.wnd`, skirmish, LAN and replay menus) with GenOnline's. `ui.style: "retail"` in `settings.client.json` drops GenOnline's layouts; which setting suits Contra is undecided.
- Fork features still active on the `GENONLINE` side inside simulation code, each a possible desync against official clients: `TheCommandSequence->update()` in `GameLogic.cpp`, the `countKill` gate and OnKill modules in `Object.cpp`, `aiAttackPosition(self->getPosition())` in `FireWeaponPower.cpp`, the production speed multiplier in `Upgrade.cpp`, and INI-gated changes in `AIGroup.cpp` and `AIStates.cpp`.
- `NETWORK_PROTOCOL_VERSION` is the same in both builds although the ON side disables compressed map transfer.
- Server probe requests still trigger screenshot uploads, and replays and stats upload after online matches.
- Per-region notes from the merge: `build/genonline-port/p3/resolve_*.md`, `p4/fixes.md`, `p5/policy.md`, `p5/archives.md`.
