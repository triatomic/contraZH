# Maps & Game Setup

## Maps

### Per-object scale

* Each placed object has a `Scale` field in WorldBuilder's Object Properties (`objectScale` in the
map). It multiplies the template `Scale`.
* WorldBuilder rolls a random scale when it places an object whose template has
`InstanceScaleFuzziness`, so a forest of one tree type gets mixed sizes. The roll is stored in the
map; the game never re-rolls it.
* Works for ordinary objects, optimized trees and props, and under fog of war.

Notes:
* Visual only. Footprint, collision, selection box and bone positions keep the template size.
* Saves from before this build load with every object at scale 1.
([#121](https://github.com/triatomic/contraZH/issues/121))

### Lightning lights the scene

* Maps fake lightning with the `CAMERA_FADE_ADD` script action, which washed the screen white. An
additive fade that lasts 1 second or less now lights terrain, roads, bridges and units with a
bluish-white light above the view instead.
* Longer additive fades, and all other fade types, still draw on the screen as before.
* Health bars and tooltips stay visible during the flash.

Notes:
* Needs `DynamicLights = Yes` in `Options.ini`. Without it the white screen flash is kept.
* Water surfaces do not take the light.
([#121](https://github.com/triatomic/contraZH/issues/121))

### Up to 30 players per map

* A map can hold 32 sides instead of 16. WorldBuilder keeps one free for the replay observer, so
it adds up to 30 players beside the neutral player (retail stopped at 14).
* The Add Player button now greys out at the same count where it stops working.

Notes:
* Games with 16 or fewer sides stay CRC compatible with retail and with older replays.
* Saves from games with 16 or fewer sides keep the retail format. Saves with more sides use a new
format that older builds refuse to load.
* Retail and older contraZH builds cannot load a map with more than 16 sides.
([#82](https://github.com/triatomic/contraZH/issues/82))

### Every road type draws

* Every road in `Roads.ini` now gets a slot. Roads past `MaxRoadTypes` in `GameData.ini` used to
appear in WorldBuilder's list but never drew in the editor or the game.
* `MaxRoadTypes` still sets the minimum number of slots.

Presentation only; replays unaffected.
([#82](https://github.com/triatomic/contraZH/issues/82))

### Neutral teams show in WorldBuilder

* Selecting the neutral player in the Teams dialog now lists its teams. They were hidden before,
so maps could carry neutral teams that nobody could see, edit, move or delete.
* Applies to both the classic and the Qt Teams dialog.
([#82](https://github.com/triatomic/contraZH/issues/82))

### Move to a free supply source

Two script actions send units to the nearest free supply source, for example a GLA Mobile Supply
Stash that deploys there.

| Action | Parameters |
|---|---|
| `Contra/Team_/Move/Set to move to the nearest free supply source` | team, minimum resources |
| `Contra/Unit_/Move/Move a specific unit to the nearest free supply source` | unit, minimum resources |

* Free means not enemy-owned and no supply center of any player within 20 cells.
* Nearest is measured from the unit, or from the team's position.
* If no source holds the minimum resources, the threshold halves until a source qualifies or it
drops to 100. Empty sources never qualify.
* Units stop beside the source on the side facing them, outside the `SupplyBuildBorder`.
* Does nothing when no source qualifies.
([#134](https://github.com/triatomic/contraZH/issues/134))

### CLP_AI scripts

142 script actions and 67 conditions ported from
[CookieLandProjects/CLP_AI](https://github.com/CookieLandProjects/CLP_AI), under the `CLP_AI` folder in
WorldBuilder's action and condition trees. The scripts were written by toirehantaa and CookieLand
Projects.

| Group | Examples |
|---|---|
| Team and unit orders | move relative, move towards or away from a relation, keep distance, meet, attack or attack-move by type, area or sight, teleport, repair, load transports evenly |
| Player and team management | merge, disband, garrison, hunt, sell, cancel construction, surrender, transfer an area, custom colour |
| Counters and KD ratios | random counter, copy counter, kills to deaths ratios fed by real kills and losses |
| Attack priority sets | apply per player or type, add sets, raise or lower a type |
| Skirmish AI building | rotated placement near a type, kind, team or supply source; base defense lists; closest-dozer priority; build list editing; particle cannon targeting |
| Waypoints | create, remove, relocate, link, label at runtime |
| Conditions | relations, start spots, map control, unit ratios, sighting, losses and kills, captures, team idle, health, reload state |

* Two new parameter types: KD ratio and selection mode (closest, farthest, random, youngest, oldest).
* Not ported, because they need data from CLP_AI's own INI files: ID build lists, AI playstyles,
Brutal/Absurd/Inhumane difficulties.
* "Seen" means the player can see the unit right now. CLP remembered sightings for 10 seconds, but
counted any player's units.
* Kill and loss conditions see every event since their last check, kept for 5 minutes.
* Runtime waypoint changes survive save and load. Removed waypoints stay valid for units already
following them.
* Old saves load unchanged; the new state lives in its own save blocks.

### New factions in PlayerTemplate.ini

`PlayableSide` defaults to `No`. Killing units of a faction without `PlayableSide = Yes` earns no
general experience and no unit veterancy. This is a retail default, not a code change.

A faction whose `Side` has no matching scheme in `ControlBarScheme.ini` uses the scheme of its
`BaseSide`, with `USA` read as `America`. Retail fell back to the art-less `Default` scheme, which
left the control bar broken for custom factions such as a mod's `AmericaCampaignGeneral`.

## Game Setup

### Random army per faction

* The army list gets a `Random <faction>` entry per base faction (`Random USA`, `Random China`,
`Random GLA`) after `Random`, for human and AI slots.
* Factions come from `BaseSide` in `PlayerTemplate.ini`, so new mod factions get an entry
automatically.

Notes:
* Labels are `GUI:Random<BaseSide>` (`GUI:RandomUSA`, `GUI:RandomChina`, `GUI:RandomGLA`), falling
back to `Random <BaseSide>`.
* Limit Armies still applies; an entry disappears when none of its generals are allowed.
* All LAN/online players need this build, and so do replays using these entries.

### Rules tab

* The Skirmish and LAN lobbies get **Game** and **Rules** tab buttons in the title row.
* The Rules page holds Quick Game, Coop Buildings and, in LAN, Max Camera Height. It takes the place
of the battle honors and info box in Skirmish, and of the chat in LAN.

Notes:
* Labels are `GUI:TabGame` and `GUI:TabRules`, falling back to `Game` and `Rules`.
* The layouts carry `ButtonTabGame`, `ButtonTabRules`, `PanelGame` and `PanelRules`. The buttons and
the Rules page start hidden, so an older exe shows the Game page without the new options.

### Quick Game

* The Skirmish and LAN lobbies get a **Quick Game** checkbox, off by default. Only the LAN host can
change it.
* In a Quick Game, objects use their `QuickGamePrerequisites` in place of `Prerequisites`, so the mod
can drop the rank requirements on advanced structures, superweapons and super units. An object
without `QuickGamePrerequisites` keeps its normal ones. See
[QuickGamePrerequisites](contraZH-Objects-and-Modules.md#quickgameprerequisites).
* Skirmish AI scripts that test `SKIRMISH_PLAYER_HAS_PREREQUISITE_TO_BUILD` follow the Quick Game
list, so the AI builds those structures early too.
* The `QUICK_GAME_ENABLED` condition (`Skirmish/ Quick Game is enabled` in WorldBuilder) is true in
a Quick Game. To stop the AI waiting for a rank before it builds, copy the script's condition clause
as a new OR clause and swap the rank test for `QUICK_GAME_ENABLED`.
* Leave rank tests that time attack waves alone. A script that raises a rank counter should not get
the clause either, or the counter jumps at game start; move its other actions into a new script
gated by `QUICK_GAME_ENABLED` instead.

Notes:
* Labels are `GUI:QuickGame` and `TOOLTIP:QuickGame`, falling back to English text.
* The checkbox comes from `CheckQuickGame` in `SkirmishGameOptionsMenu.wnd` and
`LanGameOptionsMenu.wnd`; a layout without it shows no checkbox.
* Save games and replays keep the setting. A save without Quick Game stays loadable by retail.
* All LAN players need this build. Not available on Generals Online.
* An older exe reads `QUICK_GAME_ENABLED` as false, so a script with it still runs on the rank test.
* Changes the simulation; affects replays.

### Host camera height

* The LAN host gets a `Max Camera Height` checkbox and field (210 to 1000) that overrides everyone's
`MaxCameraHeight` for that game.
* Clients need this build. Replays keep the limit they were played with.
* Generals Online keeps its `/maxcameraheight` lobby command; GO's default (310) means the mod's own
limit.
* The `Shift + Ctrl + Z` zoom-limit cheat is unaffected.

### Map search

* The skirmish and LAN map select windows have a search box above the map list, focused on open.
* Typing filters the list to maps whose name contains the text, ignoring case. It applies to both
official and custom maps.
* The box takes its height from the top of the list and copies the look of the setup screen's text
box (player name in skirmish, chat in LAN), so mod layouts need no `.wnd` change.
