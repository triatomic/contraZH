# Campaign

How the single player campaigns are listed, started and unlocked, and the `Campaign.ini` keys
behind it.

## Campaign screen

* The single player menu has one `Campaign` button in place of the USA, China and GLA buttons. It
opens a screen listing every `Campaign` block in `Campaign.ini` except challenge campaigns, so a mod
adds a campaign by adding a block.
* `CampaignGroupLabel` on a `Campaign` block names the tab the campaign sits under. Campaigns
without the key share a `Campaign` tab. The tab row stays hidden while every campaign is in one
tab; the screen holds six tabs, and with more groups the sixth becomes a `Campaign` tab holding
every group past the fifth.
* The left list holds the tab's campaigns, named by `CampaignNameLabel`. The right list holds the
missions in `NextMission` order from `FirstMission`, numbered, and named by the new
`MissionNameLabel` key on the `Mission` block. A mission without the key takes the map's display
name, or the mission id when the map cache does not know the map. The map preview sits beside the
list when the cache knows the map.
* A mission unlocks once a mission leading to it has been won, and `FirstMission` is always open.
Locked missions are greyed out and the Play button refuses them. Won missions are recorded in
`CampaignProgress.ini` in the user data folder next to `Options.ini`, one `campaign.mission = 1`
line each.
* Opening the screen selects the campaign last played and its first mission still to be won, so
going back to an earlier chapter is one click.
* Easy, Medium and Hard radios pick the difficulty, remembered as `CampaignDifficulty` in
`Options.ini`. A started mission plays as a normal campaign mission, so winning it moves on to
`NextMission`.
* The Contra mod ships the loose `MainMenu.wnd`, `CampaignChapters.wnd` and
`WindowTransitions.ini` the screen needs, plus the `TOOLTIP:CampaignChapters`, `CAMPAIGN:Generals`
and `CAMPAIGN:ZeroHour` strings. An older exe ignores the button.

Contra puts its vanilla campaigns under a Generals tab and the training mission under a Zero Hour
tab, with `CAMPAIGN:Generals` and `CAMPAIGN:ZeroHour` defined in `Generals.str`:

```ini
Campaign USA
  CampaignNameLabel CAMPAIGN:USA
  CampaignGroupLabel CAMPAIGN:Generals
  FirstMission Mission01
  Mission Mission01
    Map Maps\USA01\USA01.map
    MissionNameLabel CAMPAIGN:USA01
    NextMission Mission02
  END
  ...
END

Campaign TRAINING
  CampaignNameLabel CAMPAIGN:TRAINING
  CampaignGroupLabel CAMPAIGN:ZeroHour
  FirstMission Mission01
  ...
END
```

Note: to open every mission, for testing or after a reinstall, put `UnlockAll = yes` on its own
line in `CampaignProgress.ini` (create the file if it does not exist) and reopen the screen.

## Mission saves

* The automatic save made between missions ends its name with the difficulty, as in
`Mission Start - USA 2 - Hard`. The labels come from `GUI:DifficultyEasy`, `GUI:DifficultyNormal`
and `GUI:DifficultyHard` in `Generals.str`, and read `Easy`, `Normal` and `Hard` when the strings
are missing.

## Generals' Challenge

* The single player `Challenge` button opens the challenge screen directly. Easy, Medium and Hard
buttons sit left of Play there and share the campaign's `CampaignDifficulty` in `Options.ini`.
* `ChallengeMenu.wnd` may define `ButtonEasy`, `ButtonMedium` and `ButtonHard` push buttons to
place them itself; any it lacks are built in the Play button's look.
* A challenge map colors the player like its `ThePlayer` placeholder when WorldBuilder gives that
player a color. Maps without one keep the random color.
