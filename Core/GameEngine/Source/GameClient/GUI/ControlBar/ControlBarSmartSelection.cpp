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

// TheSuperHackers @feature Smart selection (Options.ini: SmartSelection). A row of half size
// cameos above the command bar, one per selected unit type, each with a count. Left click and
// Tab focus a type: the whole group stays selected, but the bar shows that type's command set
// instead of the group's common subset. Ctrl click drops the type from the selection.
//
// The row is built in code rather than from ControlBar.wnd, which ships in the game data.
// The container is a top level window because a child sitting outside its parent's rect is
// drawn but never hit tested. It is SEE_THRU so the gaps between cameos still reach the world.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameClient/WinInstanceData.h"

static const Int SMART_SELECTION_GAP = 2;

//-------------------------------------------------------------------------------------------------
/** The container owns the cameos, so their clicks land here. */
//-------------------------------------------------------------------------------------------------
static WindowMsgHandledType SmartSelectionBarSystem( GameWindow *window, UnsignedInt msg,
																										 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{
		case GBM_SELECTED:
		{
			if( TheControlBar )
			{
				TheControlBar->processSmartSelectionClick( (GameWindow *)mData1, (GadgetGameMessage)msg );
			}
			break;
		}

		default:
			return MSG_IGNORED;
	}

	return MSG_HANDLED;
}

//-------------------------------------------------------------------------------------------------
/** A selected object that gets a cameo. Mob members ride along with their nexus and stay out
	* of the row, like they stay out of the command bar. */
//-------------------------------------------------------------------------------------------------
static Object *getSmartSelectionObject( Drawable *draw )
{
	Object *obj = draw ? draw->getObject() : nullptr;
	if( obj == nullptr )
	{
		return nullptr;
	}
	if( obj->isKindOf( KINDOF_IGNORED_IN_GUI ) || obj->getStatusBits().test( OBJECT_STATUS_SOLD ) )
	{
		return nullptr;
	}
	return obj;
}

//-------------------------------------------------------------------------------------------------
/** Resolve a row member, or null once it is gone. */
//-------------------------------------------------------------------------------------------------
static Object *getLiveSmartSelectionObject( ObjectID id )
{
	Object *obj = TheGameLogic->findObjectByID( id );
	if( obj == nullptr || obj->isEffectivelyDead() || obj->getDrawable() == nullptr )
	{
		return nullptr;
	}
	return obj;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::initSmartSelectionBar( const ICoord2D &commandButtonSize )
{
	m_smartSelectionButtonSize.x = commandButtonSize.x / 2;
	m_smartSelectionButtonSize.y = commandButtonSize.y / 2;
	if( m_smartSelectionButtonSize.x <= 0 || m_smartSelectionButtonSize.y <= 0 )
	{
		return;
	}

	const Int stride = m_smartSelectionButtonSize.x + SMART_SELECTION_GAP;

	// the bar's own ABOVE bit decides which hit test pass finds it, so the row must match it
	UnsignedInt parentStatus = WIN_STATUS_ENABLED | WIN_STATUS_SEE_THRU | WIN_STATUS_HIDDEN;
	if( m_contextParent[ CP_MASTER ] && BitIsSet( m_contextParent[ CP_MASTER ]->winGetStatus(), WIN_STATUS_ABOVE ) )
	{
		parentStatus |= WIN_STATUS_ABOVE;
	}

	m_smartSelectionParent = TheWindowManager->winCreate( nullptr, parentStatus, 0, 0,
		MAX_SMART_SELECTION_BUTTONS * stride - SMART_SELECTION_GAP, m_smartSelectionButtonSize.y,
		SmartSelectionBarSystem );
	if( m_smartSelectionParent == nullptr )
	{
		return;
	}

	m_smartSelectionMoneyWindow = TheWindowManager->winGetWindowFromId( nullptr,
		TheNameKeyGenerator->nameToKey( "ControlBar.wnd:MoneyDisplay" ) );

	Int pointSize = m_smartSelectionButtonSize.y / 3;
	if( pointSize < 8 )
	{
		pointSize = 8;
	}
	if( pointSize > 10 )
	{
		pointSize = 10;
	}
	if( TheGlobalLanguageData )
	{
		pointSize = TheGlobalLanguageData->adjustFontSize( pointSize );
	}
	GameFont *font = TheFontLibrary ? TheFontLibrary->getFont( AsciiString( "Arial" ), pointSize, TRUE ) : nullptr;

	const Color textColor = GameMakeColor( 255, 255, 255, 255 );
	const Color dropColor = GameMakeColor( 0, 0, 0, 255 );

	AsciiString windowName;
	for( Int i = 0; i < MAX_SMART_SELECTION_BUTTONS; i++ )
	{
		WinInstanceData instData;
		instData.init();
		instData.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;

		GameWindow *button = TheWindowManager->gogoGadgetPushButton( m_smartSelectionParent,
			WIN_STATUS_ENABLED | WIN_STATUS_IMAGE | WIN_STATUS_USE_OVERLAY_STATES |
			WIN_STATUS_COUNT_BADGE | WIN_STATUS_HIDDEN,
			i * stride, 0, m_smartSelectionButtonSize.x, m_smartSelectionButtonSize.y,
			&instData, font, FALSE );
		if( button == nullptr )
		{
			continue;
		}

		windowName.format( "SmartSelection:Button%02d", i + 1 );
		button->winSetWindowId( TheNameKeyGenerator->nameToKey( windowName ) );
		if( font )
		{
			button->winSetFont( font );
		}
		button->winSetEnabledTextColors( textColor, dropColor );
		button->winSetHiliteTextColors( textColor, dropColor );
		button->winSetDisabledTextColors( textColor, dropColor );
		GadgetButtonEnableCheckLike( button, TRUE, FALSE );
		GadgetButtonSetAltSound( button, "GUICommandBarClick" );

		m_smartSelectionButtons[ i ] = button;
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::destroySmartSelectionBar()
{
	if( m_smartSelectionParent && TheWindowManager )
	{
		TheWindowManager->winDestroy( m_smartSelectionParent );
	}
	m_smartSelectionParent = nullptr;
	for( Int i = 0; i < MAX_SMART_SELECTION_BUTTONS; i++ )
	{
		m_smartSelectionButtons[ i ] = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::resetSmartSelection()
{
	m_smartSelectionGroups.clear();
	m_smartSelectionLiveIDs.clear();
	m_smartSelectionActive = -1;
	m_smartSelectionNarrowed = FALSE;
	if( m_smartSelectionParent )
	{
		m_smartSelectionParent->winHide( TRUE );
	}
}

//-------------------------------------------------------------------------------------------------
/** The type whose command set the bar should show, or null for the group's common set. */
//-------------------------------------------------------------------------------------------------
const ThingTemplate *ControlBar::getSmartSelectionFocusTemplate() const
{
	if( m_smartSelectionActive < 0 || (size_t)m_smartSelectionActive >= m_smartSelectionGroups.size() )
	{
		return nullptr;
	}
	return m_smartSelectionGroups[ m_smartSelectionActive ].thingTemplate;
}

//-------------------------------------------------------------------------------------------------
/** Whether a command set carries the button, by name because sets hold overridable copies. */
//-------------------------------------------------------------------------------------------------
static Bool commandSetHasButton( const CommandSet *commandSet, const CommandButton *command )
{
	if( commandSet == nullptr )
	{
		return FALSE;
	}
	for( Int i = 0; i < MAX_COMMANDS_PER_SET; i++ )
	{
		const CommandButton *button = commandSet->getCommandButton( i );
		if( button && button->getName() == command->getName() )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** The logic side sends a command to every unit in the player's group that can do it, so a
	* command off the focused type's card would leak to any other type with a matching one. For
	* a command that is not common to the whole group, hand the logic side just the focused type
	* until the command is done. The client selection is untouched throughout. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionBeginCommand( const CommandButton *command )
{
	if( m_smartSelectionNarrowed || command == nullptr || m_currContext != CB_CONTEXT_MULTI_SELECT )
	{
		return;
	}
	const ThingTemplate *focus = getSmartSelectionFocusTemplate();
	if( focus == nullptr || TheInGameUI == nullptr || TheMessageStream == nullptr )
	{
		return;
	}

	std::vector<Object *> focused;
	const std::vector<ObjectID> &ids = m_smartSelectionGroups[ m_smartSelectionActive ].objectIDs;
	for( size_t i = 0; i < ids.size(); i++ )
	{
		Object *obj = getLiveSmartSelectionObject( ids[ i ] );
		if( obj && obj->getDrawable()->isSelected() )
		{
			focused.push_back( obj );
		}
	}
	if( focused.empty() )
	{
		return;
	}

	// a command the focused card does not carry came from elsewhere, the shortcut bar say
	if( !commandSetHasButton( findCommandSet( focused[ 0 ]->getCommandSetString() ), command ) )
	{
		return;
	}

	// a command every selected type carries is the group's own and still goes to everyone
	Bool common = TRUE;
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); common && it != selected->end(); ++it )
	{
		Object *obj = getSmartSelectionObject( *it );
		if( obj == nullptr )
		{
			continue;
		}
		common = commandSetHasButton( findCommandSet( obj->getCommandSetString() ), command );
	}
	if( common )
	{
		return;
	}

	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
	msg->appendBooleanArgument( TRUE );
	for( size_t i = 0; i < focused.size(); i++ )
	{
		msg->appendObjectIDArgument( focused[ i ]->getID() );
	}
	m_smartSelectionNarrowed = TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Give the logic side the whole selection back, once no command is still waiting for a
	* target. That later clear of the pending command ends up here as well. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionEndCommand()
{
	if( !m_smartSelectionNarrowed || TheInGameUI == nullptr || TheMessageStream == nullptr )
	{
		return;
	}
	if( TheInGameUI->getGUICommand() != nullptr )
	{
		return;
	}

	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
	msg->appendBooleanArgument( TRUE );
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = ( *it )->getObject();
		if( obj )
		{
			msg->appendObjectIDArgument( obj->getID() );
		}
	}
	m_smartSelectionNarrowed = FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Runs whenever the UI is marked dirty, which is far more often than the selection changes,
	* so the selection is snapshotted and compared before anything is rebuilt. */
//-------------------------------------------------------------------------------------------------
void ControlBar::populateSmartSelection()
{
	if( m_smartSelectionParent == nullptr )
	{
		return;
	}
	if( TheGlobalData == nullptr || !TheGlobalData->m_smartSelection || TheInGameUI == nullptr )
	{
		resetSmartSelection();
		return;
	}

	std::vector<ObjectID> liveIDs;
	const DrawableList *selected = TheInGameUI->getAllSelectedLocalDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = getSmartSelectionObject( *it );
		if( obj )
		{
			liveIDs.push_back( obj->getID() );
		}
	}
	std::sort( liveIDs.begin(), liveIDs.end() );

	// the dirty flag fires for production, rank and much else, so an unchanged selection must
	// not repaint the row -- the button labels alone would re-lay out a display string each time
	if( liveIDs == m_smartSelectionLiveIDs )
	{
		return;
	}
	m_smartSelectionLiveIDs = liveIDs;

	if( liveIDs.empty() )
	{
		resetSmartSelection();
		return;
	}

	// The focused type survives the rebuild if it is still in the selection. It cannot survive
	// into a selection the bar drives from one drawable, whose own command set is shown and
	// where the focus would silently filter the next multi selection.
	const ThingTemplate *focus = nullptr;
	if( TheInGameUI->getSelectCount() > 1 )
	{
		focus = getSmartSelectionFocusTemplate();
	}
	m_smartSelectionActive = -1;

	m_smartSelectionGroups.clear();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = getSmartSelectionObject( *it );
		if( obj == nullptr )
		{
			continue;
		}

		const ThingTemplate *thingTemplate = obj->getTemplate();
		size_t g = 0;
		for( ; g < m_smartSelectionGroups.size(); g++ )
		{
			if( m_smartSelectionGroups[ g ].thingTemplate->isEquivalentTo( thingTemplate ) )
			{
				break;
			}
		}
		if( g == m_smartSelectionGroups.size() )
		{
			if( g >= MAX_SMART_SELECTION_BUTTONS )
			{
				continue;
			}
			SmartSelectionGroup group;
			group.thingTemplate = thingTemplate;
			m_smartSelectionGroups.push_back( group );
			// the same template, not merely an equivalent one, or an upgraded variant in the
			// same selection could take the focus away from the type the player picked
			if( thingTemplate == focus )
			{
				m_smartSelectionActive = (Int)g;
			}
		}
		m_smartSelectionGroups[ g ].objectIDs.push_back( obj->getID() );
	}

	refreshSmartSelectionButtons();
}

//-------------------------------------------------------------------------------------------------
/** Every frame: follow the command bar, which slides and hides on its own schedule. Members
	* that die leave the selection, which rebuilds the row through the dirty flag. */
//-------------------------------------------------------------------------------------------------
void ControlBar::updateSmartSelection()
{
	if( m_smartSelectionParent == nullptr )
	{
		return;
	}

	GameWindow *master = m_contextParent[ CP_MASTER ];
	GameWindow *commandWindow = m_contextParent[ CP_COMMAND ] ? m_contextParent[ CP_COMMAND ] : master;
	if( master == nullptr || master->winIsHidden() || m_smartSelectionGroups.empty() )
	{
		m_smartSelectionParent->winHide( TRUE );
		return;
	}

	// the bar's parent window starts well above its visible frame, so anchor to the command
	// grid, whose top sits at the frame
	ICoord2D commandPos;
	commandWindow->winGetScreenPosition( &commandPos.x, &commandPos.y );
	Int rowY = commandPos.y - m_smartSelectionButtonSize.y - SMART_SELECTION_GAP;

	// the money display rises out of the frame, so a row long enough to reach its housing
	// lifts above it instead of running into it
	if( m_smartSelectionMoneyWindow && !m_smartSelectionMoneyWindow->winIsHidden() )
	{
		ICoord2D moneyPos;
		m_smartSelectionMoneyWindow->winGetScreenPosition( &moneyPos.x, &moneyPos.y );
		// measured from the groups rather than read back from the container, whose size is
		// only written when the buttons are repainted
		const Int rowWidth = (Int)m_smartSelectionGroups.size() *
			( m_smartSelectionButtonSize.x + SMART_SELECTION_GAP ) - SMART_SELECTION_GAP;
		// the housing slopes out about a cameo's width left of the money text
		const Int housingX = moneyPos.x - m_smartSelectionButtonSize.x;
		const Int liftedY = moneyPos.y - m_smartSelectionButtonSize.y - SMART_SELECTION_GAP;
		if( commandPos.x + rowWidth > housingX && liftedY < rowY )
		{
			rowY = liftedY;
		}
	}

	m_smartSelectionParent->winSetPosition( commandPos.x, rowY );
	m_smartSelectionParent->winHide( FALSE );
}

//-------------------------------------------------------------------------------------------------
/** Put the groups on the cameos, with the focused one pushed in. */
//-------------------------------------------------------------------------------------------------
void ControlBar::refreshSmartSelectionButtons()
{
	const size_t groupCount = m_smartSelectionGroups.size();

	for( Int i = 0; i < MAX_SMART_SELECTION_BUTTONS; i++ )
	{
		GameWindow *button = m_smartSelectionButtons[ i ];
		if( button == nullptr )
		{
			continue;
		}
		if( (size_t)i >= groupCount )
		{
			button->winHide( TRUE );
			continue;
		}

		const SmartSelectionGroup &group = m_smartSelectionGroups[ i ];
		const Image *image = group.thingTemplate->getButtonImage();
		if( image == nullptr )
		{
			image = group.thingTemplate->getSelectedPortraitImage();
		}
		GadgetButtonSetEnabledImage( button, image );

		UnicodeString count;
		count.format( L"%d", (Int)group.objectIDs.size() );
		GadgetButtonSetText( button, count );
		button->winSetTooltip( group.thingTemplate->getDisplayName() );
		GadgetCheckLikeButtonSetVisualCheck( button, i == m_smartSelectionActive );
		button->winHide( FALSE );
	}

	const Int stride = m_smartSelectionButtonSize.x + SMART_SELECTION_GAP;
	const Int width = (Int)groupCount * stride - SMART_SELECTION_GAP;
	m_smartSelectionParent->winSetSize( width > 0 ? width : 1, m_smartSelectionButtonSize.y );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::processSmartSelectionClick( GameWindow *button, GadgetGameMessage gadgetMessage )
{
	Int groupIndex = -1;
	for( Int i = 0; i < MAX_SMART_SELECTION_BUTTONS; i++ )
	{
		if( m_smartSelectionButtons[ i ] == button )
		{
			groupIndex = i;
			break;
		}
	}
	if( groupIndex < 0 || (size_t)groupIndex >= m_smartSelectionGroups.size() )
	{
		return;
	}

	// Ctrl, not Shift, because Shift+Tab cycles the row: a click landing while that Shift is
	// still held must not delete the type. Right click is left alone so it keeps deselecting.
	if( TheKeyboard && TheKeyboard->isCtrl() )
	{
		smartSelectionRemove( groupIndex );
	}
	else if( groupIndex == m_smartSelectionActive )
	{
		smartSelectionFocus( -1 );
	}
	else
	{
		smartSelectionFocus( groupIndex );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionCycle( Int direction )
{
	const Int groupCount = (Int)m_smartSelectionGroups.size();
	if( groupCount < 2 || direction == 0 )
	{
		return;
	}

	Int next;
	if( m_smartSelectionActive < 0 )
	{
		next = direction > 0 ? 0 : groupCount - 1;
	}
	else
	{
		next = ( m_smartSelectionActive + direction + groupCount ) % groupCount;
	}
	smartSelectionFocus( next );
}

//-------------------------------------------------------------------------------------------------
/** Show one type's command set, or the common set again for -1. Nothing about the selection
	* changes; the dirty flag makes the context repopulate around the new focus. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionFocus( Int groupIndex )
{
	if( groupIndex >= (Int)m_smartSelectionGroups.size() )
	{
		return;
	}
	m_smartSelectionActive = groupIndex < 0 ? -1 : groupIndex;
	refreshSmartSelectionButtons();
	markUIDirty();
}

//-------------------------------------------------------------------------------------------------
/** Drop one type from the selection. Client side deselect plus one remove message, the same
	* shape as a shift click on a selected unit. The selection change rebuilds the row. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionRemove( Int groupIndex )
{
	if( groupIndex < 0 || (size_t)groupIndex >= m_smartSelectionGroups.size() )
	{
		return;
	}

	GameMessage *msg = nullptr;
	const std::vector<ObjectID> &ids = m_smartSelectionGroups[ groupIndex ].objectIDs;
	for( size_t i = 0; i < ids.size(); i++ )
	{
		Object *obj = getLiveSmartSelectionObject( ids[ i ] );
		if( obj == nullptr )
		{
			continue;
		}
		// the logic side keeps a member that has since left the client selection, boarding a
		// transport for one, so it is named here too or it would go on taking orders
		if( msg == nullptr )
		{
			msg = TheMessageStream->appendMessage( GameMessage::MSG_REMOVE_FROM_SELECTED_GROUP );
		}
		msg->appendObjectIDArgument( obj->getID() );
		if( obj->getDrawable()->isSelected() )
		{
			TheInGameUI->deselectDrawable( obj->getDrawable() );
		}
	}

	if( groupIndex == m_smartSelectionActive )
	{
		m_smartSelectionActive = -1;
	}
}
