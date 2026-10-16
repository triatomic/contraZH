# Generals Online (experimental)

Optional build with the [Generals Online](https://github.com/GeneralsOnlineDevelopmentTeam/GameClient)
online system, the community GameSpy replacement: login, lobbies, matchmaking and stats over REST +
WebSocket, peer to peer over Valve's GameNetworkingSockets (ICE with STUN/TURN).

* Off by default; a normal build compiles all of it out.
* `-DRTS_BUILD_GENERALS_ONLINE=ON` replaces the GameSpy path: Online runs GO's version check and
login, and the WOL screens become GO's. LAN and skirmish are unchanged.

Notes:
* Needs a Generals Online backend; without one login fails and returns to the main menu. Using the
official `playgenerals.online` service with Contra needs coordination with the GO team.
* Upstream crash reporting, anti-cheat and hardware fingerprinting are compiled out.
* See `PORT_NOTES.md` beside the GeneralsOnline sources for details and deviations.

## Engine differences in the online build

These apply to the whole executable, skirmish and campaign included.

### 60 Hz simulation

Logic runs at 60 fps, matching the official `gen_online_60hz` client. Speeds, reloads and
millisecond durations are unchanged. Raw-frame code (turret turn rates, death arcs, particle
keyframes, script timers, deploy animations) keeps retail timing via a 30 Hz counter.

* Replays only play back in the same build; 30 Hz and 60 Hz replays are rejected by each other.
* The lowest render frame rate cap is 60.

### Frame rate cap during online matches

During an online match the render cap comes from Generals Online's `settings.json`. After the match,
the cap, FPS limit switch and scroll speed return to Contra's values. Shell, skirmish and campaign
never use the GO values.

### Observer overlay

Spectators get extra player columns (science points, kills, losses, power surplus in red when short,
army name) and a notification feed for promotions, power and superweapon use, rank 3 and 5, 10k/min
income, GLA power gains, and losing all dozers and command centers. The control bar toggle hides
both.

`Options.ini` keys, all optional:

| Key | Default | Meaning |
|---|---|---|
| `ObserverNotificationFontSize` | `10` | Feed font size; `0` disables the feed |
| `ObserverNotificationSpecialPowerUsage` | `yes` | Announce power use |
| `ObserverNotificationSpecialPowerPurchase` | `yes` | Announce promotions |
| `ObserverNotificationMilestone` | `yes` | Announce rank, income, power and dozer milestones |

### Widescreen layouts and camera

The UI targets 1280x720 instead of 800x600, and max camera height scales with aspect ratio. Window
layouts come from `GeneralsOnlineGameData\` when present. Without it, the control bar and sliders
draw at the wrong scale; with it, the options menu lacks Contra's controls. See `PORT_NOTES.md`.

### Replay analytics

`-headless -replay <file> -exportStats` writes a gzipped JSON match summary next to the replay, and
`-statsUrl <url>` posts it. Only works in headless replay simulation.

### Diplomacy rule

The lobby host types `/diplomacy` to let players change alliances during the match. The GO service
has no field for it, so the host's game switches it on in the first frames. A host change turns it
off. See
[Diplomacy](contraZH-Maps-and-Setup.md#diplomacy).

## Runtime requirements

An `RTS_BUILD_GENERALS_ONLINE=ON` build needs these DLLs **next to the executable**, or it fails at
startup with "the application was unable to start correctly". The build copies them automatically;
this matters when copying the exe by hand.

| File | Provides |
|---|---|
| `GameNetworkingSockets.dll` | Valve GameNetworkingSockets - the peer to peer transport |
| `libprotobuf.dll` | Protocol Buffers, used by GameNetworkingSockets |
| `abseil_dll.dll` | Abseil, used by Protocol Buffers |
| `libcrypto-3.dll`, `libssl-3.dll` | OpenSSL 3 - TLS for the service and DTLS for peer traffic |
| `libcurl.dll` | HTTP and the WebSocket client |
| `zlib1.dll` | compression, used by libcurl |
| `discord-rpc.dll` | Discord rich presence (optional at runtime, but the import is not) |

* All are 32 bit and must come from one matching set. A mismatched `libprotobuf.dll` and
`abseil_dll.dll` corrupt the heap before `WinMain`; replace them all together.
* Some GO installs ship a **64 bit** `zlib1.dll`, which breaks `libcurl`. Use the 32 bit one.

A normal Contra build needs none of these.
