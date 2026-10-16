# Weapon.ini

## LineOfSightGraceHeight

* `LineOfSightGraceHeight = 0` - (Default. Terrain that rises this many feet above the firing line
does not block the shot.)

Units with `ATTACK_NEEDS_LINE_OF_SIGHT` only fire when the terrain between them and the target stays
below the firing line. The line runs from the top of the shooter to the target's centre, or to the
ground for a force-attacked spot, so long-range shots graze the terrain near the target and small
bumps block them. The unit then drives closer to fire. A grace height lets terrain rise that far
above the line before it counts as blocking.

```
Weapon EmperorSniperGun
  AttackRange            = 325.0
  LineOfSightGraceHeight = 8.0
End
```

Notes:
* Only units with `ATTACK_NEEDS_LINE_OF_SIGHT` test terrain before an attack, so the key does nothing
for others there. Immobile units skip that test and ignore the key.
* The current weapon's grace also applies when a unit scans for enemies it can see.
* A negative value is an INI error.
* A hill taller than the grace height still blocks the shot.
* Works in `WeaponExtend`.

Changes when units fire; affects replays when set.
([#120](https://github.com/triatomic/contraZH/issues/120))
