# contraZH Changes

Changes in [contraZH](https://github.com/triatomic/contraZH), a fork of GeneralsGameCode_Modding.
Everything here is additional to upstream; the rest of these docs still applies.

* Most changes are client-side, read from `Options.ini`, and default to retail behaviour. An
untouched `Options.ini` plays exactly as before.
* [Gameplay Fixes](contraZH-Gameplay-Fixes.md) are always on and fix retail simulation bugs.
* A few simulation rules are read from the mod's `GameData.ini`; each states its default.
* The map editor has its own page, [WorldBuilder Qt](WorldBuilder-Qt.md).
* Synced with TheSuperHackers/GeneralsGameCode and GeneralsGameCode_Modding as of August 2026. The
only casualty was the texture filter option, replaced by TheSuperHackers' version - see
[Rendering](contraZH-Options.md#rendering) for the renamed `AnisotropyLevel` key.

## Pages

* [Gameplay Fixes](contraZH-Gameplay-Fixes.md) - always-on fixes for retail simulation bugs
* [Maps & Game Setup](contraZH-Maps-and-Setup.md) - map features, 30-player maps, CLP_AI scripts, factions, game setup and campaign chapters
* [Options.ini](contraZH-Options.md) - every Options.ini setting: camera, display, hotkeys, selection, input and rendering
* [Objects & Modules](contraZH-Objects-and-Modules.md) - new object keys, module parameters and behavior modules
* [GameData.ini](contraZH-GameData.md) - new GameData.ini keys and simulation rules
* [SpecialPower.ini](contraZH-SpecialPowers.md) - new SpecialPower.ini keys
* [CommandButtons & Selection](contraZH-CommandButtons.md) - new command buttons, queue reorder and structure multi-select
* [ParticleSystem.ini](contraZH-ParticleSystems.md) - new ParticleSystem.ini keys
* [ObjectCreationList.ini](contraZH-Object-Creation-List.md) - new ObjectCreationList.ini nuggets
* [Animation2D.ini](contraZH-Animation2D.md) - direct textures in Animation2D.ini
* [Debug and Cheat Features](contraZH-Debug-and-Cheats.md) - debug overlays and cheat hotkeys in cheat builds
* [Generals Online (experimental)](contraZH-Generals-Online.md) - the experimental Generals Online build
