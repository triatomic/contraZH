# GameData.ini

## Unknown keys and blocks are skipped

An INI key or block the game does not know is logged to `DebugLogFile.txt` and skipped instead of
crashing the game or WorldBuilder. Retail asserted on an unknown key in debug builds and threw on
an unknown block in every build, so a mod INI written for a newer exe could not load on an older
one. An unknown block is skipped up to its `End`, which also covers a new key that opens a nested
block. A known key with a bad value still fails the load.

## Subdual damage defaults

Subdual, jamming, frozen and chrono tuning can be set once in GameData.ini instead of on every
ActiveBody. Any number of `SubdualDamageDefaults` blocks, each optionally limited by `KindOf` /
`ForbiddenKindOf`, with values relative to max health:

```
SubdualDamageDefaults
  SubdualDamageCap        = MaxHealth * 2
  SubdualDamageHealRate   = 500
  SubdualDamageHealAmount = MaxHealth / 16.25
End
```

* An ActiveBody key still wins; `SubdualDamageCap = 0` still means immune.
* An ActiveBody without the key falls back to the global block instead of zero.
* `MaxHealth` forms work on ActiveBody too, which also gains `ChronoDamageHealRate` and
`ChronoDamageHealAmount`.

Details in [GameData](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/GameData#subdual-damage-defaults).

## Subdual frozen

`SUBDUAL_FROZEN` is a third subdual pool beside subdual and jamming. It disables via
`DISABLED_FROZEN` and sets a `FROZEN` condition state. Customisable like jamming:

* `FrozenDamageCap`, `FrozenDamageHealRate`, `FrozenDamageHealAmount` on ActiveBody or in
`SubdualDamageDefaults`
* armor coefficient
* `Frozen` health-bar icon in Animation2D.ini
* `SoundFrozen` / `SoundUnfrozen` per unit, `UnitFrozen` / `UnitUnfrozen` in MiscAudio.ini as
fallback
* `FrozenOverlay*` texture block in GameData.ini (stacks with the jamming overlay)

Details in [Subdual Frozen](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#subdual-frozen).

## BatchParticles

* `BatchParticles = No` - (Default. `Yes` draws consecutive particle systems that share a texture,
blend mode and billboard mode in a single call.)

Batches plain particle systems into one 512-point buffer, 15 to 30 percent cheaper per
TheSuperHackers. Streak, volume and terrain-conforming systems are never batched.

Notes:
* Particle systems with nothing on screen are now skipped regardless of the option.
* Ported from TheSuperHackers commit `de20ae0cb` (Ronin and Mauller).

## ParticleAtlas

* `ParticleAtlas = No` - (Default. `Yes` packs particle textures into shared atlas pages, so alpha
and additive systems with different textures batch into the same draws.)

Plain alpha and additive particle systems draw from 2048x2048 pages with premultiplied blending.
One blend state covers both modes, and the translucency sorter merges neighbouring triangles that
share a page into one draw. Batches no longer break on texture or blend changes.

Notes:
* Implies batching for the systems it covers, whatever `BatchParticles` says.
* Additive particles now depth-sort with smoke instead of drawing after it, so smoke in front of
a fire covers it.
* Textures are packed the first time they draw, at most four per frame, and the pages are cleared between maps. Up to four
pages per map; textures that do not fit draw as before.
* Not covered: streak, volume, terrain-conforming, alpha-test and multiply systems, systems with
the flame, cryo, electric or disruption shaders, non power-of-two textures and ones smaller than
16 pixels. The glow pass also keeps the old path.
* Each texture keeps five mip levels, so very distant sprites alias a little more.
* After stephanmeesters' texture atlas prototype for TheSuperHackers.

## SkipTranslucencySort

* `SkipTranslucencySort = No` - (Default. `Yes` draws translucent triangles in submission order
instead of depth-sorting them every frame.)

Skips the per-frame CPU sort of every translucent triangle, which is costly with many particles.
The cost is layering errors where translucent effects overlap. Opaque and alpha-tested geometry is
unaffected.

Notes:
* Also settable per detail level in GameLOD.ini (`StaticGameLOD` blocks). Sorting is skipped when
either the GameData key or the active level says `Yes`.
* No Options menu checkbox.

## BackToFront

* `BackToFront = No` - (Default. `Yes` draws whole particle systems farthest first when
`SkipTranslucencySort = Yes`.)

Orders whole particle systems by distance, which removes most layering errors between effects.
Particles within one system, streaks, tank tracks and water are not reordered.

Notes:
* Does nothing while `SkipTranslucencySort = No`.
* `BatchParticles` batches get slightly smaller.

## ForceFireAllWeapons

* `ForceFireAllWeapons = No` - (Default. `Yes` makes an attack ground order fire every weapon that
can hit the ground, on every turret, instead of only the primary weapon.) Goes in the `AIUpdate`
block next to `TurretsLinked`.

Unlike `TurretsLinked`, this only affects attack ground and skips weapons that cannot hit ground.
Each turret aims and fires its own ground weapon when ready and in range.

Notes:
* Attacking a unit or building still picks the single best weapon.
* Synced slots still follow their lead; a locked weapon (attack ground special powers) fires alone.
* Weapons beyond the lead skip `PreAttackDelay`, as with linked turrets.
* Slots whose `AutoChooseSources` excludes `FROM_PLAYER` are skipped for player orders.

## NoOccupantFriendlyFire

* `NoOccupantFriendlyFire = No` - (Default. `Yes` spares the container a passenger is riding in from
that passenger's own splash damage.)

Stops e.g. a Tank Hunter in a bunker knocking down its own bunker.

Notes:
* Covers the whole containment chain (infantry in a bunker on an Overlord spare both).
* `RadiusDamageAffects = SELF` weapons (suicide attacks) still destroy the container.
* A direct order to attack the container still damages it.
* Changes the simulation; replays must use the same setting.

## Transport load slowdown

* `TransportLoadSpeedPenalty = 0%` - (Default. The fraction of its `Speed` a container loses when it
is completely full.)
* `TransportLoadTurnRatePenalty = 0%` - (Default. The same for `TurnRate`.)
* `TransportLoadAccelerationPenalty = 0%` - (Default. The same for `Acceleration`.)
* `TransportLoadLiftPenalty = 0%` - (Default. The same for `Lift`.)
* `TransportLoadPenaltyKindOf` - (Default: every kind. Only occupants with at least one of these
`KindOf` bits count toward the load.)
* `TransportLoadPenaltyForbidKindOf` - (Default: none. Occupants with any of these `KindOf` bits
never count toward the load.)

A container loses the penalty in proportion to how full it is, counted in slots, and regains it as
passengers leave.

```
GameData
  TransportLoadSpeedPenalty        = 40%
  TransportLoadAccelerationPenalty = 25%
End
```

A container's own `LoadPenaltyKindOf` and `LoadPenaltyForbidKindOf` replace the global `KindOf`
keys. Per-container overrides and opt-outs: see
[Load slowdown from occupants](https://github.com/Andreas-W/GeneralsGameCode_Modding/wiki/Objects-&-Modules#load-slowdown-from-occupants).

Notes:
* Damaged values (`SpeedDamaged`) scale by the same amount; no double penalty.
* Survives locomotor changes (`SET_NORMAL_UPGRADED`, `SET_PANIC`).
* Immobile containers (garrisoned buildings) are unaffected.
* A loaded transport slows its group like any slow unit.
* Changes the simulation; replays must use the same setting. At `0%` nothing changes.
