# Pathfinding Improvements

These changes are always on. They change the simulation, so all players need the same build and
older replays may play differently.

## Search limits and retries

* A full pathfind queue retries refused requests after a few frames. The queue holds 2047 pending
requests instead of 511.
* A fine search may allocate at most 10,000 cell records. Straight-line probes and transitions
between ground and bridge layers count toward the limit.
* A search that reaches its limit can return a partial route. The unit continues toward its original
destination for up to eight more legs. Repairing that route around traffic keeps the continuation;
a new order or destination starts a fresh allowance.
* Zero Hour's coarse search expands beyond its starting block and gives the fine search a corridor.
* The fine search's straight-line probe toward the goal stops at the first cell it already reached
more cheaply, instead of re-walking explored ground.
* Search costs use 32 bits, preventing the old overflow at 65535. The cell record pool holds 100,000
records instead of 30,000.

## Shoreline clearance

* Ground vehicles account for their footprint near water, cliffs and the map edge.
* A 3x3 vehicle pays extra to follow a shoreline. A 5x5 vehicle cannot enter a cell that leaves too
much of its footprint beyond the shore.
* The coarse search skips block crossings a 5x5 vehicle cannot enter, so it does not route large
vehicles over land strips too narrow for them.
* Infantry and amphibious units keep their existing terrain rules.
* Vehicles on a scripted exact waypoint path, or on a move that leaves the map, ignore shoreline
clearance while turning, so they follow the path as placed.
* Shoreline clearance uses the unit's full locomotor set, so amphibious units with separate ground
and water locomotors can enter and leave water.

## Group movement

* Ground-only units receive separate arrival slots around the click. Nearby units fill the inner
slots first, so the group gathers without spiralling outward.
* Move, attack-move and column orders use these slots. A click inside the group's footprint keeps
relative offsets.
* Naval, amphibious and cliff-capable units keep their existing destination handling.
* A unit joins a column only when its route passes within a quarter of its remaining length, or
six cells, whichever is greater. Units farther away find their own routes.

## Bridges

* A path under a destroyed bridge stays valid while the bridge remains destroyed.
* Destroying or repairing a bridge schedules a zone update for the next frame.
* Destinations prefer the ground when the bridge above them is impassable.
* Deleting a bridge releases its pathfind layer.

## Debugging

Cheat builds provide a [pathfinding overlay](contraZH-Debug-and-Cheats.md#pathfinding-overlay) and
a [counter log](contraZH-Debug-and-Cheats.md#pathfinding-log).
