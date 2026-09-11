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
// cameos above the command bar. A mixed selection gets one cameo per type with a count; a
// selection of one type gets one cameo per object. A cameo for a single object shows its health
// bar. Left click and Tab focus a cameo: the whole group stays selected, but the bar shows the
// command set of that cameo's type, or of its one object, instead of the group's common subset.
// Right click drops the cameo's units from the selection, double click (or Ctrl+Shift click
// with SmartSelectionUseMouse = No) keeps only them.
//
// The row is built in code rather than from ControlBar.wnd, which ships in the game data.
// The container is a top level window because the hit test only descends into a top level
// window that contains the point, so a child sitting above its parent's rect is never found.
// It is SEE_THRU so the gaps between cameos still reach the world.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
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
	if( msg != GBM_SELECTED && msg != GBM_SELECTED_RIGHT )
	{
		return MSG_IGNORED;
	}
	TheControlBar->processSmartSelectionClick( (GameWindow *)mData1, msg == GBM_SELECTED_RIGHT );
	return MSG_HANDLED;
}

//-------------------------------------------------------------------------------------------------
/** A check like button ignores the right button up, which bubbles here and would otherwise
	* reach the world as a deselect. Swallow it over a cameo only: in a gap the right button must
	* still pass, or a camera scroll started there never ends. */
//-------------------------------------------------------------------------------------------------
static WindowMsgHandledType SmartSelectionBarInput( GameWindow *window, UnsignedInt msg,
																										WindowMsgData mData1, WindowMsgData mData2 )
{
	if( msg != GWM_RIGHT_DOWN && msg != GWM_RIGHT_UP )
	{
		return MSG_IGNORED;
	}
	GameWindow *child = window->winPointInChild( LOLONGTOSHORT( mData1 ), HILONGTOSHORT( mData1 ) );
	return ( child && child != window ) ? MSG_HANDLED : MSG_IGNORED;
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
	m_smartSelectionParent->winSetInputFunc( SmartSelectionBarInput );

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
		// the button input reads the right click flag off the instance data, not the window
		instData.m_status = WIN_STATUS_RIGHT_CLICK;

		GameWindow *button = TheWindowManager->gogoGadgetPushButton( m_smartSelectionParent,
			WIN_STATUS_ENABLED | WIN_STATUS_IMAGE | WIN_STATUS_USE_OVERLAY_STATES |
			WIN_STATUS_COUNT_BADGE | WIN_STATUS_RIGHT_CLICK | WIN_STATUS_HIDDEN,
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
/** The one object the bar should show, when the focused cameo stands for a single object. */
//-------------------------------------------------------------------------------------------------
ObjectID ControlBar::getSmartSelectionFocusObject() const
{
	return m_smartSelectionActive < 0 ? INVALID_ID : m_smartSelectionGroups[ m_smartSelectionActive ].objectID;
}

//-------------------------------------------------------------------------------------------------
/** The drawable of the one focused object, which drives the bar's context in place of the
	* multi select context, or null. */
//-------------------------------------------------------------------------------------------------
Drawable *ControlBar::getSmartSelectionFocusDrawable() const
{
	const ObjectID focusObject = getSmartSelectionFocusObject();
	if( focusObject == INVALID_ID )
	{
		return nullptr;
	}
	Object *obj = TheGameLogic->findObjectByID( focusObject );
	return obj ? obj->getDrawable() : nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Whether the object takes part in the command bar while a cameo is focused. */
//-------------------------------------------------------------------------------------------------
Bool ControlBar::isSmartSelectionFocused( const Object *obj ) const
{
	const ThingTemplate *focus = getSmartSelectionFocusTemplate();
	return focus == nullptr || obj->getTemplate() == focus;
}

//-------------------------------------------------------------------------------------------------
/** Whether the cameo is pushed in: the focused one, or every one of the focused type. */
//-------------------------------------------------------------------------------------------------
Bool ControlBar::isSmartSelectionGroupFocused( Int groupIndex ) const
{
	if( m_smartSelectionActive < 0 )
	{
		return FALSE;
	}
	const SmartSelectionGroup &focus = m_smartSelectionGroups[ m_smartSelectionActive ];
	return focus.objectID != INVALID_ID ? groupIndex == m_smartSelectionActive : m_smartSelectionGroups[ groupIndex ].thingTemplate == focus.thingTemplate;
}

//-------------------------------------------------------------------------------------------------
Int ControlBar::getSmartSelectionRowWidth() const
{
	return (Int)m_smartSelectionGroups.size() * ( m_smartSelectionButtonSize.x + SMART_SELECTION_GAP ) - SMART_SELECTION_GAP;
}

//-------------------------------------------------------------------------------------------------
/** Runs whenever the UI is marked dirty, which is far more often than the selection changes,
	* so the entries are rebuilt into a local and the row only repainted when they differ. */
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

	// one cameo per type, then a selection of a single type spreads into one cameo per object
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
			if( groups[ g ].thingTemplate == thingTemplate )
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
			group.objectID = obj->getID();
			groups.push_back( group );
		}
		groups[ g ].count++;
		if( groups[ g ].count > 1 )
		{
			groups[ g ].objectID = INVALID_ID;
		}
	}
	if( groups.size() == 1 && groups[ 0 ].count > 1 )
	{
		SmartSelectionGroup group = groups[ 0 ];
		group.count = 1;
		groups.clear();
		for( DrawableListCIt it = selected->begin(); it != selected->end() && groups.size() < MAX_SMART_SELECTION_BUTTONS; ++it )
		{
			Object *obj = getSmartSelectionObject( *it );
			if( obj )
			{
				group.objectID = obj->getID();
				groups.push_back( group );
			}
		}
	}

	Bool same = groups.size() == m_smartSelectionGroups.size();
	for( size_t g = 0; same && g < groups.size(); g++ )
	{
		same = groups[ g ].thingTemplate == m_smartSelectionGroups[ g ].thingTemplate &&
					 groups[ g ].count == m_smartSelectionGroups[ g ].count &&
					 groups[ g ].objectID == m_smartSelectionGroups[ g ].objectID;
	}
	if( same )
	{
		return;
	}

	// The focus survives the rebuild if its object, or else its type, is still in the
	// selection. It cannot survive into a selection the bar drives from one drawable, whose own
	// command set is shown and where the focus would silently filter the next multi selection.
	SmartSelectionGroup focus;
	focus.thingTemplate = nullptr;
	focus.objectID = INVALID_ID;
	if( m_smartSelectionActive >= 0 && TheInGameUI->getSelectCount() > 1 )
	{
		focus = m_smartSelectionGroups[ m_smartSelectionActive ];
	}
	m_smartSelectionGroups.swap( groups );
	m_smartSelectionActive = -1;
	for( size_t g = 0; focus.thingTemplate && g < m_smartSelectionGroups.size(); g++ )
	{
		const SmartSelectionGroup &group = m_smartSelectionGroups[ g ];
		if( focus.objectID != INVALID_ID ? group.objectID == focus.objectID : group.thingTemplate == focus.thingTemplate )
		{
			m_smartSelectionActive = (Int)g;
			break;
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

	// the bar is one shot on the button, so a lone member's health goes on every frame
	for( size_t g = 0; g < m_smartSelectionGroups.size(); g++ )
	{
		if( m_smartSelectionGroups[ g ].objectID == INVALID_ID || m_smartSelectionButtons[ g ] == nullptr )
		{
			continue;
		}
		const Object *obj = TheGameLogic->findObjectByID( m_smartSelectionGroups[ g ].objectID );
		if( obj == nullptr )
		{
			continue;
		}
		const BodyModuleInterface *body = obj->getBodyModule();
		if( body->getMaxHealth() > 0.0f )
		{
			GadgetButtonDrawHealthBar( m_smartSelectionButtons[ g ], body->getHealth() / body->getMaxHealth() );
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Put the groups on the cameos, with the focused one, or every one of its type, pushed in. */
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
		if( group.objectID == INVALID_ID )
		{
			count.format( L"%d", group.count );
		}
		GadgetButtonSetText( button, count );
		button->winSetTooltip( group.thingTemplate->getDisplayName() );
		GadgetCheckLikeButtonSetVisualCheck( button, isSmartSelectionGroupFocused( i ) );
		button->winHide( FALSE );
	}

	m_smartSelectionParent->winSetSize( MAX( getSmartSelectionRowWidth(), 1 ), m_smartSelectionButtonSize.y );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ControlBar::processSmartSelectionClick( GameWindow *button, Bool rightClick )
{
	const Int groupIndex = (Int)(size_t)GadgetButtonGetData( button ) - 1;
	if( groupIndex < 0 || (size_t)groupIndex >= m_smartSelectionGroups.size() )
	{
		return;
	}

	if( rightClick )
	{
		smartSelectionRemove( groupIndex, FALSE );
		return;
	}

	Bool keepOnly;
	if( TheGlobalData->m_smartSelectionUseMouse )
	{
		// The window layer folds a double click into a plain press, so it is found here as a
		// second press on the same cameo within the system double click time.
		const UnsignedInt now = timeGetTime();
		keepOnly = groupIndex == m_smartSelectionLastClickSlot && now - m_smartSelectionLastClickTime <= GetDoubleClickTime();
		m_smartSelectionLastClickSlot = keepOnly ? -1 : groupIndex;
		m_smartSelectionLastClickTime = now;
	}
	else
	{
		// Ctrl and Shift together, because Shift+Tab cycles the row: a click landing while that
		// Shift is still held must not throw the rest of the selection away.
		keepOnly = TheKeyboard && TheKeyboard->isCtrl() && TheKeyboard->isShift();
	}
	if( keepOnly )
	{
		smartSelectionRemove( groupIndex, TRUE );
	}
	else
	{
		smartSelectionFocus( isSmartSelectionGroupFocused( groupIndex ) ? -1 : groupIndex );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/** Step the focus to the next cameo that is not pushed in. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionCycle( Int direction )
{
	const Int groupCount = (Int)m_smartSelectionGroups.size();
	if( groupCount < 2 || direction == 0 )
	{
		return;
	}

	Int next = m_smartSelectionActive;
	for( Int step = 0; step < groupCount; step++ )
	{
		if( next < 0 )
		{
			next = direction > 0 ? 0 : groupCount - 1;
		}
		else
		{
			next = ( next + direction + groupCount ) % groupCount;
		}
		if( !isSmartSelectionGroupFocused( next ) )
		{
			smartSelectionFocus( next );
			return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Show one cameo's command set, or the common set again for -1. Nothing about the selection
	* changes; the dirty flag makes the context repopulate around the new focus. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionFocus( Int groupIndex )
{
	m_smartSelectionActive = groupIndex;
	refreshSmartSelectionButtons();
	markUIDirty();
}

//-------------------------------------------------------------------------------------------------
/** Drop a cameo's units from the selection, or every other unit for keepGroup. A cameo stands
	* for one object or for a whole type. Client side deselect plus one remove message, the same
	* shape as a shift click on a selected unit. The selection change rebuilds the row. */
//-------------------------------------------------------------------------------------------------
void ControlBar::smartSelectionRemove( Int groupIndex, Bool keepGroup )
{
	const SmartSelectionGroup &group = m_smartSelectionGroups[ groupIndex ];

	// gathered first, since deselecting walks the very list being read
	std::vector<Drawable *> members;
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = getSmartSelectionObject( *it );
		if( obj == nullptr )
		{
			continue;
		}
		const Bool inGroup = group.objectID != INVALID_ID ? obj->getID() == group.objectID : obj->getTemplate() == group.thingTemplate;
		if( inGroup != keepGroup )
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

	// what is kept is one type, which shows its own command set, so it needs no focus either
	if( groupIndex == m_smartSelectionActive || keepGroup )
	{
		m_smartSelectionActive = -1;
	}
}

//-------------------------------------------------------------------------------------------------
/** The logic side sends a command to every unit in the player's group that can do it, so a
	* command off the focused card would leak to any other unit with a matching one. The group
	* the command acts on goes ahead of it instead. The client selection is untouched. */
//-------------------------------------------------------------------------------------------------
void ControlBar::appendCommandGroup( const CommandButton *command )
{
	// the bar is that object's own, so anything out of it goes to that object alone
	const ObjectID focusObject = getSmartSelectionFocusObject();
	if( focusObject != INVALID_ID )
	{
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_COMMAND_GROUP );
		msg->appendObjectIDArgument( focusObject );
		return;
	}

	const ThingTemplate *focus = getSmartSelectionFocusTemplate();
	if( focus == nullptr || m_currContext != CB_CONTEXT_MULTI_SELECT )
	{
		return;
	}

	// with a cameo focused the populated commands are its card, so anything else, a shortcut
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

	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_COMMAND_GROUP );
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = ( *it )->getObject();
		if( obj && obj->getTemplate() == focus )
		{
			msg->appendObjectIDArgument( obj->getID() );
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** A multi selection only gets build buttons off a focused dozer's own bar. The builder leads
	* the group and the logic side sends the dozers behind it to help. */
//-------------------------------------------------------------------------------------------------
void ControlBar::appendBuildGroup( const Object *builder )
{
	if( builder == nullptr || TheInGameUI->getSelectCount() < 2 )
	{
		return;
	}
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_COMMAND_GROUP );
	msg->appendObjectIDArgument( builder->getID() );
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		Object *obj = ( *it )->getObject();
		if( obj && obj != builder && obj->isKindOf( KINDOF_DOZER ) )
		{
			msg->appendObjectIDArgument( obj->getID() );
		}
	}
}
