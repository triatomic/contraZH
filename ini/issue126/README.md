# C27 Spartan transport configuration

Apply the four patches to the mod's `Data/INI` directory with `git apply` after installing an
engine built from this PR.

* The Air Force C27 carries six infantry slots and shows six passenger inventory buttons.
* One drop shot releases every passenger into its own selectable parachute.
* The drop button is disabled while empty. Loading infantry enables it again.
* The weapon has unlimited clip ammunition. Passengers supply the drop payload.
* The firing range is 25, so the plane approaches the ordered location before releasing cargo.
* The weapon keeps its existing 0.1 damage so the engine can select it for ground attacks.

The original predefined infantry OCL remains available for other weapons.
