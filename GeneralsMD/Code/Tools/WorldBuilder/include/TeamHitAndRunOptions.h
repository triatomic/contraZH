#pragma once

#include "Common/Dict.h"
#include "Common/Team.h"
#include "Common/WellKnownKeys.h"
#include "resource.h"

struct TeamHitAndRunField
{
	Int control;
	NameKeyType key;
	Int minimum;
	Int maximum;
	Int defaultValue;
	const char *help;
};

inline const TeamHitAndRunField *getTeamHitAndRunField(Int control)
{
	static const TeamHitAndRunField fields[] =
	{
		{ IDC_TEAM_HIT_AND_RUN_RELOAD_PCT, TheKey_teamHitAndRunReloadPercent, 1, 100, TeamTemplateInfo::HIT_AND_RUN_DEFAULT_PERCENT,
			"Attack when this percentage of attackers have fired and reloaded in the current volley. If fewer units fired, wait for all of them." },
		{ IDC_TEAM_HIT_AND_RUN_DELAY, TheKey_teamHitAndRunDelaySeconds, 0, TeamTemplateInfo::HIT_AND_RUN_MAX_DELAY, TeamTemplateInfo::HIT_AND_RUN_DEFAULT_DELAY,
			"Attack this many seconds after the team starts retreating." },
		{ IDC_TEAM_HIT_AND_RUN_RETREAT, TheKey_teamHitAndRunRetreatPercent, 1, 100, TeamTemplateInfo::HIT_AND_RUN_DEFAULT_PERCENT,
			"Retreat after this percentage of the team's attackers have fired in the current volley." },
		{ IDC_TEAM_HIT_AND_RUN_RETREAT_DELAY, TheKey_teamHitAndRunRetreatDelaySeconds, 0, TeamTemplateInfo::HIT_AND_RUN_MAX_DELAY, TeamTemplateInfo::HIT_AND_RUN_DEFAULT_DELAY,
			"Retreat this many seconds after the team's first shot in the current volley." }
	};

	for (Int i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
	{
		if (fields[i].control == control)
		{
			return &fields[i];
		}
	}
	return nullptr;
}

inline Int getTeamHitAndRunValue(const Dict *dict, const TeamHitAndRunField &field)
{
	Bool exists;
	const Int value = dict->getInt(field.key, &exists);
	return exists ? std::max(field.minimum, std::min(field.maximum, value)) : field.defaultValue;
}

inline Int setTeamHitAndRunValue(Dict *dict, const TeamHitAndRunField &field, const char *text)
{
	char *end = nullptr;
	const long value = strtol(text, &end, 10);
	const Int normalized = end == text || *end ? field.defaultValue :
		(Int)std::max((long)field.minimum, std::min((long)field.maximum, value));
	dict->setInt(field.key, normalized);
	return normalized;
}
