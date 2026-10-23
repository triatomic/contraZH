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
* Search costs use 32 bits, preventing the old overflow at 65535. The cell record pool holds 100,000
records instead of 30,000.

## Shoreline clearance

* Ground vehicles account for their footprint near water, cliffs and the map edge.
* A 3x3 vehicle pays extra to follow a shoreline. A 5x5 vehicle cannot enter a cell that leaves too
much of its footprint beyond the shore.
* Infantry and amphibious units keep their existing terrain rules.
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

## Infantry and vehicles

* Moving infantry steer along vehicle hulls and away from contact instead of stopping against them.
* Sidesteps must stay on a passable route. Infantry try the other side when the preferred side is
blocked, and stop and request a new route if neither side is clear.
* Vehicles creep past marching infantry without cancelling the infantry's route with a move-aside
order. A soldier that makes no progress for ten frames can still be asked to move aside.
* Panicking infantry keep their existing collision response.

Ported from Reforged commit `b7514253ed7cc2ad403cebe6dac64626751cc70f`.

## Bridges

* A path under a destroyed bridge stays valid while the bridge remains destroyed.
* Destroying or repairing a bridge schedules a zone update for the next frame.
* Destinations prefer the ground when the bridge above them is impassable.
* Deleting a bridge releases its pathfind layer.

## Debugging

Cheat builds provide a [pathfinding overlay](contraZH-Debug-and-Cheats.md#pathfinding-overlay) and
a [counter log](contraZH-Debug-and-Cheats.md#pathfinding-log).
