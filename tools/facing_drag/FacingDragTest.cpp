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



namespace
{
ICoord2D px(Int x, Int y)
{
	ICoord2D p = { x, y };
	return p;
}
} // namespace

// Right then left (or left then right) held: the drag starts at the second press, which nobody else sees.
TEST(FacingDrag, BothButtonsStartTheDragInEitherOrder)
{
	FacingDragState rightFirst;
	EXPECT_EQ(facingDragOnButton(rightFirst, FACING_DRAG_RIGHT, TRUE, px(100, 100), TRUE), FACING_DRAG_PASS);
	EXPECT_EQ(facingDragOnButton(rightFirst, FACING_DRAG_LEFT, TRUE, px(110, 105), TRUE), FACING_DRAG_BEGIN);
	EXPECT_TRUE(rightFirst.active);
	EXPECT_EQ(rightFirst.anchor.x, 110);

	FacingDragState leftFirst;
	EXPECT_EQ(facingDragOnButton(leftFirst, FACING_DRAG_LEFT, TRUE, px(5, 5), TRUE), FACING_DRAG_PASS);
	EXPECT_EQ(facingDragOnButton(leftFirst, FACING_DRAG_RIGHT, TRUE, px(6, 6), TRUE), FACING_DRAG_BEGIN);
}

// One button alone stays the game's own click, drag box or scroll.
TEST(FacingDrag, OneButtonIsLeftAlone)
{
	FacingDragState s;
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_RIGHT, TRUE, px(0, 0), TRUE), FACING_DRAG_PASS);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_RIGHT, FALSE, px(50, 0), TRUE), FACING_DRAG_PASS);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_LEFT, TRUE, px(0, 0), TRUE), FACING_DRAG_PASS);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_LEFT, FALSE, px(0, 0), TRUE), FACING_DRAG_PASS);
	EXPECT_FALSE(s.active);
}

// Nothing selected: both buttons do what they always did.
TEST(FacingDrag, NoSelectionNoDrag)
{
	FacingDragState s;
	facingDragOnButton(s, FACING_DRAG_RIGHT, TRUE, px(0, 0), FALSE);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_LEFT, TRUE, px(0, 0), FALSE), FACING_DRAG_PASS);
	EXPECT_FALSE(s.active);
}

// The first release gives the order; the other button is eaten until it is up too, then all is normal again.
TEST(FacingDrag, FirstReleaseOrdersAndTheOtherButtonIsEaten)
{
	FacingDragState s;
	facingDragOnButton(s, FACING_DRAG_RIGHT, TRUE, px(0, 0), TRUE);
	facingDragOnButton(s, FACING_DRAG_LEFT, TRUE, px(0, 0), TRUE);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_LEFT, FALSE, px(80, 0), TRUE), FACING_DRAG_FINISH);
	EXPECT_FALSE(s.active);
	EXPECT_TRUE(s.swallowing);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_LEFT, TRUE, px(80, 0), TRUE), FACING_DRAG_SWALLOW);	// pressed again meanwhile
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_LEFT, FALSE, px(80, 0), TRUE), FACING_DRAG_SWALLOW);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_RIGHT, FALSE, px(80, 0), TRUE), FACING_DRAG_SWALLOW);
	EXPECT_FALSE(s.swallowing);
	EXPECT_EQ(facingDragOnButton(s, FACING_DRAG_RIGHT, TRUE, px(0, 0), TRUE), FACING_DRAG_PASS);		// normal again
}

// Selection box and right-button scroll stay blocked from the chord until both buttons are up - not just while
// dragging, or the button still held after the order would start a box or a scroll.
TEST(FacingDrag, OwnsTheButtonsFromChordUntilBothAreUp)
{
	FacingDragState s;
	facingDragOnButton(s, FACING_DRAG_LEFT, TRUE, px(0, 0), TRUE);
	EXPECT_FALSE(doesFacingDragOwnButtons(s));	// one button: the box may still start
	facingDragOnButton(s, FACING_DRAG_RIGHT, TRUE, px(0, 0), TRUE);
	EXPECT_TRUE(doesFacingDragOwnButtons(s));
	facingDragOnButton(s, FACING_DRAG_RIGHT, FALSE, px(50, 0), TRUE);
	EXPECT_TRUE(doesFacingDragOwnButtons(s));	// order given, left still held
	facingDragOnButton(s, FACING_DRAG_LEFT, FALSE, px(50, 0), TRUE);
	EXPECT_FALSE(doesFacingDragOwnButtons(s));

	FacingDragState nothingSelected;
	facingDragOnButton(nothingSelected, FACING_DRAG_RIGHT, TRUE, px(0, 0), FALSE);
	facingDragOnButton(nothingSelected, FACING_DRAG_LEFT, TRUE, px(0, 0), FALSE);
	EXPECT_FALSE(doesFacingDragOwnButtons(nothingSelected));
}

TEST(FacingDrag, ShortDragIsAPlainMove)
{
	EXPECT_FALSE(isFacingDragLongEnough(px(100, 100), px(110, 110), 20));
	EXPECT_TRUE(isFacingDragLongEnough(px(100, 100), px(120, 100), 20));
	EXPECT_TRUE(isFacingDragLongEnough(px(100, 100), px(100, 70), 20));
}

// Same convention as Object orientation: 0 along +x, pi/2 along +y.
TEST(FacingDrag, AngleFollowsTheDrag)
{
	const Coord3D origin = { 0.0f, 0.0f, 0.0f };
	const Coord3D east = { 10.0f, 0.0f, 0.0f };
	const Coord3D north = { 0.0f, 10.0f, 0.0f };
	const Coord3D west = { -10.0f, 0.0f, 0.0f };
	EXPECT_NEAR(facingAngle(origin, east), 0.0f, 1e-5f);
	EXPECT_NEAR(facingAngle(origin, north), 1.5707964f, 1e-5f);
	EXPECT_NEAR(std::fabs(facingAngle(origin, west)), 3.1415927f, 1e-5f);
}

// Logic: turn once arrived, but not in the order's own frame, when the move has not started yet.
TEST(FacingMove, FacesOnceArrived)
{
	EXPECT_FALSE(shouldFaceOnArrival(100, 100, FALSE, FALSE, TRUE));	// same frame as the order
	EXPECT_FALSE(shouldFaceOnArrival(101, 100, TRUE, FALSE, TRUE));		// still moving
	EXPECT_FALSE(shouldFaceOnArrival(101, 100, FALSE, TRUE, TRUE));		// still has a path (or waits for one)
	EXPECT_FALSE(shouldFaceOnArrival(101, 100, FALSE, FALSE, FALSE));	// between the legs of an approach path
	EXPECT_TRUE(shouldFaceOnArrival(101, 100, FALSE, FALSE, TRUE));
}

TEST(FacingMove, OnlyALaterOrderCancelsTheFacing)
{
	EXPECT_FALSE(doesCommandCancelFacing(100, 100));	// the move the facing came with
	EXPECT_TRUE(doesCommandCancelFacing(101, 100));
}

// Holding the slot: only a new order from the player (or a script) after the formation order forgets it.
TEST(FacingMove, MakeWayAndOwnOrdersKeepTheSlot)
{
	EXPECT_TRUE(doesCommandKeepFormationSlot(100, 100, FALSE, FALSE));	// the formation order itself
	EXPECT_TRUE(doesCommandKeepFormationSlot(150, 100, TRUE, TRUE));		// an ally's make-way push
	EXPECT_TRUE(doesCommandKeepFormationSlot(150, 100, FALSE, TRUE));		// its own AI: facing turn, fighting
	EXPECT_FALSE(doesCommandKeepFormationSlot(150, 100, FALSE, FALSE));	// the player's next order
}

// Pushed aside: go back if near, just face if still on it, give up if far away or pushed too often.
TEST(FacingMove, ReturnToTheSlotAfterAPush)
{
	EXPECT_EQ(decideFormationReturn(1.0f, 0), FORMATION_RETURN_FACE);
	EXPECT_EQ(decideFormationReturn(FORMATION_RETURN_MIN + 1.0f, 0), FORMATION_RETURN_MOVE);
	EXPECT_EQ(decideFormationReturn(FORMATION_RETURN_MAX, FORMATION_MAX_RETURNS - 1), FORMATION_RETURN_MOVE);
	EXPECT_EQ(decideFormationReturn(FORMATION_RETURN_MAX + 1.0f, 0), FORMATION_RETURN_FORGET);
	EXPECT_EQ(decideFormationReturn(40.0f, FORMATION_MAX_RETURNS), FORMATION_RETURN_FORGET);
}

// Ctrl held: the facing snaps to the nearest absolute 45 degrees, as building placement does;
// without it the angle stays as dragged.
TEST(FacingDrag, CtrlSnapsToAbsolute45Degrees)
{
	const Real deg = 3.1415927f / 180.0f;
	EXPECT_NEAR(snapFacingAngle(10.0f * deg, TRUE), 0.0f, 1e-5f);
	EXPECT_NEAR(snapFacingAngle(30.0f * deg, TRUE), 45.0f * deg, 1e-5f);
	EXPECT_NEAR(snapFacingAngle(100.0f * deg, TRUE), 90.0f * deg, 1e-5f);
	EXPECT_NEAR(snapFacingAngle(-30.0f * deg, TRUE), -45.0f * deg, 1e-5f);
	EXPECT_NEAR(snapFacingAngle(170.0f * deg, TRUE), 180.0f * deg, 1e-5f);
	EXPECT_NEAR(snapFacingAngle(30.0f * deg, FALSE), 30.0f * deg, 1e-5f);
}

TEST(FacingDrag, FormationOrdersUseTheWholeSelection)
{
	EXPECT_TRUE(GameMessage::isPlainOrder(GameMessage::MSG_DO_MOVETO_FACING));
}
