# Test Replays

## Advanced waypoint regression checks

Run `python scripts/test_advanced_waypoints.py` from a Visual Studio developer prompt, or pass
`--cxx` with a configured clang++ or g++ compiler. The check compiles the actual queue and path
snapshot methods with stubbed engine services. It checks empty-node cleanup, duplicate inclusion,
terminal-order rejection, player-specific construction counts, prepaid cancellation refunds, and
save/load of node IDs, resume targets, reverse flags, and path command IDs.

Complete these checks in a running game before release.

* Guard an object and hold Alt. The waypoint display must remain responsive.
* Send a busy dozer or worker to an abandoned scaffold, including from a mixed selection.
* Queue an $800 worker building with at least $1600 available. Starting it must take no second payment.
* Queue two buildings, save, load, and cancel the remaining order. Refund only its prepaid cost.
* Deliver construction orders after other spending exhausts the balance. Rejected orders must not
  create previews or refunds. Repeat after a terminal Guard Position order.
* In multiplayer, queue a limited building for one player. Another player must retain their own limit.
* Place another player's building over a queued preview. Both peers must make the same decision.
* Save and load during a move/reverse-move path followed by construction. The unit must retain its
  direction, finish the path once, and start the building once.

## Replay compatibility

The GeneralsReplays folder contains replays and the required maps that are tested in CI to ensure that the game is retail compatible.

You can also test with these replays locally:
- Copy the replays into a subfolder in your `%USERPROFILE%/Documents/Command and Conquer Generals Zero Hour Data/Replays` folder.
- Copy the maps into `%USERPROFILE%/Documents/Command and Conquer Generals Zero Hour Data/Maps`
- Start the test with this: (copy into a .bat file next to your executable)
```
START /B /W generalszh.exe -jobs 4 -headless -replay subfolder/*.rep > replay_check.log
echo %errorlevel%
PAUSE
```
It will run the game in the background and check that each replay is compatible. You need to use a VC6 build with optimizations and RTS_BUILD_OPTION_DEBUG = OFF, otherwise the game won't be compatible.
