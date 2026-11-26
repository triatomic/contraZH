#pragma once

// GenOnline's render and camera settings hold during an online match only; outside one the fork's values rule.

// TRUE while a GenOnline match is loaded or played.
Bool GenOnlineForkIsOnlineMatch();

// Keeps the fork's values for the restore, once per match. Call before GenOnline overwrites them.
void GenOnlineForkSaveMatchSettings();

// Applies GenOnline's frame rate limit during an online match and restores the fork's values after it. Runs every engine update.
void GenOnlineForkUpdateMatchSettings();
