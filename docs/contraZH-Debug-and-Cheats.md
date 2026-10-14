# Debug and Cheat Features

Only in builds made with `RTS_DEBUG_CHEATS=ON`.

## Debug name overlays

Names drawn above every object on screen, props and wreckage included.

* `Ctrl + [` - (Cycles three ways: off, the object's template (INI) name in white, then the
model's sub object names in green as well.)
* `Ctrl + ]` - (Particle systems running on that object, in blue, with the FXList that
spawned them in amber)
* `Ctrl + '` - (The `CommandSet` the object uses, in yellow)
* `Ctrl + ;` - (The weapons the object is armed with, in red, under the command set)
* `Ctrl + -` - (The `Armor` the object currently uses, in light blue, under the weapons)
* `Ctrl + /` - (The W3D model each draw module has loaded, in teal, as a callout: a leader line runs
from the model out to one side and the name sits at the end of it)
* `Ctrl + ,` - (Laser beams: the laser object's name, which for a weapon is its `LaserName`, in pink
at the middle of the beam)
* `Ctrl + .` - (Laser beams: the `W3DLaserDraw` module tags, in lavender, under the laser name)

* Sub object names (hull, turret, wheels, ...) are for `ShowSubObject` / `HideSubObject`. Up to 16,
then "and N more".
* Command set, weapons and armor are read from the live object, so they show the set or block the
engine actually picked. Empty cases show `<none>`, `<no weapons>` or `<no armor>`.
* Weapons are listed as in a `WeaponSet` block, e.g. `PRIMARY NapalmMissileWeapon`.
* Models are the ones the current condition states picked, so damaged, night and snow variants show.
A module with no model in its current state gets no label. Each leader starts at the centre of its
model's bounds and goes out on the side that model is on; labels that would overlap are pushed up
above each other. Up to 16 per object.
* Laser labels that would overlap are pushed up above each other. A second beam of the same laser
landing on an existing label is not labelled again. With a laser overlay on, the object overlays
skip laser objects, which sit on their firer.

Also bindable in `CommandMap.ini` as `CHEAT_SHOW_OBJECT_NAME`, `CHEAT_SHOW_PARTICLE_NAMES`,
`CHEAT_SHOW_COMMAND_SET`, `CHEAT_SHOW_WEAPON_SET`, `CHEAT_SHOW_ARMOR_SET`, `CHEAT_SHOW_MODEL_NAMES`,
`CHEAT_SHOW_LASER_NAME` and `CHEAT_SHOW_LASER_BEAM_BLOCK`.

* `ParticleNameLingerMS = 0` - (Options.ini. Milliseconds a particle name stays on screen after its
system has gone. 0 or absent shows names only while the system is alive.)

Notes:
* Without a linger, one-shot effects flash past unreadably.
* The particle overlay is expensive; leave it off when not in use.

## Other cheat hotkeys

* ``Ctrl + ` `` - (Instant build, +999999 credits, this general's own sciences, max rank,
reveals the map)
* `Ctrl + \` - (Toggles rendering off and on; the simulation keeps running)
* `Shift + Ctrl + Z` - (Toggles the camera zoom limit)
* `Ctrl + F1` - (Toggles back face culling, drawing every face double sided when off. Contra also
binds this key to `SAVE_VIEW1`, so it saves camera bookmark 1 too. `CHEAT_TOGGLE_FACE_CULLING` in
CommandMap.ini)
* ``Shift + Ctrl + ` `` - (Cycles `HealthBarDisplayMode` live)

Notes:
* The health bar cycle is **not** a cheat and works in a normal release build too.
* A second press of the combined cheat resets rank via `resetRank()`, wiping purchased sciences.
* Sciences granted are only this general's own tree.
