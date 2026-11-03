/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// The facing drag takes mouse events before selection, scrolling and orders.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/FacingDrag.h"

#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/LookAtXlat.h"
#include "GameClient/MetaEvent.h"
#include "GameClient/Mouse.h"
#include "GameClient/SelectionXlat.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LineFormation.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

static const Int FACING_DRAG_MIN_PIXELS = 20;	///< a shorter drag is a plain move

//-------------------------------------------------------------------------------------------------
FacingDragAction facingDragOnButton( FacingDragState &state, FacingDragButton button, Bool down, const ICoord2D &pos,
	Bool canOrder )
{
	Bool &thisDown = (button == FACING_DRAG_LEFT) ? state.leftDown : state.rightDown;
	thisDown = down;

	if( down )
	{
		if( state.active || state.swallowing )
		{
			return FACING_DRAG_SWALLOW;
		}
		if( state.leftDown && state.rightDown && canOrder )
		{
			state.active = TRUE;
			state.anchor = pos;
			return FACING_DRAG_BEGIN;
		}
		return FACING_DRAG_PASS;
	}

	if( state.active )
	{
		state.active = FALSE;
		state.swallowing = state.leftDown || state.rightDown;
		return FACING_DRAG_FINISH;
	}
	if( state.swallowing )
	{
		if( !state.leftDown && !state.rightDown )
		{
			state.swallowing = FALSE;
		}
		return FACING_DRAG_SWALLOW;
	}
	return FACING_DRAG_PASS;
}

//-------------------------------------------------------------------------------------------------
Bool doesFacingDragOwnButtons( const FacingDragState &state )
{
	return state.active || state.swallowing;
}

//-------------------------------------------------------------------------------------------------
Bool isFacingDragLongEnough( const ICoord2D &from, const ICoord2D &to, Int minPixels )
{
	const Int dx = to.x - from.x;
	const Int dy = to.y - from.y;
	return dx * dx + dy * dy >= minPixels * minPixels;
}

//-------------------------------------------------------------------------------------------------
Real facingAngle( const Coord3D &from, const Coord3D &to )
{
	return atan2f( to.y - from.y, to.x - from.x );
}

//-------------------------------------------------------------------------------------------------
// Absolute snapping keeps the preview and the order aligned.
Real snapFacingAngle( Real angle, Bool snap )
{
	if( !snap )
	{
		return angle;
	}
	const Real snapRadians = DEG_TO_RADF( 45 );
	return (Real)WWMath::Round( angle / snapRadians ) * snapRadians;
}

//-------------------------------------------------------------------------------------------------
// The player's formation style (Options.ini FormationOrder, FormationWheels, FormationSpeed), read once per session.
FacingDragTranslator::FacingDragTranslator() :
	m_hotkeyHeld(FALSE),
	m_blocking(FALSE),
	m_style(0),
	m_anchorValid(FALSE)
{
	m_swallowClick[0] = m_swallowClick[1] = FALSE;
	m_anchorWorld.zero();
	OptionPreferences prefs;
	OptionPreferences::const_iterator order = prefs.find( "FormationOrder" );
	OptionPreferences::const_iterator wheels = prefs.find( "FormationWheels" );
	OptionPreferences::const_iterator speed = prefs.find( "FormationSpeed" );
	m_style = parseFormationStyle( order != prefs.end() ? order->second.str() : nullptr,
		wheels != prefs.end() ? wheels->second.str() : nullptr,
		speed != prefs.end() ? speed->second.str() : nullptr );
}

void FacingDragTranslator::reset()
{
	endPreview();
	m_state = FacingDragState();
	m_hotkeyHeld = FALSE;
	m_swallowClick[0] = m_swallowClick[1] = FALSE;
	m_anchorValid = FALSE;
	blockOtherTranslators( FALSE );
}

//-------------------------------------------------------------------------------------------------
GameMessageDisposition FacingDragTranslator::translateGameMessage( const GameMessage *msg )
{
	const GameMessage::Type type = msg->getType();
	if( type == GameMessage::MSG_META_BEGIN_FACING_DRAG || type == GameMessage::MSG_META_END_FACING_DRAG )
	{
		const ICoord2D &pos = TheMouse->getMouseStatus()->pos;
		if( type == GameMessage::MSG_META_BEGIN_FACING_DRAG )
		{
			if( TheGlobalData->m_facingDrag && !doesFacingDragOwnButtons( m_state ) && TheInGameUI->getSelectCount() > 0 )
			{
				m_hotkeyHeld = TRUE;
				m_state = FacingDragState();
				m_state.active = TRUE;
				m_state.anchor = pos;
				m_anchorValid = TheTacticalView->screenToTerrain( &pos, &m_anchorWorld );
				blockOtherTranslators( TRUE );
				beginPreview();
			}
		}
		else if( m_hotkeyHeld )
		{
			giveOrder( pos );
			endPreview();
			m_hotkeyHeld = FALSE;
			m_state.active = FALSE;
			m_state.swallowing = m_state.leftDown || m_state.rightDown;
			blockOtherTranslators( doesFacingDragOwnButtons( m_state ) );
		}
		return DESTROY_MESSAGE;
	}

	if( m_hotkeyHeld )
	{
		if( type == GameMessage::MSG_RAW_MOUSE_POSITION )
		{
			updatePreview( msg->getArgument( 0 )->pixel );
		}
		if( type == GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN || type == GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK )
		{
			m_state.leftDown = TRUE;
		}
		else if( type == GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP )
		{
			m_state.leftDown = FALSE;
			m_swallowClick[FACING_DRAG_LEFT] = TRUE;
		}
		else if( type == GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN || type == GameMessage::MSG_RAW_MOUSE_RIGHT_DOUBLE_CLICK )
		{
			m_state.rightDown = TRUE;
		}
		else if( type == GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP )
		{
			m_state.rightDown = FALSE;
			m_swallowClick[FACING_DRAG_RIGHT] = TRUE;
		}
		if( ( type >= GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN && type <= GameMessage::MSG_RAW_MOUSE_LEFT_DRAG )
			|| ( type >= GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN && type <= GameMessage::MSG_RAW_MOUSE_RIGHT_DRAG )
			|| type == GameMessage::MSG_MOUSE_LEFT_CLICK || type == GameMessage::MSG_MOUSE_LEFT_DOUBLE_CLICK
			|| type == GameMessage::MSG_MOUSE_RIGHT_CLICK || type == GameMessage::MSG_MOUSE_RIGHT_DOUBLE_CLICK )
		{
			if( type == GameMessage::MSG_MOUSE_LEFT_CLICK || type == GameMessage::MSG_MOUSE_LEFT_DOUBLE_CLICK )
			{
				m_swallowClick[FACING_DRAG_LEFT] = FALSE;
			}
			if( type == GameMessage::MSG_MOUSE_RIGHT_CLICK || type == GameMessage::MSG_MOUSE_RIGHT_DOUBLE_CLICK )
			{
				m_swallowClick[FACING_DRAG_RIGHT] = FALSE;
			}
			return DESTROY_MESSAGE;
		}
		return KEEP_MESSAGE;
	}

	const MetaMapRec *binding = TheMetaMap->getFirstMetaMapRec();
	while( binding && binding->m_meta != GameMessage::MSG_META_BEGIN_FACING_DRAG )
	{
		binding = binding->m_next;
	}
	const Bool mouseChord = binding && binding->m_key == MK_MOUSE_LEFT_RIGHT && binding->m_usableIn != COMMANDUSABLE_NONE;
	if( !mouseChord
		&& !doesFacingDragOwnButtons( m_state ) && !m_swallowClick[0] && !m_swallowClick[1] )
	{
		return KEEP_MESSAGE;
	}

	// The option enables both the default chord and the player's hold key.
	if( !TheGlobalData->m_facingDrag && !doesFacingDragOwnButtons( m_state ) )
	{
		return KEEP_MESSAGE;
	}
	switch( msg->getType() )
	{
		// a double click is a button down as well
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
		case GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK:
			if( !mouseChord && !doesFacingDragOwnButtons( m_state ) )
			{
				return KEEP_MESSAGE;
			}
			return onButton( FACING_DRAG_LEFT, TRUE, msg );
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_DOUBLE_CLICK:
			if( !mouseChord && !doesFacingDragOwnButtons( m_state ) )
			{
				return KEEP_MESSAGE;
			}
			return onButton( FACING_DRAG_RIGHT, TRUE, msg );
		case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP:
			return onButton( FACING_DRAG_LEFT, FALSE, msg );
		case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP:
			return onButton( FACING_DRAG_RIGHT, FALSE, msg );

		case GameMessage::MSG_MOUSE_LEFT_CLICK:
		case GameMessage::MSG_MOUSE_LEFT_DOUBLE_CLICK:
			if( m_swallowClick[FACING_DRAG_LEFT] )
			{
				m_swallowClick[FACING_DRAG_LEFT] = FALSE;
				return DESTROY_MESSAGE;
			}
			break;
		case GameMessage::MSG_MOUSE_RIGHT_CLICK:
		case GameMessage::MSG_MOUSE_RIGHT_DOUBLE_CLICK:
			if( m_swallowClick[FACING_DRAG_RIGHT] )
			{
				m_swallowClick[FACING_DRAG_RIGHT] = FALSE;
				return DESTROY_MESSAGE;
			}
			break;

		case GameMessage::MSG_RAW_MOUSE_LEFT_DRAG:
		case GameMessage::MSG_RAW_MOUSE_RIGHT_DRAG:
			if( m_state.active || m_state.swallowing )
			{
				return DESTROY_MESSAGE;
			}
			break;

		case GameMessage::MSG_RAW_MOUSE_POSITION:
			if( m_state.active )
			{
				updatePreview( msg->getArgument( 0 )->pixel );
			}
			break;

		default:
			break;
	}
	return KEEP_MESSAGE;
}

//-------------------------------------------------------------------------------------------------
GameMessageDisposition FacingDragTranslator::onButton( FacingDragButton button, Bool down, const GameMessage *msg )
{
	const ICoord2D &pos = msg->getArgument( 0 )->pixel;
	const Bool canOrder = TheInGameUI->getSelectCount() > 0;
	const FacingDragAction action = facingDragOnButton( m_state, button, down, pos, canOrder );
	// Later translators can still receive the first press from this frame.
	blockOtherTranslators( doesFacingDragOwnButtons( m_state ) );
	switch( action )
	{
		case FACING_DRAG_BEGIN:
		{
			// Later translators must not receive the other press after both releases are swallowed.
			const GameMessage::Type otherDown = button == FACING_DRAG_LEFT ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN;
			const GameMessage::Type otherDouble = button == FACING_DRAG_LEFT ? GameMessage::MSG_RAW_MOUSE_RIGHT_DOUBLE_CLICK : GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK;
			for (const GameMessage *prior = msg->prev(); prior; prior = prior->prev())
			{
				if (prior->getType() == otherDown || prior->getType() == otherDouble)
				{
					deleteInstance(const_cast<GameMessage *>(prior));
					break;
				}
			}
			m_anchorValid = TheTacticalView->screenToTerrain( &pos, &m_anchorWorld );
			beginPreview();
			// the first button's up will come; its click must not select or order
			m_swallowClick[FACING_DRAG_LEFT] = FALSE;
			m_swallowClick[FACING_DRAG_RIGHT] = FALSE;
			return DESTROY_MESSAGE;
		}
		case FACING_DRAG_FINISH:
			m_swallowClick[button] = TRUE;
			giveOrder( pos );
			endPreview();
			return DESTROY_MESSAGE;
		case FACING_DRAG_SWALLOW:
			if( !down )
			{
				m_swallowClick[button] = TRUE;
			}
			return DESTROY_MESSAGE;
		case FACING_DRAG_PASS:
		default:
			return KEEP_MESSAGE;
	}
}

//-------------------------------------------------------------------------------------------------
void FacingDragTranslator::blockOtherTranslators( Bool block )
{
	if( block == m_blocking )
	{
		return;
	}
	m_blocking = block;
	if( TheSelectionTranslator )
	{
		TheSelectionTranslator->setDragSelectBlocked( block );
	}
	if( TheLookAtTranslator )
	{
		TheLookAtTranslator->setRightMouseScrollBlocked( block );
	}
}

//-------------------------------------------------------------------------------------------------
void FacingDragTranslator::giveOrder( const ICoord2D &releasePos )
{
	if( !m_anchorValid )
	{
		return;
	}

	Real angle;
	Int rows;
	if( computeFacing( releasePos, (Int)m_ghostObjects.size(), &angle, &rows ) )
	{
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_MOVETO_FACING );
		msg->appendLocationArgument( m_anchorWorld );
		msg->appendRealArgument( angle );
		msg->appendIntegerArgument( rows );
		msg->appendIntegerArgument( (Int)m_style );
	}
	else
	{
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_MOVETO );
		msg->appendLocationArgument( m_anchorWorld );
	}
	pickAndPlayUnitVoiceResponse( TheInGameUI->getAllSelectedDrawables(), GameMessage::MSG_DO_MOVETO );
}

//-------------------------------------------------------------------------------------------------
// Long enough to be a facing drag: the angle to the cursor, and the rows for its distance on the ground.
Bool FacingDragTranslator::computeFacing( const ICoord2D &cursor, Int moverCount, Real *angle, Int *rows ) const
{
	if( !m_anchorValid || !isFacingDragLongEnough( m_state.anchor, cursor, FACING_DRAG_MIN_PIXELS ) )
	{
		return FALSE;
	}
	Coord3D cursorWorld;
	if( !TheTacticalView->screenToTerrain( &cursor, &cursorWorld ) )
	{
		return FALSE;
	}
	const Real dx = cursorWorld.x - m_anchorWorld.x;
	const Real dy = cursorWorld.y - m_anchorWorld.y;
	// Ctrl held: absolute 45 degree steps, like building placement (the ghosts show it while dragging)
	*angle = snapFacingAngle( facingAngle( m_anchorWorld, cursorWorld ), TheInGameUI->isInForceAttackMode() );
	*rows = lineFormationRows( sqrtf( dx * dx + dy * dy ), moverCount );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
// The movers at the chord; their ghosts come once the drag has a facing (the anchor ring meanwhile).
void FacingDragTranslator::beginPreview()
{
	endPreview();
	TheInGameUI->setFacingDragPreview( m_anchorValid, &m_anchorWorld, 0.0f, FALSE );

	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		const Object *obj = (*it)->getObject();
		if( isLineFormationMover( obj ) )
		{
			m_ghostObjects.push_back( (UnsignedInt)obj->getID() );
			m_ghosts.push_back( 0 );
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Hiding a truck ghost can start wheel emitters that require an object.
static Drawable *createGhost( const Object *obj )
{
	UnsignedInt drawableStatus = DRAWABLE_STATUS_NO_STATE_PARTICLES | DRAWABLE_STATUS_NO_SAVE;
	drawableStatus |= TheGlobalData->m_objectPlacementShadows ? DRAWABLE_STATUS_SHADOWS : 0;
	Drawable *ghost = TheThingFactory->newDrawable( obj->getTemplate(), drawableStatus );
	if( !ghost )
	{
		return nullptr;
	}
	const Player *owner = obj->getControllingPlayer();
	if( owner )
	{
		ghost->setIndicatorColor( TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT ? owner->getPlayerNightColor() : owner->getPlayerColor() );
	}
	ghost->setDrawableOpacity( TheGlobalData->m_objectPlacementOpacity );
	ghost->enableAmbientSound( FALSE );
	return ghost;
}

//-------------------------------------------------------------------------------------------------
void FacingDragTranslator::updatePreview( const ICoord2D &cursor )
{
	Real angle = 0.0f;
	Int rows = 1;
	const Bool hasFacing = computeFacing( cursor, (Int)m_ghostObjects.size(), &angle, &rows );
	TheInGameUI->setFacingDragPreview( m_anchorValid, &m_anchorWorld, angle, hasFacing );
	if( !hasFacing )
	{
		destroyGhosts();
		return;
	}

	// The units as the logic will see them (LineFormation.h); a unit gone meanwhile loses its ghost.
	std::vector<FormationMember> members;
	std::vector<Drawable *> ghosts;
	std::vector<Real> heights;	// above the ground: flying units' ghosts fly at their height
	for( size_t i = 0; i < m_ghostObjects.size(); ++i )
	{
		Drawable *ghost = m_ghosts[i] ? TheGameClient->findDrawableByID( (DrawableID)m_ghosts[i] ) : nullptr;
		const Object *obj = TheGameLogic->findObjectByID( (ObjectID)m_ghostObjects[i] );
		if( !isLineFormationMover( obj ) )
		{
			if( ghost )
			{
				TheGameClient->destroyDrawable( ghost );
			}
			m_ghosts[i] = 0;
			continue;
		}
		if( !ghost )
		{
			ghost = createGhost( obj );
			m_ghosts[i] = ghost ? (UnsignedInt)ghost->getID() : 0;
		}
		members.push_back( makeFormationMember( obj ) );
		ghosts.push_back( ghost );
		const Coord3D *objPos = obj->getPosition();
		const Real height = objPos->z - TheTerrainLogic->getGroundHeight( objPos->x, objPos->y );
		heights.push_back( height > 0.0f ? height : 0.0f );
	}
	if( members.empty() )
	{
		return;
	}

	std::vector<Coord3D> slots( members.size() );
	computeLineFormation( m_anchorWorld, angle, rows, &members[0], (Int)members.size(), &slots[0], m_style );
	for( size_t i = 0; i < ghosts.size(); ++i )
	{
		if( !ghosts[i] )
		{
			continue;
		}
		Coord3D pos = slots[i];
		pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y ) + heights[i];
		ghosts[i]->setPosition( &pos );
		ghosts[i]->setOrientation( angle );
	}
}

//-------------------------------------------------------------------------------------------------
void FacingDragTranslator::destroyGhosts()
{
	for( size_t i = 0; i < m_ghosts.size(); ++i )
	{
		Drawable *ghost = m_ghosts[i] ? TheGameClient->findDrawableByID( (DrawableID)m_ghosts[i] ) : nullptr;
		if( ghost )
		{
			TheGameClient->destroyDrawable( ghost );
		}
		m_ghosts[i] = 0;
	}
}

//-------------------------------------------------------------------------------------------------
void FacingDragTranslator::endPreview()
{
	TheInGameUI->setFacingDragPreview( FALSE, nullptr, 0.0f, FALSE );
	destroyGhosts();
	m_ghosts.clear();
	m_ghostObjects.clear();
}
