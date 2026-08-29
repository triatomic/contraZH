# Camera Controls

Cheat-build camera kit (`generalszh_c-camera.exe`). Everything here is client-side
only: no effect on the simulation, replays or multiplayer sync.

## Mode cycle — `Delete`

Each press moves one stage forward. Stages that need an object are skipped when
nothing is selected.

| Stage | Name | What it is |
|---|---|---|
| 0 | **Default** | The normal RTS camera, restored exactly as you left it. |
| 1 | **Free** | Fly anywhere, perspective projection. |
| 2 | **Chase** | Third-person camera following the selected object. |
| 3 | **Perspective** | First-person ride on that object; view turns with its facing. |
| 4 | **Orthographic** | Free flight with a parallel projection — the schematic look. |

With nothing selected: Default → Free → Orthographic → Default.

While any mode is active the whole UI hides: control bar, radar, health bars and
icons, hover tooltips, superweapon and script timers, FPS/time readouts. The
terrain also draws fully **unshrouded** — no black fog-of-war walls at low angles
(hidden enemy units stay hidden; only the terrain clears). Game hotkeys stay live.
Everything restores on exit, on object death, and at match end.

## Movement — Free and Orthographic

| Control | Action |
|---|---|
| `W` `A` `S` `D` | Fly forward / left / back / right along the view. |
| `Space` | Ascend. |
| `Ctrl` | Descend (paused while RMB is held — that's the roll gesture). |
| `Shift` | Hold for 5× speed. |

The camera cannot go below the terrain.

## Chase controls

| Control | Action |
|---|---|
| Arrow keys | Pan the view away from the object; it keeps following. `Shift` = 5×. |
| Mouse wheel | Zoom the follow distance in and out. |

## Right-mouse-button gestures (hold RMB and drag)

| Modifier | Action |
|---|---|
| *(none)* | Look around (Free/Perspective) or orbit the object (Chase). |
| `Alt` | Field of view: drag down widens, up narrows (10°–120°). In Ortho: scales the view size instead. |
| `Ctrl` | Roll — bank the camera for a dutch angle (±180°). |
| `Ctrl` + `Alt` | **Dolly zoom** (Chase only): FOV changes while the camera compensates, keeping the object the same size while the background stretches — the vertigo shot. |

The cursor is captured while looking and released when RMB is let go. FOV and roll
reset when you exit the cheat.

## Scene cheats

| Key | Action |
|---|---|
| `Ctrl` + `D` | Cycle the skybox: map default → Morning → Moon. Forces the sky to draw. |
| `Ctrl` + `X` | Cycle the terrain: normal → black → **green screen**. Hides terrain, roads, trees, shroud and water, leaving units over a flat backdrop for chroma keying. |

Terrain-hidden mode also suppresses the skybox (the water object draws it), so use
`Ctrl+D` with terrain on Normal.

## Notes

- All keys are rebindable through `CommandMap.ini`: `CHEAT_CYCLE_CAMERA_MODE`,
  `CHEAT_CYCLE_SKYBOX`, `CHEAT_CYCLE_TERRAIN_MODE`.
- The mode-change message ("Camera: Free" …) stays on screen deliberately, so you
  always know where you are in the cycle.
- The unit you ride or chase is still yours to command.
