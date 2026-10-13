/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: ScriptEngineCLP.cpp //////////////////////////////////////////////////////////////////////
// CLP counter, kills to deaths ratio and attack priority set handlers on the script engine.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/ObjectTypes.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/ScriptCLP.h"

#include <vector>

//-------------------------------------------------------------------------------------------------
static ScriptCLPState::KDRatio *clpFindKDRatio(Parameter *param, Bool create)
{
	if (!TheScriptCLPState)
	{
		return nullptr;
	}
	return TheScriptCLPState->findKDRatio(param->getString(), create);
}

//-------------------------------------------------------------------------------------------------
/** Sums the build cost of events newer than *read, then marks them read. */
//-------------------------------------------------------------------------------------------------
static Real clpSumBuildCost(const ScriptCLPState::EventList &events, UnsignedInt *read)
{
	Real total = 0.0f;
	for (size_t i = 0; i < events.size(); ++i)
	{
		const ThingTemplate *thing = events[i].thing;
		if (events[i].seq > *read && thing && thing->friend_getBuildCost() > 0)
		{
			total += (Real)thing->friend_getBuildCost();
		}
	}
	*read = TheScriptCLPState->getLastEventSeq();
	return total;
}

//-------------------------------------------------------------------------------------------------
static void clpFindTemplates(const AsciiString &objectType, std::vector<const ThingTemplate *> &templates)
{
	const ObjectTypes *types = TheScriptEngine->getObjectTypes(objectType);
	if (types)
	{
		for (size_t i = 0; i < types->getListSize(); ++i)
		{
			const ThingTemplate *thing = TheThingFactory->findTemplate(types->getNthInList(i), FALSE);
			if (thing)
			{
				templates.push_back(thing);
			}
		}
		return;
	}
	const ThingTemplate *thing = TheThingFactory->findTemplate(objectType, FALSE);
	if (thing)
	{
		templates.push_back(thing);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::setCounterRandom( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 3, ("Not enough parameters."));
	Int counterNdx = pAction->getParameter(0)->getInt();
	if (counterNdx == 0)
	{
		counterNdx = allocateCounter(pAction->getParameter(0)->getString());
		pAction->getParameter(0)->friend_setInt(counterNdx);
	}
	Int low = pAction->getParameter(1)->getInt();
	Int high = pAction->getParameter(2)->getInt();
	if (low > high)
	{
		Int swap = low;
		low = high;
		high = swap;
	}
	m_counters[counterNdx].value = GameLogicRandomValue(low, high);
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::copyCounter( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	Int counterNdxA = pAction->getParameter(0)->getInt();
	if (counterNdxA == 0)
	{
		counterNdxA = allocateCounter(pAction->getParameter(0)->getString());
		pAction->getParameter(0)->friend_setInt(counterNdxA);
	}
	Int counterNdxB = pAction->getParameter(1)->getInt();
	if (counterNdxB == 0)
	{
		counterNdxB = allocateCounter(pAction->getParameter(1)->getString());
		pAction->getParameter(1)->friend_setInt(counterNdxB);
	}
	m_counters[counterNdxB].value = m_counters[counterNdxA].value;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptEngine::evaluateTwoCounters( Condition *pCondition )
{
	DEBUG_ASSERTCRASH(pCondition->getNumParameters() >= 3, ("Not enough parameters."));
	Int counterNdxA = pCondition->getParameter(0)->getInt();
	if (counterNdxA == 0)
	{
		counterNdxA = allocateCounter(pCondition->getParameter(0)->getString());
		pCondition->getParameter(0)->friend_setInt(counterNdxA);
	}
	Int counterNdxB = pCondition->getParameter(2)->getInt();
	if (counterNdxB == 0)
	{
		counterNdxB = allocateCounter(pCondition->getParameter(2)->getString());
		pCondition->getParameter(2)->friend_setInt(counterNdxB);
	}
	Int a = m_counters[counterNdxA].value;
	Int b = m_counters[counterNdxB].value;
	switch (pCondition->getParameter(1)->getInt())
	{
		case Parameter::LESS_THAN: return a < b;
		case Parameter::LESS_EQUAL: return a <= b;
		case Parameter::EQUAL: return a == b;
		case Parameter::GREATER_EQUAL: return a >= b;
		case Parameter::GREATER: return a > b;
		case Parameter::NOT_EQUAL: return a != b;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptEngine::evaluateCounterDivisible( Condition *pCondition )
{
	DEBUG_ASSERTCRASH(pCondition->getNumParameters() >= 2, ("Not enough parameters."));
	Int counterNdx = pCondition->getParameter(0)->getInt();
	if (counterNdx == 0)
	{
		counterNdx = allocateCounter(pCondition->getParameter(0)->getString());
		pCondition->getParameter(0)->friend_setInt(counterNdx);
	}
	Int divisor = pCondition->getParameter(1)->getInt();
	if (divisor == 0)
	{
		return false;
	}
	return m_counters[counterNdx].value % divisor == 0;
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::setKDRatio( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 3, ("Not enough parameters."));
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(0), TRUE);
	if (!ratio)
	{
		return;
	}
	ratio->kills = pAction->getParameter(1)->getReal();
	ratio->deaths = pAction->getParameter(2)->getReal();
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::addKDRatioKills( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(1), TRUE);
	if (ratio)
	{
		ratio->kills += pAction->getParameter(0)->getReal();
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::addKDRatioDeaths( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(1), TRUE);
	if (ratio)
	{
		ratio->deaths += pAction->getParameter(0)->getReal();
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::subKDRatioKills( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(1), TRUE);
	if (ratio)
	{
		ratio->kills -= pAction->getParameter(0)->getReal();
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::subKDRatioDeaths( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(1), TRUE);
	if (ratio)
	{
		ratio->deaths -= pAction->getParameter(0)->getReal();
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::updateKDRatioKills( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	Player *player = getPlayerFromAsciiString(pAction->getParameter(1)->getString());
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(0), TRUE);
	if (!player || !ratio)
	{
		return;
	}
	Int index = player->getPlayerIndex();
	ratio->kills += clpSumBuildCost(TheScriptCLPState->getPlayerKills(index), &ratio->readKills[index]);
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::updateKDRatioDeaths( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	Player *player = getPlayerFromAsciiString(pAction->getParameter(1)->getString());
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(0), TRUE);
	if (!player || !ratio)
	{
		return;
	}
	Int index = player->getPlayerIndex();
	ratio->deaths += clpSumBuildCost(TheScriptCLPState->getPlayerLosses(index), &ratio->readLosses[index]);
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::copyKDRatioOntoCounters( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 3, ("Not enough parameters."));
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pAction->getParameter(0), TRUE);
	if (!ratio)
	{
		return;
	}
	Int counterNdxKills = pAction->getParameter(1)->getInt();
	if (counterNdxKills == 0)
	{
		counterNdxKills = allocateCounter(pAction->getParameter(1)->getString());
		pAction->getParameter(1)->friend_setInt(counterNdxKills);
	}
	Int counterNdxDeaths = pAction->getParameter(2)->getInt();
	if (counterNdxDeaths == 0)
	{
		counterNdxDeaths = allocateCounter(pAction->getParameter(2)->getString());
		pAction->getParameter(2)->friend_setInt(counterNdxDeaths);
	}
	m_counters[counterNdxKills].value = (Int)ratio->kills;
	m_counters[counterNdxDeaths].value = (Int)ratio->deaths;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptEngine::evaluateKDRatio( Condition *pCondition )
{
	DEBUG_ASSERTCRASH(pCondition->getNumParameters() >= 3, ("Not enough parameters."));
	ScriptCLPState::KDRatio *ratio = clpFindKDRatio(pCondition->getParameter(0), FALSE);
	if (!ratio || ratio->kills == 0.0f || ratio->deaths == 0.0f)
	{
		return false;
	}
	Real kd = ratio->kills / ratio->deaths;
	Real value = pCondition->getParameter(2)->getReal();
	switch (pCondition->getParameter(1)->getInt())
	{
		case Parameter::LESS_THAN: return kd < value;
		case Parameter::LESS_EQUAL: return kd <= value;
		case Parameter::EQUAL: return kd == value;
		case Parameter::GREATER_EQUAL: return kd >= value;
		case Parameter::GREATER: return kd > value;
		case Parameter::NOT_EQUAL: return kd != value;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
// AttackPriorityInfo hides its map, so this copies every template whose priority differs from the source default.
void ScriptEngine::addPrioritySetToOther( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 2, ("Not enough parameters."));
	AttackPriorityInfo *sourceInfo = findAttackInfo(pAction->getParameter(0)->getString(), false);
	if (!sourceInfo)
	{
		return;
	}
	AttackPriorityInfo *targetInfo = findAttackInfo(pAction->getParameter(1)->getString(), true);
	if (!targetInfo || targetInfo == sourceInfo)
	{
		return;
	}

	Int sourceDefault = sourceInfo->getPriority(nullptr);
	for (const ThingTemplate *thing = TheThingFactory->firstTemplate(); thing; thing = thing->friend_getNextTemplate())
	{
		Int priority = sourceInfo->getPriority(thing);
		if (priority != sourceDefault)
		{
			targetInfo->setPriority(thing, priority);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::increasePriorityOfType( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 3, ("Not enough parameters."));
	std::vector<const ThingTemplate *> templates;
	clpFindTemplates(pAction->getParameter(1)->getString(), templates);
	if (templates.empty())
	{
		return;
	}
	AttackPriorityInfo *info = findAttackInfo(pAction->getParameter(0)->getString(), true);
	if (!info)
	{
		return;
	}

	Int amount = pAction->getParameter(2)->getInt();
	for (size_t i = 0; i < templates.size(); ++i)
	{
		info->setPriority(templates[i], info->getPriority(templates[i]) + amount);
	}
}

//-------------------------------------------------------------------------------------------------
void ScriptEngine::decreasePriorityOfType( ScriptAction *pAction )
{
	DEBUG_ASSERTCRASH(pAction->getNumParameters() >= 3, ("Not enough parameters."));
	std::vector<const ThingTemplate *> templates;
	clpFindTemplates(pAction->getParameter(1)->getString(), templates);
	if (templates.empty())
	{
		return;
	}
	AttackPriorityInfo *info = findAttackInfo(pAction->getParameter(0)->getString(), true);
	if (!info)
	{
		return;
	}

	Int amount = pAction->getParameter(2)->getInt();
	for (size_t i = 0; i < templates.size(); ++i)
	{
		info->setPriority(templates[i], max(info->getPriority(templates[i]) - amount, 0));
	}
}
