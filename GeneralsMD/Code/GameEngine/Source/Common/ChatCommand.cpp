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

// FILE: ChatCommand.cpp ////////////////////////////////////////////////////////////////////////
// Desc: Parsing and storage for ChatCommand blocks defined in the optional ChatCommands.ini.
//////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ChatCommand.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Upgrade.h"
#include "Common/GameAudio.h"
#include "Common/MiscAudio.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/KindOf.h"
#include "Common/ModelState.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/View.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/WeaponSetType.h"
#include "GameLogic/ArmorSet.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"

//-------------------------------------------------------------------------------------------------
ChatCommandStore* TheChatCommandStore = nullptr;

// Names accepted by "SetSelectedOwner", in OwnerTarget order starting at OWNER_ENEMIES.
static const char *TheChatCommandOwnerNames[] =
{
	"ENEMIES",
	"NEUTRAL",
	"ALLIES",
	"SELF",
	nullptr
};

// What "AddHealth" grants when the INI leaves the amount out, past any unit's own max health.
static const Real DEFAULT_ADD_HEALTH = 100000.0f;

const FieldParse ChatCommand::s_fieldParseTable[] =
{
	{ "AddMoney",		INI::parseInt,			nullptr,	offsetof( ChatCommand, m_addMoney ) },
	{ "AddRank",		INI::parseUnsignedInt,	nullptr,	offsetof( ChatCommand, m_addRank ) },
	{ "ReadyTimers",	INI::parseBool,			nullptr,	offsetof( ChatCommand, m_readyTimers ) },
	{ "SpawnObjectAtCursor",	ChatCommand::parseSpawnObjectAtCursor,	nullptr,	0 },
	{ "TogglePrerequisites",	INI::parseBool,			nullptr,	offsetof( ChatCommand, m_togglePrerequisites ) },
	{ "ToggleInfiniteEnergy",	INI::parseBool,			nullptr,	offsetof( ChatCommand, m_toggleInfiniteEnergy ) },
	{ "GrantAllUpgrades",		INI::parseBool,			nullptr,	offsetof( ChatCommand, m_grantAllUpgrades ) },
	{ "AddVeterancyLevel",		INI::parseInt,			nullptr,	offsetof( ChatCommand, m_addVeterancyLevel ) },
	{ "AddSalvageTier",			INI::parseInt,			nullptr,	offsetof( ChatCommand, m_addSalvageTier ) },
	{ "ProductionSpeedMultiplier",	INI::parseReal,		nullptr,	offsetof( ChatCommand, m_productionSpeedMultiplier ) },
	{ "SetSelectedOwner",		INI::parseIndexList,	TheChatCommandOwnerNames,	offsetof( ChatCommand, m_setSelectedOwner ) },
	{ "AddHealth",				ChatCommand::parseAddHealth,		nullptr,	0 },
	{ "KillSelected",			INI::parseBool,			nullptr,	offsetof( ChatCommand, m_killSelected ) },
	{ NULL, NULL, 0, 0 }  // keep this last
};

//-------------------------------------------------------------------------------------------------
// Set a weapon-salvager's crate-upgrade tier (0=none, 1=ONE, 2=TWO) to match newTier.
static void setWeaponSalvageTier( Object *obj, Int newTier )
{
	obj->clearWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE );
	obj->clearWeaponSetFlag( WEAPONSET_CRATEUPGRADE_TWO );
	if (newTier >= 2)
		obj->setWeaponSetFlag( WEAPONSET_CRATEUPGRADE_TWO );
	else if (newTier == 1)
		obj->setWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE );
}

//-------------------------------------------------------------------------------------------------
// Set an armor-salvager's crate-upgrade tier (0=none, 1=ONE, 2=TWO), including model visuals.
static void setArmorSalvageTier( Object *obj, Int newTier )
{
	obj->clearArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE );
	obj->clearArmorSetFlag( ARMORSET_CRATE_UPGRADE_TWO );
	obj->clearModelConditionState( MODELCONDITION_ARMORSET_CRATEUPGRADE_ONE );
	obj->clearModelConditionState( MODELCONDITION_ARMORSET_CRATEUPGRADE_TWO );
	if (newTier >= 2)
	{
		obj->setArmorSetFlag( ARMORSET_CRATE_UPGRADE_TWO );
		obj->setModelConditionState( MODELCONDITION_ARMORSET_CRATEUPGRADE_TWO );
	}
	else if (newTier == 1)
	{
		obj->setArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE );
		obj->setModelConditionState( MODELCONDITION_ARMORSET_CRATEUPGRADE_ONE );
	}
}

//-------------------------------------------------------------------------------------------------
void ChatCommand::parseAddHealth( INI * /*ini*/, void *instance, void * /*store*/, const void * /*userData*/ )
{
	ChatCommand *command = (ChatCommand *)instance;
	const char *token = INI::getNextTokenOrNull();

	command->m_addHealth = (token != nullptr) ? INI::scanReal( token ) : DEFAULT_ADD_HEALTH;
}

//-------------------------------------------------------------------------------------------------
// Snapshot the selection, since giving an object away or killing it changes what is selected.
static void gatherSelectedObjects( std::vector<Object *>& objects )
{
	const DrawableList *selected = TheInGameUI ? TheInGameUI->getAllSelectedDrawables() : nullptr;
	if (selected == nullptr)
		return;

	for (DrawableList::const_iterator it = selected->begin(); it != selected->end(); ++it)
	{
		Drawable *draw = *it;
		Object *obj = draw ? draw->getObject() : nullptr;
		if (obj)
			objects.push_back( obj );
	}
}

//-------------------------------------------------------------------------------------------------
// The player "SetSelectedOwner" hands the selection to: the local player for SELF, the neutral
// player for NEUTRAL, otherwise the first player holding that relationship to the local player.
static Player *findOwnerForChatCommand( Int ownerTarget )
{
	if (ThePlayerList == nullptr)
		return nullptr;

	if (ownerTarget == ChatCommand::OWNER_SELF)
		return ThePlayerList->getLocalPlayer();

	if (ownerTarget == ChatCommand::OWNER_NEUTRAL)
		return ThePlayerList->getNeutralPlayer();

	const Player *localPlayer = ThePlayerList->getLocalPlayer();
	if (localPlayer == nullptr)
		return nullptr;

	const Player *neutralPlayer = ThePlayerList->getNeutralPlayer();
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer( i );
		if (player == nullptr || player == localPlayer)
			continue;
		// the neutral player is everyone's neutral, and NEUTRAL was already answered above
		if (player == neutralPlayer)
			continue;
		// setTeam bounces objects given to a defeated player back to neutral, so skip them here
		if (!player->isPlayerActive())
			continue;
		if (localPlayer->getRelationship( player->getDefaultTeam() ) == (Relationship)ownerTarget)
			return player;
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void ChatCommand::parseSpawnObjectAtCursor( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	ChatCommand *command = (ChatCommand *)instance;
	command->m_spawnObjectAtCursor = ini->getNextAsciiString();
	// Presence of the key is what makes this a spawn command; the value is only the default object,
	// and is allowed to be blank so the object can be typed after the command instead.
	command->m_isSpawnObjectCommand = TRUE;
}

//-------------------------------------------------------------------------------------------------
// Find an ObjectTemplate by name for a chat command. Unlike a plain findTemplate() call this is
// forgiving, because the name can come from whatever the user typed: the assert on a miss is
// suppressed, and an exact lookup that fails falls back to a case-insensitive search so
// "americatankcrusader" still finds "AmericaTankCrusader".
static const ThingTemplate *findTemplateForChatCommand( const AsciiString& name )
{
	if (TheThingFactory == nullptr || name.isEmpty())
	{
		return nullptr;
	}

	// Exact match first; this is the hash lookup, and the only one that can hit for INI-supplied names.
	const ThingTemplate *tmpl = TheThingFactory->findTemplate( name, FALSE );
	if (tmpl)
	{
		return tmpl;
	}

	for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t; t = t->friend_getNextTemplate())
	{
		if (t->getName().compareNoCase( name ) == 0)
		{
			return t;
		}
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void ChatCommand::execute( const AsciiString& args ) const
{
	// Money: positive grants cash, negative removes it. The local player's funds can never
	// go below zero (withdraw caps at the current balance), so a large removal just empties it.
	if (m_addMoney != 0)
	{
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		Money *money = player ? player->getMoney() : nullptr;
		if (money)
		{
			// Suppress the built-in deposit/withdraw sound; play the cash-hack sound instead (below).
			if (m_addMoney > 0)
				money->deposit( (UnsignedInt)m_addMoney, FALSE );
			else
				money->withdraw( (UnsignedInt)(-m_addMoney), FALSE );

			// Play the same sound the Cash Hack special power uses, for the local player.
			if (TheAudio && TheAudio->getMiscAudio())
			{
				AudioEventRTS sound = TheAudio->getMiscAudio()->m_moneyDepositSound;
				sound.setPlayerIndex( player->getPlayerIndex() );
				TheAudio->addAudioEvent( &sound );
			}
		}
	}

	// Rank: grant additional rank levels. setRankLevel() clamps to the maximum rank,
	// so over-granting just tops the player out.
	if (m_addRank > 0)
	{
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (player)
			player->setRankLevel( player->getRankLevel() + (Int)m_addRank );
	}

	// ReadyTimers: set every special power timer the player owns to ready (available now).
	if (m_readyTimers)
	{
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (player)
		{
			const UnsignedInt now = TheGameLogic->getFrame();
			for (Player::PlayerTeamList::const_iterator pt = player->getPlayerTeams()->begin();
				pt != player->getPlayerTeams()->end(); ++pt)
			{
				for (DLINK_ITERATOR<Team> teamIt = (*pt)->iterate_TeamInstanceList(); !teamIt.done(); teamIt.advance())
				{
					Team *team = teamIt.cur();
					if (!team)
						continue;

					for (DLINK_ITERATOR<Object> objIt = team->iterate_TeamMemberList(); !objIt.done(); objIt.advance())
					{
						Object *obj = objIt.cur();
						if (!obj)
							continue;

						for (BehaviorModule **b = obj->getBehaviorModules(); b && *b; ++b)
						{
							SpecialPowerModuleInterface *sp = (*b)->getSpecialPower();
							if (sp)
							{
								// Ready the per-object timer (superweapons) and the player's shared
								// timer (generals' shortcut powers read their readiness from there).
								sp->setReadyFrame( now );
								const SpecialPowerTemplate *temp = sp->getSpecialPowerTemplate();
								if (temp)
									player->expressSpecialPowerReadyFrame( temp, now );
							}
						}
					}
				}
			}
		}
	}

	// SpawnObjectAtCursor: create one instance of the named ObjectTemplate for the local player,
	// placed on the terrain under the mouse cursor. A name typed after the command wins over the
	// INI one, so a single command can spawn anything; with nothing typed the INI name is used.
	// The command only counts as a spawn command if one of the two supplied a name.
	AsciiString spawnName = args.isEmpty() ? m_spawnObjectAtCursor : args;
	if (m_isSpawnObjectCommand && !spawnName.isEmpty())
	{
		const ThingTemplate *tmpl = findTemplateForChatCommand( spawnName );
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		Team *team = player ? player->getDefaultTeam() : nullptr;
		if (tmpl && team && TheMouse && TheTacticalView)
		{
			Coord3D pos;
			const MouseIO *mouseIO = TheMouse->getMouseStatus();
			TheTacticalView->screenToTerrain( &mouseIO->pos, &pos );

			Object *obj = TheThingFactory->newObject( tmpl, team );
			if (obj)
			{
				obj->setPosition( &pos );
				obj->setOrientation( 0.0f );

				// newObject only runs the onCreate half of creation, so finish the object off the
				// way ProductionUpdate does after a factory builds one.
				for (BehaviorModule **m = obj->getBehaviorModules(); m && *m; ++m)
				{
					CreateModuleInterface *create = (*m)->getCreate();
					if (create != nullptr)
					{
						create->onBuildComplete();
					}
				}
			}
		}
		else if (!tmpl)
		{
			// A typo is the normal failure now that the name can be typed, so say so on screen;
			// the log alone is invisible in a release cheats build.
			DEBUG_LOG((">>> CHAT COMMAND SpawnObjectAtCursor: ThingTemplate '%s' not found.", spawnName.str()));
			if (TheInGameUI)
			{
				UnicodeString msg;
				UnicodeString wideName;
				wideName.translate( spawnName );
				msg.format( L"Unknown object: %s", wideName.str() );
				TheInGameUI->message( msg );
			}
		}
	}
	else if (m_isSpawnObjectCommand && TheInGameUI)
	{
		// Declared as a spawn command but neither the INI nor the user named anything to spawn.
		TheInGameUI->message( UnicodeString( L"Usage: type an object name after the command." ) );
	}

	// TogglePrerequisites: flip ignoring of unit/building build prereqs for the local player
	// (science prereqs still apply). Mark the control bar dirty so build buttons re-evaluate.
	if (m_togglePrerequisites)
	{
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (player)
		{
			player->toggleIgnoreUnitPrereqs();
			if (TheControlBar)
				TheControlBar->markUIDirty();
		}
	}

	// ToggleInfiniteEnergy: flip infinite power for the local player. The setter refreshes
	// power-dependent objects; mark the control bar dirty so the power UI updates.
	if (m_toggleInfiniteEnergy)
	{
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (player)
		{
			player->toggleInfinitePower();
			if (TheControlBar)
				TheControlBar->markUIDirty();
		}
	}

	// GrantAllUpgrades: complete every player-type upgrade for the local player.
	if (m_grantAllUpgrades)
	{
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (player && TheUpgradeCenter)
		{
			for (UpgradeTemplate *tmpl = TheUpgradeCenter->firstUpgradeTemplate();
				tmpl != nullptr; tmpl = tmpl->friend_getNext())
			{
				if (tmpl->getUpgradeType() != UPGRADE_TYPE_PLAYER)
					continue;
				if (player->hasUpgradeComplete( tmpl ))
					continue;
				player->addUpgrade( tmpl, UPGRADE_STATUS_COMPLETE );
			}
		}
	}

	// AddVeterancyLevel: change veterancy of the player's currently selected (trainable) units by
	// m_addVeterancyLevel levels (negative demotes), clamped to the valid range.
	if (m_addVeterancyLevel != 0 && TheInGameUI)
	{
		const DrawableList *selected = TheInGameUI->getAllSelectedLocalDrawables();
		if (selected)
		{
			for (DrawableList::const_iterator it = selected->begin(); it != selected->end(); ++it)
			{
				Drawable *draw = *it;
				Object *obj = draw ? draw->getObject() : nullptr;
				ExperienceTracker *exp = obj ? obj->getExperienceTracker() : nullptr;
				if (!exp || !exp->isTrainable())
					continue;

				Int newLevel = (Int)exp->getVeterancyLevel() + m_addVeterancyLevel;
				if (newLevel < LEVEL_FIRST)
					newLevel = LEVEL_FIRST;
				else if (newLevel > LEVEL_LAST)
					newLevel = LEVEL_LAST;

				exp->setVeterancyLevel( (VeterancyLevel)newLevel );
			}
		}
	}

	// AddSalvageTier: change the crate-upgrade tier of the player's selected salvagers by
	// m_addSalvageTier steps (negative removes), clamped 0..2. Weapon tier only applies to
	// weapon-salvagers, armor tier only to armor-salvagers; other units are unaffected.
	if (m_addSalvageTier != 0 && TheInGameUI)
	{
		const DrawableList *selected = TheInGameUI->getAllSelectedLocalDrawables();
		if (selected)
		{
			for (DrawableList::const_iterator it = selected->begin(); it != selected->end(); ++it)
			{
				Drawable *draw = *it;
				Object *obj = draw ? draw->getObject() : nullptr;
				if (!obj)
					continue;

				Bool changed = FALSE;

				if (obj->isKindOf( KINDOF_WEAPON_SALVAGER ))
				{
					Int cur = obj->testWeaponSetFlag( WEAPONSET_CRATEUPGRADE_TWO ) ? 2
							: obj->testWeaponSetFlag( WEAPONSET_CRATEUPGRADE_ONE ) ? 1 : 0;
					Int next = cur + m_addSalvageTier;
					next = next < 0 ? 0 : (next > 2 ? 2 : next);
					if (next != cur)
					{
						setWeaponSalvageTier( obj, next );
						changed = TRUE;
					}
				}

				if (obj->isKindOf( KINDOF_ARMOR_SALVAGER ))
				{
					Int cur = obj->testArmorSetFlag( ARMORSET_CRATE_UPGRADE_TWO ) ? 2
							: obj->testArmorSetFlag( ARMORSET_CRATE_UPGRADE_ONE ) ? 1 : 0;
					Int next = cur + m_addSalvageTier;
					next = next < 0 ? 0 : (next > 2 ? 2 : next);
					if (next != cur)
					{
						setArmorSalvageTier( obj, next );
						changed = TRUE;
					}
				}

				// play the salvage crate pickup sound on the unit when its tier changed.
				if (changed && TheAudio && TheAudio->getMiscAudio())
				{
					AudioEventRTS sound = TheAudio->getMiscAudio()->m_crateSalvage;
					sound.setObjectID( obj->getID() );
					TheAudio->addAudioEvent( &sound );
				}
			}
		}
	}

	// ProductionSpeedMultiplier: set the local player's global build-speed multiplier (>1 builds faster).
	// A value of 0 means the attribute was not present in the INI, so leave the multiplier untouched.
	// Mark the control bar dirty so any build-time UI re-evaluates.
	if (m_productionSpeedMultiplier > 0.0f)
	{
		Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (player)
		{
			player->setProductionSpeedMultiplier( m_productionSpeedMultiplier );
			if (TheControlBar)
				TheControlBar->markUIDirty();
		}
	}

	// SetSelectedOwner: hand the selected objects to another player. The selection is dropped
	// first, because the local player may not own what it ends up with.
	if (m_setSelectedOwner != OWNER_UNCHANGED && TheInGameUI)
	{
		Player *newOwner = findOwnerForChatCommand( m_setSelectedOwner );
		Team *newTeam = newOwner ? newOwner->getDefaultTeam() : nullptr;
		if (newTeam == nullptr)
		{
			// a skirmish with no teammate has nobody to be allied with, which is worth saying out loud
			TheInGameUI->message( UnicodeString( L"No player to give the selection to." ) );
		}
		else
		{
			std::vector<Object *> objects;
			gatherSelectedObjects( objects );

			if (!objects.empty())
				TheInGameUI->deselectAllDrawables();

			for (std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); ++it)
			{
				Object *obj = *it;
				obj->setTeam( newTeam );

				// setTeam leaves these to the caller, as updateTeamAndPlayerStuff in ScriptActions.cpp does
				obj->updateUpgradeModules();
				Drawable *draw = obj->getDrawable();
				if (draw)
				{
					if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
						draw->setIndicatorColor( obj->getNightIndicatorColor() );
					else
						draw->setIndicatorColor( obj->getIndicatorColor() );
				}
			}
		}
	}

	// AddHealth: raise the selected objects' max health and current health by the same amount, so
	// the units end up tougher rather than merely topped up. Damage takes the ordinary damage path
	// instead, because setMaxHealth only clamps health and never runs the death sequence.
	if (m_addHealth != 0.0f && TheInGameUI)
	{
		std::vector<Object *> objects;
		gatherSelectedObjects( objects );

		for (std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); ++it)
		{
			Object *obj = *it;
			BodyModuleInterface *body = obj->getBodyModule();
			if (body == nullptr)
				continue;

			if (m_addHealth > 0.0f)
			{
				body->setMaxHealth( body->getMaxHealth() + m_addHealth, ADD_CURRENT_HEALTH_TOO );
			}
			else if (!obj->isEffectivelyDead())
			{
				DamageInfo damageInfo;
				damageInfo.in.m_damageType = DAMAGE_UNRESISTABLE;
				damageInfo.in.m_deathType = DEATH_NORMAL;
				damageInfo.in.m_sourceID = INVALID_ID;
				damageInfo.in.m_amount = -m_addHealth;
				obj->attemptDamage( &damageInfo );
			}
		}
	}

	// KillSelected: kill the selected objects outright.
	if (m_killSelected && TheInGameUI)
	{
		std::vector<Object *> objects;
		gatherSelectedObjects( objects );

		std::vector<Object *> killable;
		for (std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); ++it)
		{
			Object *obj = *it;
			const BodyModuleInterface *body = obj->getBodyModule();
			// an InactiveBody cannot be killed, and asserts if asked
			if (!obj->isEffectivelyDead() && body && body->getMaxHealth() > 0.0f)
				killable.push_back( obj );
		}

		if (!killable.empty())
			TheInGameUI->deselectAllDrawables();

		for (std::vector<Object *>::iterator it = killable.begin(); it != killable.end(); ++it)
			(*it)->kill();
	}

	// Notify the player that the command ran.
	if (TheInGameUI)
	{
		UnicodeString uName;
		uName.translate( m_name );
		UnicodeString msg;
		msg.format( L"Chat command executed: %s", uName.str() );
		TheInGameUI->message( msg );
	}
}

//-------------------------------------------------------------------------------------------------
ChatCommandStore::~ChatCommandStore()
{
	clear();
}

//-------------------------------------------------------------------------------------------------
void ChatCommandStore::clear()
{
	for (std::vector<ChatCommand*>::iterator it = m_commands.begin(); it != m_commands.end(); ++it)
		delete *it;
	m_commands.clear();
}

//-------------------------------------------------------------------------------------------------
void ChatCommandStore::reset()
{
	// Chat commands are static definitions loaded once from ChatCommands.ini; they must persist
	// across game resets (like ThingTemplates), so do not clear them here.
}

//-------------------------------------------------------------------------------------------------
const ChatCommand* ChatCommandStore::findChatCommand( const AsciiString& name ) const
{
	for (std::vector<ChatCommand*>::const_iterator it = m_commands.begin(); it != m_commands.end(); ++it)
	{
		if ((*it)->getName().compareNoCase(name) == 0)
			return *it;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void ChatCommandStore::parseChatCommandDefinition( INI* ini )
{
	// read the command name that follows the "ChatCommand" keyword
	AsciiString name;
	name.set( ini->getNextToken() );

	if (!TheChatCommandStore)
		return;

	// command names must be unique; a duplicate would shadow the earlier definition at dispatch.
	if (TheChatCommandStore->findChatCommand( name ) != nullptr)
	{
		DEBUG_CRASH(("[LINE: %d - FILE: '%s'] Duplicate ChatCommand '%s'", ini->getLineNum(), ini->getFilename().str(), name.str()));
		throw INI_INVALID_DATA;
	}

	ChatCommand* command = new ChatCommand;
	command->setName( name );

	// consume the block to its "End" token; no attributes are defined yet
	ini->initFromINI( command, command->getFieldParse() );

	TheChatCommandStore->m_commands.push_back( command );

	DEBUG_LOG((">>> ADDED CHAT COMMAND '%s'", command->getName().str()));
}

//-------------------------------------------------------------------------------------------------
/*static*/ void INI::parseChatCommandDefinition( INI* ini )
{
	ChatCommandStore::parseChatCommandDefinition( ini );
}
