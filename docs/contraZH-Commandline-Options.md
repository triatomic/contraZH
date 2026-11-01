# Commandline Options

This page lists every command-line option registered by the Zero Hour game in this fork,
including options inherited from upstream, debug options and the separate AI match and MCP parsers.
WorldBuilder and standalone tools have their own command lines and are outside this list.

## Usage

Append options to the executable in a shortcut's Target field or in a terminal.
Replace values in angle brackets with your own values. Quote paths and map names containing spaces.

```bat
generalszh.exe -win -xres 1920 -yres 1080 -quickstart
generalszh.exe -mod "C:\Mods\MyMod.big"
generalszh.exe -headless -replay "Tournament*.rep" -jobs 4
```

Option names are case-insensitive and must match in full. Values normally follow the option as
a separate argument; the MCP port option uses `:` or `=` instead. Supply every required value.
Unrecognized options are ignored by the main parser, including options absent from the build.

Startup options are processed before engine-init options, regardless of their order on the command
line. Within each pass, options are processed from left to right. Engine-init options run before
INI loading, so a handler setting a value does not guarantee that later initialization preserves it.

## Release options

These options are registered without a debug-build requirement.

### Window and rendering

| Option | Effect |
|---|---|
| `-win` | Start in a window. |
| `-fullscreen` | Start fullscreen. |
| `-xres <pixels>` | Set the horizontal resolution. |
| `-yres <pixels>` | Set the vertical resolution. |
| `-forcefullviewport` | Set the viewport height scale to 1.0 for Control Bar Pro-style addons. |
| `-noshaders` | Force the legacy minimal-feature chipset path. This is not a switch for every D3D9 effect. |
| `-headless` | Run without a window, graphics, input or audio; skip intro and sizzle videos. Useful for replay simulation and AI matches. |

### Startup, data and tools

| Option | Effect |
|---|---|
| `-quickstart` | Combine `-nologo`, `-noshellmap` and `-noShellAnim`. |
| `-nologo` | Skip intro and sizzle videos. |
| `-noshellmap` | Disable the animated map behind the main menu. |
| `-noShellAnim` | Disable menu window animations. |
| `-fullVersion <0\|1>` | Hide or show the full version string. The handler reads the value but only marks the option itself as consumed. |
| `-mod <path>` | Load a mod directory or BIG file. Relative paths are resolved under the game's user-data directory; absolute paths are used directly. |
| `-live` | Check loose files before archives on every read, allowing loose overrides added while the game runs to take effect. |
| `-useCwd` | Use the working directory supplied when the process was launched. |
| `-setCwd <directory>` | Set an explicit working directory. The last successful `-setCwd` or `-useCwd` selection wins; otherwise the executable directory is used. |
| `-particleEdit` | Enable particle-editor mode, Windows cursors and windowed mode. |
| `-scriptDebug` | Enable the script debugger and Windows cursors. |
| `-useWaveEditor` | Enable the water-track editor mode. |
| `-playStats <n>` | Set play-stat logging; values at or below zero disable logging. |

### Saves and replays

| Option | Effect |
|---|---|
| `-loadsave <file>` | Load a save game at startup; disable the shellmap and intro/sizzle videos. |
| `-loadreplay <file>` | Play a replay at startup; disable the shellmap and intro/sizzle videos. |
| `-replay <pattern>` | Queue replay simulation. Include `.rep`; paths are relative to the replay directory and can include subdirectories and wildcards. Repeat the option to queue more patterns. Disables the shellmap and intro/sizzle videos and permits another game instance to run. |
| `-jobs <n>` | Run queued replay simulations in separate processes, with up to `n` concurrent processes. Positive integers are accepted; `-1` selects sequential simulation in one process, which is also the default. Zero and values below `-1` exit with an error. |

## AI matches

These Zero Hour options run an AI-only skirmish with a local observer. They do not require a debug
build. Put `-aiMatch` before the `-aiMatch...` settings because those settings are read only after
the match has been created. See [AI Matches](Game-MCP-and-AI-Matches.md#ai-matches) for log details.

| Option | Effect |
|---|---|
| `-aiMatch <map>` | Start an AI match on a multiplayer map matched by name or part of its path. Allows another game instance to run. |
| `-aiPlayers <list>` | Supply at least two AI players as comma-separated `Faction:difficulty[:team]` entries. Difficulty accepts `easy`, `normal` or `hard`; omitted difficulty defaults to hard and omitted team to `-1`. |
| `-aiMatchLog <file>` | Write the JSON-lines match log here. Default is `AIMatch.jsonl`. |
| `-aiMatchMinutes <minutes>` | Stop after this much game time. Accepts fractional minutes; default is 30. |
| `-aiMatchInterval <seconds>` | Set the game-time interval between snapshots. Accepts fractions; default is 10 seconds and the minimum is one logic frame. |
| `-aiMatchCash <amount>` | Override starting cash; default is the game's setting. |
| `-aiMatchScripts <file>` | Use this AI script file instead of `Data\Scripts\SkirmishScripts.scb`. |
| `-aiMatchAIData <file>` | Load this INI over the game's AI data to override settings, side information and build lists. |

```bat
generalszh.exe -headless -aiMatch "Tournament Desert" -aiPlayers FactionChina:hard,FactionGLA:hard -aiMatchMinutes 10
```

`-seed` can fix the random seed, but is registered only in `RTS_DEBUG` builds.

## Game MCP bridge

Requires `RTS_BUILD_OPTION_GAME_MCP=ON`, which defines `RTS_GAME_MCP`. These options do not require
a debug build. See [Game MCP Bridge](Game-MCP-and-AI-Matches.md) for commands and sandbox behavior.

| Option | Effect |
|---|---|
| `-mcp` | Enable the bridge on `127.0.0.1:47801`, unless a port was already selected. |
| `-mcpport:<port>` or `-mcpport=<port>` | Enable the bridge on a specific localhost port. Valid ports are 1 through 65535; an invalid final port disables the bridge. |
| `-mcpGame <map>` | Start a skirmish on a multiplayer map matched by name or part of its path. An AI match takes precedence if both are supplied. |
| `-mcpPlayers <list>` | Supply the local player first as `Faction[:team]` or `Observer`, then optional AI players as `Faction:difficulty[:team]`, comma separated. |
| `-mcpCash <amount>` | Override starting cash; default is the game's setting. |
| `-mcpStartPaused` | Pause when the game starts. |

The game-start settings require an enabled bridge. Supplying only the local player creates a
sandbox without opponents. Difficulty names are the same as for AI matches.

## Generals Online

Requires `RTS_BUILD_GENERALS_ONLINE=ON`, which defines `GENERALS_ONLINE`.

| Option | Effect |
|---|---|
| `-exportStats` | Export game statistics as JSON alongside the replay. |
| `-statsUrl <url>` | Set the URL for posting compressed game statistics. |

## Cheat-build options

These two options require `RTS_DEBUG` or `_ALLOW_DEBUG_CHEATS_IN_RELEASE`
(`RTS_DEBUG_CHEATS=ON` for release builds). That setting alone does not enable the debug-only
options in the next section.

| Option | Effect |
|---|---|
| `-preload` | Enable asset preloading. |
| `-displayDebug` | Enable the debug display. |

## Debug-only options

All options in this section require `RTS_DEBUG`. Some also require the condition stated in the row.
Legacy editor, profiling and test switches are included because the parser still registers them;
their handlers can set a mode even when its supporting tools are unavailable.

### Audio, cinematics and input

| Option | Effect |
|---|---|
| `-noaudio` | Disable audio, speech, sounds and music. |
| `-nomusic` | Disable music. |
| `-novideo` | Disable video playback. |
| `-NoInputDisable` | Prevent scripts from disabling player input. |
| `-nomilcap` | Disable military-caption delays. |
| `-nofade` | Disable camera fades. |
| `-nomovecamera` | Disable scripted camera movement. |
| `-nocinematic` | Combine the camera-movement, military-caption, fade and scripted-input-disable overrides. |
| `-winCursors` | Use Windows cursors. |
| `-UseCSF` | Use CSF strings instead of the debug text string file. |

### Rendering and assets

| Option | Effect |
|---|---|
| `-lowDetail` | Select the minimum terrain LOD. |
| `-noDynamicLOD` | Disable dynamic LOD. |
| `-noStaticLOD` | Disable static LOD. |
| `-fps <n>` | Set the frame-rate limit. |
| `-noFPSLimit` | Disable the FPS limiter and set its stored limit to 30000. |
| `-noDraw` | Set no-draw mode only when `DEBUG_CRC` is also defined; otherwise the handler does nothing. |
| `-jumpToFrame <frame>` | Disable the FPS limiter and set the no-draw cutoff to this frame. |
| `-wireframe` | Enable wireframe rendering. |
| `-showCollision` | Show collision extents. |
| `-noShowClientPhysics` | Disable the client-physics display. |
| `-showTerrainNormals` | Show terrain normals. |
| `-noshadowvolumes` | Disable both shadow volumes and shadow decals. |
| `-nofx` | Disable FX. |
| `-noshroud` | Disable shroud. Also requires `ENABLE_CONFIGURABLE_SHROUD`. |
| `-shellmap <map>` | Override the shellmap name. |
| `-noagpfix` | Enable incremental AGP-buffer behavior. |
| `-FPUPreserve <n>` | Set the graphics wrapper's FPU-preservation value. |
| `-preloadEverything` | Enable asset preloading and preload-everything mode. |
| `-logAssets` | Enable preload reporting and clear `PreloadedAssets.txt`. |
| `-dumpAssetUsage` | Enable the asset-usage dump. |
| `-updateImages` | Enable TGA-to-DDS updating during loading. |

### Testing and diagnostics

| Option | Effect |
|---|---|
| `-map <path>` | Set the map name only when this option has exactly one argument left, so place it last. Expands `Maps\Name.map` to `Maps\Name\Name.map`. |
| `-file <path>` | Set the initial map file, expanding the same short map-path form as `-map`. |
| `-seed <n>` | Set a fixed random seed. |
| `-noViewLimit` | Disable camera constraints. |
| `-cameraDebug` | Enable camera debugging. |
| `-stateMachineDebug` | Enable state-machine debugging. |
| `-showTeamDot` | Enable team-dot display. |
| `-selectTheUnselectable` | Allow selection of normally unselectable objects. |
| `-constantDebug` | Enable constant debug updates. |
| `-extraLogging` | Enable extra debug logging. |
| `-logToCon` | Enable console logging when `ALLOW_DEBUG_UTILS` is defined. |
| `-jabber` | Enable the legacy Jabber test mode. |
| `-munkee` | Enable the legacy Munkee test mode. |
| `-benchmark <n>` | Set both the benchmark timer and the play-stat logging value. |
| `-forceBenchmark` | Force benchmark mode. |
| `-vTune` | Enable the legacy VTune profiling mode. |
| `-stats <n>` | Enable periodic performance-stat dumps and set the interval. Also requires `DUMP_PERF_STATS`. |
| `-saveStats <directory>` | Enable statistics saving to this base directory. |
| `-saveAllStats <directory>` | Enable statistics saving, including all-statistics mode, to this base directory. |
| `-localMOTD <path>` | Use a local message-of-the-day file. |
| `-netMinPlayers <n>` | Set the network minimum-player value. |
| `-DemoLoadScreen` | Enable the demo loading-screen mode. |
| `-buildmapcache` | Enable map-cache building. |
| `-ignoresync` | Ignore synchronization errors for debugging. |
| `-packetloss <percent>` | Set simulated packet loss. |
| `-latAvg <n>` | Set the average simulated network latency. |
| `-latAmp <n>` | Set the amplitude of sinusoidal latency variation. |
| `-latPeriod <n>` | Set the period of sinusoidal latency variation. |
| `-latNoise <n>` | Set the maximum simulated latency jitter. |
| `-noLogOrCrash` | Unsupported. The handler raises a debug crash and changes no setting. |

## CRC and logging options

These options use their own compile-time guards rather than `RTS_DEBUG`. In the current sources,
`Debug.h` enables `RELEASE_DEBUG_LOGGING`, which enables `DEBUG_LOGGING`; `CRCDebug.h` then enables
`DEBUG_CRC` unless `NO_DEBUG_CRC` is defined. These options can therefore be present in release builds.
Use the same CRC settings on all multiplayer clients to avoid mismatches.

| Option | Requirement | Effect |
|---|---|---|
| `-DebugCRCFromFrame <frame>` | `DEBUG_CRC` | Start CRC logging at this frame. `0` logs from the beginning; `-1` disables it and is the default. |
| `-DebugCRCUntilFrame <frame>` | `DEBUG_CRC` | Set the last frame to log. |
| `-KeepCRCSaves` | `DEBUG_CRC` | Keep binary data used in CRC calculations. |
| `-SaveDebugCRCPerFrame <directory>` | `DEBUG_CRC` | Save each frame's CRC debug data separately. **The directory is deleted if it already exists at each game start.** If no start frame was selected, sets it to zero. |
| `-CRCLogicModuleData` | `DEBUG_CRC` | Include module data from logic in CRC debugging. |
| `-CRCClientModuleData` | `DEBUG_CRC` | Include module data from the client in CRC debugging. |
| `-VerifyClientCRC` | `DEBUG_CRC` | Check that client updates do not change the game-logic CRC. |
| `-ClientDeepCRC` | `DEBUG_CRC` | Write detailed CRC data before and after client updates to `clientPre.crc` and `clientPost.crc`. |
| `-LogObjectCRCs` | `DEBUG_CRC` | Log object and weapon CRCs. |
| `-NetCRCInterval <frames>` | `DEBUG_CRC` | Set frames between multiplayer CRC checks. Has no effect with `RETAIL_COMPATIBLE_NETWORKING` enabled. |
| `-ReplayCRCInterval <frames>` | `DEBUG_CRC` | Set frames between CRC entries written to single-player replays. |
| `-setDebugLevel <name>` | `DEBUG_LOGGING` | Enable the named debug-level bit. The only defined name is `NET`, which must be uppercase. |
| `-clearDebugLevel <name>` | `DEBUG_LOGGING` | Disable the named debug-level bit. The only defined name is `NET`, which must be uppercase. |
| `-ignoreAsserts` | `DEBUG_CRASHING` | Ignore debug assertions. |
| `-ignoreStackTrace` | `DEBUG_STACKTRACE` | Disable assertion stack traces. |

The current `RELEASE_DEBUG_LOGGING` block disables `DEBUG_CRASHING` and `DEBUG_STACKTRACE` by
default, so their two switches require a build that explicitly enables those features.

## Unregistered option

`-allAdvice` appears only in commented-out code. It has no active parser entry and no effect.

## Sources

The option lists and build guards come from these game parsers.

* [CommandLine.cpp](https://github.com/triatomic/contraZH/blob/main/Core/GameEngine/Source/Common/CommandLine.cpp) contains the startup and engine-init tables and handlers.
* [AIMatch.cpp](https://github.com/triatomic/contraZH/blob/main/GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIMatch.cpp) reads AI match settings.
* [GameMcp.cpp](https://github.com/triatomic/contraZH/blob/main/GeneralsMD/Code/GameEngine/Source/Common/Mcp/GameMcp.cpp) reads MCP settings.
