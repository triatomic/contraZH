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
* The parent must be defined above the extending block. A missing parent is an INI error.
* Nuggets only add. A parent nugget cannot be changed or removed.
* The parent stays linked, so redefining it later, for example in a map.ini, changes the extended
list too. A plain `FXList` block with the same name drops the link.
* Naming the list itself as parent appends nuggets to it. A list cannot extend one of its own
descendants.
