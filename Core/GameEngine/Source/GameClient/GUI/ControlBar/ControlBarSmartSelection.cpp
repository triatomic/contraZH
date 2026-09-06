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
// The container is a top level window because the hit test only descends into a top level
// window that contains the point, so a child sitting above its parent's rect is never found.
// It is SEE_THRU so the gaps between cameos still reach the world.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
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
	if( msg != GBM_SELECTED )
	{
		return MSG_IGNORED;
	}
	TheControlBar->processSmartSelectionClick( (GameWindow *)mData1 );
	return MSG_HANDLED;
}

//-------------------------------------------------------------------------------------------------
/** A selected object that gets a cameo. Mob members ride along with their nexus and stay out
	* of the row, like they stay out of the command bar. */
//-------------------------------------------------------------------------------------------------
static Object *getSmartSelectionObject( Drawable *draw )
{
	Object *obj = draw->getObject();
	if( obj == nullptr || !obj->isLocallyControlled() )
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
/** Hand the logic side a group made of the selection, or of one type in it for onlyType. */
//-------------------------------------------------------------------------------------------------
static void appendSelectionGroup( const ThingTemplate *onlyType )
{
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
	msg->appendBooleanArgument( TRUE );
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = ( *it )->getObject();
		if( obj == nullptr )
		{
			continue;
		}
		if( onlyType && !obj->getTemplate()->isEquivalentTo( onlyType ) )
		{
			continue;
		}
		msg->appendObjectIDArgument( obj->getID() );
	}
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

	Int pointSize = MIN( MAX( m_smartSelectionButtonSize.y / 3, 8 ), 10 );
	if( TheGlobalLanguageData )
	{
		pointSize = TheGlobalLanguageData->adjustFontSize( pointSize );
	}
	GameFont *font = TheFontLibrary->getFont( AsciiString( "Arial" ), pointSize, TRUE );

	const Color textColor = GameMakeColor( 255, 255, 255, 255 );
	const Color dropColor = GameMakeColor( 0, 0, 0, 255 );

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

		// the slot rides on the button, one based so an unset payload never reads as slot zero
		GadgetButtonSetData( button, (void *)(size_t)( i + 1 ) );
		button->winSetFont( font );
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
	if( m_smartSelectionParent )
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
	m_smartSelectionActive = -1;
	m_smartSelectionNarrowed = FALSE;
	if( m_smartSelectionParent && !m_smartSelectionParent->winIsHidden() )
	{
		m_smartSelectionParent->winHide( TRUE );
	}
}

//-------------------------------------------------------------------------------------------------
/** The type whose command set the bar should show, or null for the group's common set. */
//-------------------------------------------------------------------------------------------------
const ThingTemplate *ControlBar::getSmartSelectionFocusTemplate() const
{
	return m_smartSelectionActive < 0 ? nullptr : m_smartSelectionGroups[ m_smartSelectionActive ].thingTemplate;
}

//-------------------------------------------------------------------------------------------------
/** Whether the object takes part in the command bar while a type is focused. */
//-------------------------------------------------------------------------------------------------
Bool ControlBar::isSmartSelectionFocused( const Object *obj ) const
{
	const ThingTemplate *focus = getSmartSelectionFocusTemplate();
	return focus == nullptr || obj->getTemplate()->isEquivalentTo( focus );
}

//-------------------------------------------------------------------------------------------------
Int ControlBar::getSmartSelectionRowWidth() const
{
	return (Int)m_smartSelectionGroups.size() * ( m_smartSelectionButtonSize.x + SMART_SELECTION_GAP ) - SMART_SELECTION_GAP;
}

//-------------------------------------------------------------------------------------------------
/** Runs whenever the UI is marked dirty, which is far more often than the selection changes,
	* so the groups are rebuilt into a local and the row only repainted when they differ. */
//-------------------------------------------------------------------------------------------------
void ControlBar::populateSmartSelection()
{
	if( m_smartSelectionParent == nullptr )
	{
		return;
	}
	if( !TheGlobalData->m_smartSelection )
	{
		resetSmartSelection();
		return;
	}

	std::vector<SmartSelectionGroup> groups;
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = getSmartSelectionObject( *it );
		if( obj == nullptr )
		{
			continue;
		}

		const ThingTemplate *thingTemplate = obj->getTemplate();
		size_t g = 0;
		for( ; g < groups.size(); g++ )
		{
			if( groups[ g ].thingTemplate->isEquivalentTo( thingTemplate ) )
			{
				break;
			}
		}
		if( g == groups.size() )
		{
			if( g >= MAX_SMART_SELECTION_BUTTONS )
			{
				continue;
			}
			SmartSelectionGroup group;
			group.thingTemplate = thingTemplate;
			group.count = 0;
			groups.push_back( group );
		}
		groups[ g ].count++;
	}

	Bool same = groups.size() == m_smartSelectionGroups.size();
	for( size_t g = 0; same && g < groups.size(); g++ )
	{
		same = groups[ g ].thingTemplate == m_smartSelectionGroups[ g ].thingTemplate &&
					 groups[ g ].count == m_smartSelectionGroups[ g ].count;
	}
	if( same )
	{
		return;
	}

	// The focused type survives the rebuild if it is still in the selection, matched as the
	// same template so an upgraded variant in the selection cannot take it. It cannot survive
	// into a selection the bar drives from one drawable, whose own command set is shown and
	// where the focus would silently filter the next multi selection.
	const ThingTemplate *focus = TheInGameUI->getSelectCount() > 1 ? getSmartSelectionFocusTemplate() : nullptr;
	m_smartSelectionGroups.swap( groups );
	m_smartSelectionActive = -1;
	for( size_t g = 0; g < m_smartSelectionGroups.size(); g++ )
	{
		if( m_smartSelectionGroups[ g ].thingTemplate == focus )
		{
			m_smartSelectionActive = (Int)g;
		}
	}

	refreshSmartSelectionButtons();
}

//-------------------------------------------------------------------------------------------------
/** Every frame: follow the command bar, which slides in on show and hides on its own schedule. */
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
		if( !m_smartSelectionParent->winIsHidden() )
		{
			m_smartSelectionParent->winHide( TRUE );
		}
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
		// the housing slopes out about a cameo's width left of the money text
		const Int housingMargin = m_smartSelectionButtonSize.x;
		const Int liftedY = moneyPos.y - m_smartSelectionButtonSize.y - SMART_SELECTION_GAP;
		if( commandPos.x + getSmartSelectionRowWidth() > moneyPos.x - housingMargin && liftedY < rowY )
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
		count.format( L"%d", group.count );
		GadgetButtonSetText( button, count );
		button->winSetTooltip( group.thingTemplate->getDisplayName() );
		GadgetCheckLikeButtonSetVisualCheck( button, i == m_smartSelectionActive );
		button->winHide( FALSE );
	}

	m_smartSelectionParent->winSetSize( MAX( getSmartSelectionRowWidth(), 1 ), m_smartSelectionButtonSize.y );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::processSmartSelectionClick( GameWindow *button )
{
	const Int groupIndex = (Int)(size_t)GadgetButtonGetData( button ) - 1;
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
	else
	{
		smartSelectionFocus( groupIndex == m_smartSelectionActive ? -1 : groupIndex );
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
	m_smartSelectionActive = groupIndex;
	refreshSmartSelectionButtons();
	markUIDirty();
}

//-------------------------------------------------------------------------------------------------
/** Drop one type from the selection. Client side deselect plus one remove message, the same
	* shape as a shift click on a selected unit. The selection change rebuilds the row. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionRemove( Int groupIndex )
{
	const ThingTemplate *thingTemplate = m_smartSelectionGroups[ groupIndex ].thingTemplate;

	// gathered first, since deselecting walks the very list being read
	std::vector<Drawable *> members;
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = getSmartSelectionObject( *it );
		if( obj && obj->getTemplate()->isEquivalentTo( thingTemplate ) )
		{
			members.push_back( *it );
		}
	}
	if( members.empty() )
	{
		return;
	}

	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_REMOVE_FROM_SELECTED_GROUP );
	for( size_t i = 0; i < members.size(); i++ )
	{
		msg->appendObjectIDArgument( members[ i ]->getObject()->getID() );
		TheInGameUI->deselectDrawable( members[ i ] );
	}

	if( groupIndex == m_smartSelectionActive )
	{
		m_smartSelectionActive = -1;
	}
}

//-------------------------------------------------------------------------------------------------
/** The logic side sends a command to every unit in the player's group that can do it, so a
	* command off the focused type's card would leak to any other type with a matching one. For
	* a command that is not common to the whole group, hand the logic side just the focused type
	* until the command is done. The client selection is untouched throughout. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionBeginCommand( const CommandButton *command )
{
	if( m_smartSelectionNarrowed || m_currContext != CB_CONTEXT_MULTI_SELECT )
	{
		return;
	}
	const ThingTemplate *focus = getSmartSelectionFocusTemplate();
	if( focus == nullptr )
	{
		return;
	}

	// with a type focused the populated commands are its card, so anything else, a shortcut
	// bar power say, came from elsewhere
	Bool onCard = FALSE;
	for( Int i = 0; !onCard && i < MAX_COMMANDS_PER_SET; i++ )
	{
		onCard = m_commonCommands[ i ] == command;
	}
	if( !onCard )
	{
		return;
	}

	// a command every other selected type carries too is the group's own and still goes to everyone
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = getSmartSelectionObject( *it );
		if( obj == nullptr || obj->getTemplate()->isEquivalentTo( focus ) )
		{
			continue;
		}
		if( !commandSetHasButton( findCommandSet( obj->getCommandSetString() ), command ) )
		{
			appendSelectionGroup( focus );
			m_smartSelectionNarrowed = TRUE;
			return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Give the logic side the whole selection back, once no command is still waiting for a
	* target. That later clear of the pending command ends up here as well. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionEndCommand()
{
	if( !m_smartSelectionNarrowed || TheInGameUI->getGUICommand() != nullptr )
	{
		return;
	}
	appendSelectionGroup( nullptr );
	m_smartSelectionNarrowed = FALSE;
}
