# FXList.ini

## FXListExtend

* `FXListExtend <NewList> <ParentList>` - (Plays every nugget of the parent, then the nuggets in
its own block.)

```
FXListExtend FX_BattleMasterDeathBig FX_BattleMasterDeath
  ViewShake
    Type = STRONG
  End
End
```

Notes:
* The parent must be defined above the extending block. A missing parent asserts in debug builds and
the block loads without it.
* Nuggets only add. A parent nugget cannot be changed or removed.
* The copy is taken at load. Redefining the parent later does not change the extended list.
* Naming the list itself as parent appends nuggets to it.
* Nuggets stay in memory until the game exits, as `ObjectCreationList` nuggets always have.
Redefining an `FXList`, for example in a map.ini, keeps the old nuggets, so each map load that
redefines lists adds a little memory.
