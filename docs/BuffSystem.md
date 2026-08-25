# Overview

Added a new system to apply and track Buffs and Debuffs. A new INI file can be added (BuffTemplate.ini) to define templates that can be used in various modules.

## Buff Application

- via module: See BuffUpdate
- via weapon: TODO

# BuffTemplate.ini

## BuffTemplate definition

Each BuffTemplate has
- a name
- a number of modifiers (gameplay effects)
- a number of effects (visual effects)
- generic parameters

### Generic Parameters

- MaxStacksSize = 1  ; Maximum number of stacks that can be applied
- HasPriorityOver = <BuffSystem names>  ; The BuffTemplate will override active buffs listed here if they are applied on the same object.

## Modifiers

### ValueModifer
Scalars to various unit stats. These can be applied multiple times if stack size > 1

- MovementSpeedScalar = 1.0  ; increase/decrease a units movement speed
- ArmorDamageScalar = 1.0  ; increase/decrease how much damage unit takes
- SightRangeScalar = 1.0  ; increase/decrease a units' sight range

### FlagModifier
Flag modifiers are binary modifiers, i.e. multiple stacks from the same buff will not increase the effect.

- WeaponBonus = <WeaponBonus type>  ; applies the weaponBonus for the unit
- WeaponBonusAgainst = <WeaponBonus type>  ; applies the weaponBonus when attacking the target (like Avenger targetDesignator)
- WeaponSetFlag = <WeaponSet type>  ; applies the weaponset type for the unit (this only works if the unit has a corresponding weaponset defined)
- ArmorSetFlag = <ArmorSet type> ; same as above
- StatusToSet = <StatusName>  ; set the status for the unit.

## Effects

### ColorTintEffect

- TintStatusType = <TintStatus type>  ; Apply the color tint effect

### ParticleSystemEffect

- ParticleSystem = <ParticleSystemEntry>   ; attaches the particlesystem to the object

### 

# Examples

```
BuffTemplate SupW_BuffForceShieldOne
  ValueModifier
    ArmorDamageScalar  = 0.7
  End
  
  ColorTintEffect
    TintStatusType = FORCE_FIELD
  End
End

BuffTemplate SupW_BuffCryoBombSlow
  ValueModifier
    MovementSpeedScalar  = 0.4
  End

  FlagModifier
    WeaponBonus = CRYO_THREE
  End
  
  ColorTintEffect
    TintStatusType = FROZEN
  End
  
  ParticleSystemEffect
    ParticleSystem = CryoBuffEffectIceParticles
  End
  
End
```