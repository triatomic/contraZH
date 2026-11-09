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

// Both mouse buttons set a formation destination and facing.

#pragma once

#include <vector>

#include "Lib/BaseType.h"
#include "Common/MessageStream.h"

enum FacingDragButton CPP_11(: Int)
{
	FACING_DRAG_LEFT,
	FACING_DRAG_RIGHT,
};

enum FacingDragAction CPP_11(: Int)
{
	FACING_DRAG_PASS,			///< not ours: other translators handle it
	FACING_DRAG_SWALLOW,	///< part of a drag: nobody else sees it
	FACING_DRAG_BEGIN,		///< both buttons now held: the drag starts here
	FACING_DRAG_FINISH,		///< a button went up: give the order
};

struct FacingDragState
{
	FacingDragState() : leftDown(FALSE), rightDown(FALSE), active(FALSE), swallowing(FALSE)
	{
		anchor.x = anchor.y = 0;
	}

	Bool leftDown;
	Bool rightDown;
	Bool active;
	Bool swallowing;
	ICoord2D anchor;
};

/// One button event. canOrder: there is a selection to give the order to.
FacingDragAction facingDragOnButton( FacingDragState &state, FacingDragButton button, Bool down, const ICoord2D &pos,
	Bool canOrder );

/// From the chord until both buttons are up: the selection box and right-button scroll must stay off.
Bool doesFacingDragOwnButtons( const FacingDragState &state );

/// A drag shorter than this is a plain move.
Bool isFacingDragLongEnough( const ICoord2D &from, const ICoord2D &to, Int minPixels );

/// World angle (radians, as Object orientation) from the chord's point to the dragged point.
Real facingAngle( const Coord3D &from, const Coord3D &to );

/// Ctrl snaps the facing to absolute 45 degree steps.
Real snapFacingAngle( Real angle, Bool snap );

//-------------------------------------------------------------------------------------------------
class FacingDragTranslator : public GameMessageTranslator
{
public:
	FacingDragTranslator();
	void reset();
	virtual GameMessageDisposition translateGameMessage( const GameMessage *msg ) override;

private:
	GameMessageDisposition onButton( FacingDragButton button, Bool down, const GameMessage *msg );
	void giveOrder( const ICoord2D &releasePos );
	void blockOtherTranslators( Bool block );
	Bool computeFacing( const ICoord2D &cursor, Int moverCount, Real *angle, Int *rows ) const;
	void beginPreview();
	void updatePreview( const ICoord2D &cursor );
	void endPreview();
	void destroyGhosts();

	FacingDragState m_state;
	Bool m_hotkeyHeld;
	Bool m_blocking;	///< selection box and right-button scroll are blocked
	UnsignedInt m_style;	///< FormationStyle from Options.ini, sent with each order
	Bool m_swallowClick[2];	///< the meta translator puts a click after each button up
	Coord3D m_anchorWorld;
	Bool m_anchorValid;
	std::vector<UnsignedInt> m_ghostObjects;	///< ObjectID of each mover at the chord
	std::vector<UnsignedInt> m_ghosts;				///< DrawableID of its ghost, 0 if none (looked up, so a client reset is harmless)
};
