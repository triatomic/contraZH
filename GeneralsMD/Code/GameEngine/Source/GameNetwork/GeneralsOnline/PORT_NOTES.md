# Generals Online port notes

This directory holds the online-system replacement ported from
GeneralsOnlineDevelopmentTeam/GameClient ("GO"), a TheSuperHackers-derived fork that
replaced GameSpy with a modern stack (REST + WebSocket backend, Valve
GameNetworkingSockets ICE P2P transport, lobbies/matchmaking/stats).

## Provenance

- Ported from GO commit: `d7f75517dd76c3a4e3de75e988f328a6877621c5` (2026-08-24)
- GO's merge base with TheSuperHackers: `e760b3695` (2026-07-26)
- contraZH's merge base with TheSuperHackers at port time: `5943d3856` (== tsh HEAD)
- Future re-syncs: `git fetch go && git diff -w <old-go-sha>..<new-go-sha> -- <ported
  paths>`, applied per the rules below.

## Port decisions

1. Online system only. GO's engine-feel fork (widescreen, 60 Hz sim, frame pacing,
   renderer/particle/memory changes) and its unguarded gameplay/balance edits are NOT
   ported.
2. GameSpy stays. Everything is gated behind the CMake option
   `RTS_BUILD_GENERALS_ONLINE` (default OFF). The OFF build must stay byte-identical in
   behavior; every in-place edit to a shared file must sit inside
   `#if defined(GENERALS_ONLINE)`.
3. Telemetry off by default: Sentry behind `GENERALS_ONLINE_SENTRY`, hardware
   fingerprinting behind `GENERALS_ONLINE_HW_FINGERPRINT`, anti-cheat behind
   `GENERALS_ONLINE_USE_PLUGINS_INTERFACE` — all undefined by default, kept in the
   protocol so official-server coordination can re-enable them.
4. Exe name stays `generalszh`; GO's rename is not taken.

## Merge rules (apply to every file brought over)

- Diff GO with `-w` always: its tree carries heavy CRLF churn and `nullptr`->`NULL`
  reversions of TSH modernization. Discard modernization-revert hunks.
- `#else` (non-GO) branches always keep contraZH's text, never GO's — GO's dead branches
  contain code that does not compile (e.g. `m_fpsAverages` in ConnectionManager.cpp).
- Every GO include added to a Core/ file must be wrapped in
  `#if defined(GENERALS_ONLINE)` and use a full `GameNetwork/GeneralsOnline/...` path
  (GO's unguarded relative includes break the Generals (vanilla) build).
- UI screens are dual-copied (GO's versions live in `.../Menus/GeneralsOnline/`), never
  merged in place: GO rewrote them with direct NGMP calls that cannot compile against
  GameSpy.
- `InGameUI.{h,cpp}` are never merged from GO — adapt GO call sites to contraZH's API.
- Blind file copies of any GO-touched gameplay file are forbidden — GO's balance changes
  are mostly unguarded.

## Decisions made during the port (deviations from upstream GO)

- **30 Hz client**: `GENERALS_ONLINE_HIGH_FPS_SERVER` is commented out (the 60 Hz
  simulation is part of GO's engine-feel fork, not ported). Client id is
  `gen_online_30hz`; `GENERALS_ONLINE_HIGH_FPS_LIMIT` resolves to 30. All
  `GENERALS_ONLINE_HIGH_FPS_*` / `_RUN_FAST` hunks in shared files were dropped.
- **Texture filtering kept**: `GENERALS_ONLINE_DISABLE_TEXTURE_FILTERING_AND_AA` is
  commented out and the W3DDisplay hunk that forces MSAA off was not taken - this fork
  has its own TextureFilter/AnisotropyLevel feature.
- **Sentry**: gate reuses GO's own `GENERALS_ONLINE_USE_SENTRY`, now default-off and
  extended to cover `InitSentry`'s body, `ShutdownSentry`, and the lib pragma (upstream
  ran sentry_init unconditionally in release builds).
- **Fingerprinting**: new `GENERALS_ONLINE_HW_FINGERPRINT` (default-off) gates machine
  GUID / MAC / volume serial in the auth payloads and the loaded-module report on the
  WS keepalive; fields are sent empty so the wire shape is unchanged.
- **Version/branding**: `version.cpp` product title/version hunks NOT taken (cosmetic;
  the backend gets `GENERALS_ONLINE_VERSION_STRING` directly from OnlineServices_Init).
  WinMain's `TheVersion->setVersion` GO variant IS taken (guarded) since the network
  version fields matter for matchmaking.
- **W3DView::setDefaultView**: takes GO's 4-arg signature under the macro for compile
  compatibility, but the body keeps this fork's camera behavior - GO's settings-driven
  camera min/max heights (their settings.json camera section) are not applied.
- **GO headers made self-sufficient**: upstream GO force-includes `PreRTS.h` into every
  TU via CMake PCH, which transitively provides `NextGenMP_defines.h`. This port keeps
  contraZH's PCH setup, so `NextGenMP_defines.h` is included explicitly where needed
  (GeneralsOnline_Settings.h, NGMPGame.h, ConnectionManager.h).
- **Deferred to the UI phases**: `SetLookAtPlayer` signature change
  (PersistentStorageDefs.h - would break the unported WOL menus when ON),
  `GameSpyOpenOverlay` buddy bypass, StatsExporter/StatsUploader plus their
  CommandLine (-exportStats/-statsUrl/-disableCommunityDataPatch), GameMain validation
  and GlobalData fields.
- Log-only hunks (NetworkLog conversions of commented DEBUG_LOGs), `isspace/isdigit`
  cast fixes, `nullptr`->`NULL` reverts, and GO's dead `#else` branches were not taken.

### Open: authenticated session is refused by the live service (as of 2026-08-26)

Where the bring-up currently stands against `api.playgenerals.online`:

- **Working**: `VersionCheck`, `MOTD`, the whole browser login flow. `CheckLogin` returns
  `result: 1` with a well formed JWT (`sub: 112094`, roles `Player`/`GameClient`,
  `iss: GeneralsOnline`, `aud: GeneralsOnline-Client`). The GO team confirmed the user
  row exists in their database, so login genuinely succeeds server side.
- **Failing**: every *authenticated* call is answered `401 invalid_token` - the
  `wss://api.playgenerals.online/ws` upgrade, `ServiceConfig`, and `RefreshToken` -
  using a token 18 seconds old with ~14 minutes of validity left. Ruled out by
  experiment: header format (`Bearer`/`bearer`/raw), token as `?access_token=` query
  param, `contract/1` vs `contract/2`, user agent, and rate limiting (unauthenticated
  endpoints answer 200 from the same machine at the same moment). The request shape
  matches the shipping official client, verified by comparing strings in
  `GeneralsOnlineZH_60.exe`.
- **Leading theory** (unproven - only GO's server logs can confirm): the session is only
  honored once the anticheat handshake completes. Their `OnLoginComplete` calls
  `AnticheatPlugInterface::Authenticate()` immediately before connecting the WebSocket;
  this fork compiles the plugin interface out, so that never runs. Their current
  anticheat is **GOAC** (`plugins/goanticheat/goanticheat.dll`), not EAC - the EAC
  references in their repo are historical. Alternative suspects visible in the token:
  `client_id: "0"` and an empty `display_name`.

Next step is a question for the GO team, not a code change - see
`FOR_GENERALSONLINE_DEVS.md` outside the repo. Note that enabling GOAC means loading a
closed source anticheat DLL into a Contra build, which is a deliberate decision, not a
default.

### Login flow: the repo is AHEAD of production

GO commit `27da5b89d` ("- New login flow", 2026-08-22) introduced a server issued login
code via a new `LoginCode` endpoint. Production does not serve it (hard 404) and the
shipping official client does not contain the string at all - it still uses the older
handshake: generate the gamecode client side, open the browser, poll `CheckLogin` with
`{code, client_id}`. The `pre-newserver` branch reverts
`OnlineServices_Auth.{h,cpp}` to `27da5b89d^` for that reason; `feature/go-port` keeps
the newer flow for when production catches up.

### Vendor DLL fix (found by crash triage)

Upstream GO's vendored `Vendor/ValveNetworkingSockets/abseil_dll.dll` (4.3 MB) does
not match the abseil the vendored `libprotobuf.dll` was built against - loading it
corrupts the heap inside protobuf's static initializer (0xc0000374 in `DllMain`,
before WinMain). GO never noticed because their POST_BUILD copies only
discord-rpc.dll; the working DLL set comes from their patcher. The vendored copy is
replaced here with the 1.8 MB `abseil_dll.dll` an official GeneralsOnline install
ships (byte-verified against `libprotobuf.dll`/`GameNetworkingSockets.dll`, which are
identical between the repo and the official install). Note the official install also
carries a 64-bit `zlib1.dll` for other tooling - do NOT take that one; the repo's
x86 `zlib1.dll` is correct for the game.

### UI phase decisions

- **Dual copies** live in `GUICallbacks/Menus/GeneralsOnline/`; CMake swaps them for the
  originals when the option is ON. Ported so far: WOLLoginMenu, WOLWelcomeMenu,
  WOLLobbyMenu, WOLGameSetupMenu, WOLMapSelectMenu, PopupHostGame, PopupJoinGame.
- **LobbyUtils.cpp** uses a whole-file dual instead of ~20 guarded hunks: the Core
  implementation is wrapped in `#if !defined(GENERALS_ONLINE)` and GO's rewritten copy
  compiles from the GO tree when ON. The shared LobbyUtils.h carries guarded enums.
- **W3DListBox** row-entry animation: when OFF, `rowDrawY` is a const alias of `drawY`
  (the one deliberate shared-text change that is not inside an `#if`; it is
  behavior-identical). GO's hardcoded green listbox background fill was NOT taken
  (visual restyle), nor were its unguarded null-check hunks in GadgetListBox.
- **GeneralsOnline_UIStubs.cpp** temporarily defines showNotificationBox,
  updateBuddyInfo and the GO-signature SetLookAtPlayer until WOLBuddyOverlay and
  PopupPlayerInfo are ported; delete the stubs with that phase.
- The GO menus' `#include "../X.h"` lines resolve through the Vendor `-I` entry
  (`Vendor/../` = the GeneralsOnline dir) - kept verbatim for diffability with GO.
- GO's menus reference only retail `.wnd` layout names; GO ships updated layouts as
  loose files under `GeneralsOnlineGameData\` which winCreateFromScript prefers when
  ON. Obtain that directory from a GO install/patcher for the full experience.
- **InGameChat gate adapted, not adopted**: an active NGMP game additionally allows
  the chat window; upstream GO allowed it ONLY then, which would have broken LAN chat
  and this fork's singleplayer chat-command window.
- **Skipped in the UI phases**: OptionsMenu hunks (observer-overlay font needs GO's
  InGameUI, texture-filter guard is inert here), SkirmishGameOptionsMenu's team-colored
  start positions (unguarded, and drops a bounds check), PopupReplay's null-check,
  Diplomacy's GO_REVEAL_TEAMS block (macro never defined), and the **StatsExporter /
  StatsUploader replay-analytics pipeline** (its hooks live in Player.cpp/Object.cpp
  gameplay code; server-side stats are already covered by OnlineServices_StatsInterface).

## Fixes

Everything below was found after the port compiled, by running the client against the
live service. They fall into three groups, and the second one is a pattern worth
knowing before hunting the next crash.

### 1. Environment / vendored binaries

| Fix | Symptom | Cause |
|---|---|---|
| Replace vendored `abseil_dll.dll` | Process died before `WinMain` (`0xC0000374`) | The repo's abseil does not match the abseil its `libprotobuf.dll` was built against. See the vendor DLL section above. |

### 2. GameSpy singletons that GO redirects to NGMP (the recurring pattern)

**With the GO stack active, `SetUpGameSpy()` is compiled out, so `TheGameSpyInfo` and
`TheGameSpyGame` are never created.** Upstream GO rewrote every site that reads them to
use the NGMP equivalents instead. Where the port took a header change but missed the
matching body, the result is a null dereference that only appears once the UI flow
actually reaches that screen - so these surfaced one at a time, going deeper into the
online flow with each fix.

| Fix | Crash site | Redirect applied |
|---|---|---|
| Preference constructors (`Custom`, `QuickMatch`, `GameSpyMisc`, `Ignore`) | `CustomMatchPreferences::CustomMatchPreferences+0x7e` via `PopupHostGameInit` - clicking Host or Quick Match | `TheGameSpyInfo->getLocalProfileID()` -> NGMP auth user id, stored under `GeneralsOnlineData\` |
| `LoadScreen::didPlayerPreorder` | latent, would crash during an online match load | forced to `false` when GO is active |
| `GameLogic` game-info selection | `GameSpyLoadScreen::init+0xce` via `tryStartNewGame` - launching a hosted match | `TheGameInfo = TheGameSpyGame` -> `TheNGMPGame` for `GAME_INTERNET` |
| `Recorder` stats path | latent (`RTS_DEBUG` + `m_saveStats`) | same redirect, guarded on the services manager existing |

**How to diagnose the next one.** The game writes its own dumps to
`Documents\Command and Conquer Generals Zero Hour Data\CrashDumps\` as
`CrashMZ-<date>-<commit>-pid<n>.dmp` (mini) and `CrashFZ-...` (full, ~1 GB) - *not* to
the Windows WER folder. The mini dump plus the deployed `generalszh.pdb` is enough:

```
cdb.cmd -z "<CrashMZ dump>" -y "C:\Games\contra\contraprerelease;srv*<symcache>*https://msdl.microsoft.com/download/symbols"
.ecxr
kb 40
```

A null read of the form `mov eax,dword ptr [ecx] ds:002b:00000000` inside a function
whose name starts with `GameSpy` is almost certainly another instance of this pattern:
find the same function in the GO clone, take its `#if defined(GENERALS_ONLINE)` branch.

### 3. Behavior differences from running a fork against the official service

| Fix | Reason |
|---|---|
| Report `anticheat_id = -1` (`NONE`) | Upstream's stub returns `0`, which is `GO_INTEGRATED_AC` - the client claimed to run GeneralsOnline's anticheat while running none. `IsPluginLoaded()`/`IsExternalProcessRunning()` were likewise hardcoded `true`. |
| `GENERALS_ONLINE_DISABLE_SELF_UPDATE` | The service reports "update needed" for this client, and the stock flow would download and run the official patcher over a Contra install. |
| Login link copied to clipboard, dialog reworded | `ShellExecute` opens the browser *behind* the game in exclusive fullscreen (upstream runs windowed-fullscreen, which this fork does not take), so the login appeared to fail silently. Applied at all three browser-open sites of the pre-newserver flow. |

## Triage of GO's non-online-tree changes (`git diff -w e760b3695..d7f75517d`,
## excluding `*GameNetwork/GeneralsOnline/*`; 361 files, +21522/-3744)

Categories: **A** = online-required, port (guarded). **B** = engine-feel fork, skip.
**C** = gameplay/balance ("community patch", mostly unguarded), skip. **D** = noise
(NULL-reverts, whitespace, typos), skip. **I** = infra/CI/packaging, skip.
**R** = review at the phase that touches it; default skip unless an A-hunk is found.

### A — port (Core, Phase 3)

| File | -w delta | Notes |
|---|---|---|
| Core/.../GameNetwork/Transport.{h,cpp} | 19-29 / 11-361 | dual-mode: GO abstract base under GENERALS_ONLINE, original otherwise |
| Core/.../GameNetwork/ConnectionManager.{h,cpp} | 18-0 / 277-26 | initTransport switch + guarded hunks |
| Core/.../GameNetwork/Network.cpp | 118-55 | A-hunks only; drop RUN_FAST/HIGH_FPS |
| Core/.../GameNetwork/NetworkInterface.h | 15-1 | |
| Core/.../GameNetwork/NetworkDefs.h | 26-14 | |
| Core/.../GameNetwork/FrameMetrics.{h,cpp} | 21-0 / 151-8 | |
| Core/.../GameNetwork/Connection.{h,cpp} | 4-0 / 13-1 | |
| Core/.../GameNetwork/DisconnectManager.cpp | 8-0 | |
| Core/.../GameNetwork/GameInfo.{h,cpp} | 1-1 / 2-0 | |
| Core/.../GameNetwork/NAT.cpp | 4-2 | |
| Core/.../GameNetwork/NetworkUtil.cpp, NetCommandMsg.cpp, LANAPI.cpp | tiny | verify A vs D |
| Core/.../GameSpy/PeerDefs.{h,cpp} | 2-1 / 4-0 | SetUpGameSpy bypass |
| Core/.../GameSpy/MainMenuUtils.cpp | 151-4 | Online button -> NGMP init |
| Core/.../GameSpy/LobbyUtils.{h,cpp} | 13-4 / 540-34 | lobby list plumbing |
| Core/.../GameSpyOverlay.{h,cpp} | 5-0 / 14-0 | message boxes reused by GO UI |
| Core/.../GameSpy/LadderDefs.cpp | 3-0 | |
| Core/.../GameSpy/PersistentStorage{Defs.h,Thread.h,Thread.cpp} | 5-1 / 7-0 / 13-0 | |
| Core/.../GameSpy/Thread/{PeerThread,BuddyThread,PingThread,GameResultsThread}.cpp | 2-3 each | small guards |
| Core/.../GameNetwork/DownloadManager.h | 2-0 | |

### A — port (GeneralsMD lifecycle + support, Phase 3)

| File | -w delta | Notes |
|---|---|---|
| GeneralsMD/.../Common/GameEngine.cpp (+.h) | 135-28 / 7-0 | hand merge (fork delta); skip HIGH_FPS block, keep our try/catch |
| GeneralsMD/Code/Main/WinMain.cpp | 21-3 | hand merge (fork delta) |
| GeneralsMD/.../Win32Device/.../Win32GameEngine.cpp | 9-1 | |
| GeneralsMD/.../GameNetwork/UDPTransport.{h,cpp} | new 73/413 | retail transport impl, compiled only when ON |
| GeneralsMD/.../Common/version.cpp | 10-0 | |
| GeneralsMD/.../Common/System/registry.cpp | 53-0 | guarded language fallback |
| GeneralsMD/.../Common/CommandLine.cpp | 30-0 | GO-guarded args only |
| GeneralsMD/.../Common/Recorder.cpp | 38-6 | replay metadata; review hunks |
| GeneralsMD/.../Common/GameMain.cpp | 7-1 | review, likely lifecycle |
| GeneralsMD/.../GameLogic/System/GameLogic.cpp (+GameLogic.h) | 406-152 / 40-2 | MIXED — NGMP hunks only, file also carries C |
| GeneralsMD/.../GameClient/GUI/GUICallbacks/MessageBox.cpp (+.h) | 5-0 / 2-0 | |
| GeneralsMD/.../Common/CustomMatchPreferences.h | 3-0 | |
| GeneralsMD/.../Common/MessageStream.{h,cpp} | 5-0 / 4-0 | review: likely new GO message plumbing |
| Core/GameEngineDevice W3DDisplay.cpp (ZH path), ww3d.{cpp,h} | 7 / 2-1 | |

### A — port (UI, Phases 4-5)

Dual copies into `Menus/GeneralsOnline/`: WOLLoginMenu (140-265), WOLWelcomeMenu
(178-10), WOLLobbyMenu (1175-69), WOLGameSetupMenu (1743-425), WOLMapSelectMenu (54-16),
PopupHostGame (135-5), PopupJoinGame (34-6), WOLQuickMatchMenu (704-10), WOLBuddyOverlay
(659-6), PopupPlayerInfo (248-138), ScoreScreen (325-76).

In-place guards: MainMenu.cpp (4-1), OptionsMenu.cpp (23-1), InGameChat.cpp (7-0),
Diplomacy.cpp (15-1), DownloadMenu.cpp (35-1), GUIUtil.cpp (43-3), SkirmishGameOptionsMenu
(38-9, review), PopupReplay.cpp (3-0, review), LanGameOptionsMenu (2-2, review).

Stats (Phase 5): StatsExporter.{h,cpp} (44/864 new), StatsUploader.{h,cpp} (new).

### R — review on demand (pull minimally when the compiler asks, Phase 4)

GameWindowManager.{h,cpp} (259-256; GO also touched Generals copy), GadgetListBox.{h,cpp}
(110-0 / 8-0), W3DListBox (15-10), GadgetPushButton (11-2), GameWindowManagerScript
(22-1), GadgetTextEntry.h (1-0), GameWindow.h (2-1), AnimateWindowManager (4-1),
ControlBarPopupDescription (5-1), MapUtil.{h,cpp} (2-2 / 8-3), INIMapCache (2-1),
QuotedPrintable (8-8), SubsystemInterface (8-4), LanguageFilter (2-0), GameClient.{h,cpp}
(14-2 / 33-1), GlobalData.{h,cpp} (24-0 / 62-9 — mostly B, check for GO fields),
StackDump.cpp (3-2, Sentry-adjacent), WWLib/Except.cpp (75-2, Sentry crash handler —
only under GENERALS_ONLINE_SENTRY if ever), Core/GameEngine/CMakeLists.txt (5-10 —
re-implement by hand, never apply).

### B — skip (engine-feel fork)

GameMemory relocation (all GameMemory*/MemoryInit moves, Generals+MD), FramePacer,
FrameRateLimit, GameLOD.{h,cpp}, GameDefines.h, GameCommon.h, ReplaySimulation,
Intro.{h,cpp}, LoadScreen, CommandXlat, MetaEvent, Keyboard, W3DMouse, W3DView,
dx8wrapper, surfaceclass, render2dsentence, seglinerenderer, W3DVolumetricShadow,
W3DTerrainTracks, W3DTreeBuffer, W3DTankTruckDraw, W3DTruckDraw, W3DControlBar,
W3DHorizontalSlider, MilesAudioManager, FFmpeg*, GameAudio, ParticleSys,
ArchiveFileSystem.{h,cpp}, UserPreferences, OptionPreferences.{h,cpp}, View.h,
RadiusDecal, Drawable.cpp, ControlBar/ControlBarScheme/ControlBarResizer, Shell.cpp
(pure noise), Radar.cpp, Eva.cpp.

### C — skip (gameplay/balance, mostly unguarded)

All GameLogic/Object, GameLogic/AI, ScriptEngine bulk (ScriptEngine.cpp 341-248,
ScriptActions, ScriptConditions, Scripts), Weapon.cpp, EMPUpdate, TurretAI,
HelicopterSlowDeathUpdate, NeutronMissileUpdate, DeployStyleAIUpdate(+h), DeletionUpdate
(+h), JetSlowDeathBehavior, SlavedUpdate, MissileLauncherBuildingUpdate, PhysicsUpdate,
ParachuteContain, SpecialPowerModule, ObjectCreationList, Object.cpp, AI.cpp, FXList,
RankInfo, CrateSystem, Science, SidesList, Team, Player, CampaignManager, StateMachine,
SwayClientUpdate, ExperienceTracker.h, PerfTimer.h, Overridable.h,
ParkingPlaceBehavior.h, AcademyStats.h, GameStateMap/GameState, INIWebpageURL,
Compression (NoxCompress, EAC), AsciiString, INI.cpp, Debug.cpp.

### Skip wholesale

- All of `Generals/Code/**` (GO is ZH-only; vanilla Generals keeps GameSpy always).
- Infra: .github/workflows/*, resources/dockerbuild-msvc/*, scripts/*, PatchNotes/*,
  MakePatch.ps1, CMakePresets.json hunks, RTS.RC, Generals.ico, .gitignore,
  .editorconfig.
- Vendored-but-dead NAT libs: `Vendor/{miniupnpc,libplum,libnatpmp}` (35 files — not in
  GO's source lists, never included).
- GO's `GeneralsMD/Code/GameEngine/CMakeLists.txt` diff (86-7) and Main/CMakeLists.txt
  rename — contraZH's CMake is hand-edited instead.
