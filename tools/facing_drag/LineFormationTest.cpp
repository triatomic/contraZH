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
FormationMember member( UnsignedInt id, Real x, Real y, Real radius = 10.0f, Real toughness = 100.0f )
{
	FormationMember m;
	m.id = id;
	m.pos.x = x;
	m.pos.y = y;
	m.pos.z = 0.0f;
	m.radius = radius;
	m.toughness = toughness;
	return m;
}

const Coord3D CENTER = { 100.0f, 100.0f, 0.0f };
const Real HALF_PI = 1.5707964f;
} // namespace

// One row at first; every further step adds one, never more rows than units.
TEST(LineFormation, DraggingFurtherAddsRows)
{
	EXPECT_EQ(lineFormationRows(0.0f, 10), 1);
	EXPECT_EQ(lineFormationRows(FORMATION_ROW_START, 10), 1);
	EXPECT_EQ(lineFormationRows(FORMATION_ROW_START + FORMATION_ROW_STEP * 0.5f, 10), 1);
	EXPECT_EQ(lineFormationRows(FORMATION_ROW_START + FORMATION_ROW_STEP, 10), 2);
	EXPECT_EQ(lineFormationRows(FORMATION_ROW_START + FORMATION_ROW_STEP * 2.5f, 10), 3);
	EXPECT_EQ(lineFormationRows(10000.0f, 3), 3);
	EXPECT_EQ(lineFormationRows(10000.0f, 1), 1);
}

// Facing +x: one line along y through the point, each unit on its own side (no crossing).
TEST(LineFormation, OneRowAcrossTheFacing)
{
	const FormationMember m[3] = { member(1, 0.0f, 50.0f), member(2, 0.0f, 300.0f), member(3, 0.0f, 150.0f) };
	Coord3D slots[3];
	computeLineFormation(CENTER, 0.0f, 1, m, 3, slots);

	const Real spacing = 2.0f * 10.0f + FORMATION_GAP;
	for (Int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(slots[i].x, 100.0f, 1e-3f);
	}
	EXPECT_NEAR(slots[0].y, 100.0f - spacing, 1e-3f);	// the rightmost unit (lowest y) takes the right end
	EXPECT_NEAR(slots[2].y, 100.0f, 1e-3f);
	EXPECT_NEAR(slots[1].y, 100.0f + spacing, 1e-3f);
}

// Facing +y: the line runs along x instead.
TEST(LineFormation, TurnsWithTheFacing)
{
	const FormationMember m[2] = { member(1, 0.0f, 0.0f), member(2, 200.0f, 0.0f) };
	Coord3D slots[2];
	computeLineFormation(CENTER, HALF_PI, 1, m, 2, slots);
	EXPECT_NEAR(slots[0].y, 100.0f, 1e-3f);
	EXPECT_NEAR(slots[1].y, 100.0f, 1e-3f);
	EXPECT_LT(slots[0].x, slots[1].x);	// left of +y is -x: the unit at x=0 stays on the -x side
}

// Two rows centred on the point: the toughest in front (towards the facing), the rest behind.
TEST(LineFormation, ToughestInTheFrontRow)
{
	const FormationMember m[4] = {
		member(1, 0.0f, 0.0f, 10.0f, 50.0f), member(2, 0.0f, 20.0f, 10.0f, 400.0f),
		member(3, 0.0f, 40.0f, 10.0f, 60.0f), member(4, 0.0f, 60.0f, 10.0f, 300.0f) };
	Coord3D slots[4];
	computeLineFormation(CENTER, 0.0f, 2, m, 4, slots);

	const Real halfDepth = (10.0f + 10.0f + FORMATION_GAP) * 0.5f;
	EXPECT_NEAR(slots[1].x, 100.0f + halfDepth, 1e-3f);
	EXPECT_NEAR(slots[3].x, 100.0f + halfDepth, 1e-3f);
	EXPECT_NEAR(slots[0].x, 100.0f - halfDepth, 1e-3f);
	EXPECT_NEAR(slots[2].x, 100.0f - halfDepth, 1e-3f);
}

// 5 units in 2 rows: 3 in front, the 2 behind centred on the point.
TEST(LineFormation, ShortLastRowIsCentred)
{
	std::vector<FormationMember> m;
	for (UnsignedInt i = 0; i < 5; ++i)
	{
		m.push_back(member(i + 1, 0.0f, (Real)i * 10.0f, 10.0f, i < 3 ? 200.0f : 100.0f));
	}
	Coord3D slots[5];
	computeLineFormation(CENTER, 0.0f, 2, &m[0], 5, slots);

	const Real spacing = 2.0f * 10.0f + FORMATION_GAP;
	EXPECT_NEAR(slots[3].y, 100.0f - spacing * 0.5f, 1e-3f);
	EXPECT_NEAR(slots[4].y, 100.0f + spacing * 0.5f, 1e-3f);
	EXPECT_NEAR(slots[1].y, 100.0f, 1e-3f);
}

// The logic and the preview list the units in different orders: every unit must still get the same slot.
TEST(LineFormation, OrderOfTheUnitsDoesNotMatter)
{
	const FormationMember a[4] = { member(7, 0.0f, 0.0f), member(3, 0.0f, 0.0f), member(9, 0.0f, 0.0f), member(5, 0.0f, 0.0f) };
	const FormationMember b[4] = { a[2], a[0], a[3], a[1] };
	Coord3D slotsA[4];
	Coord3D slotsB[4];
	computeLineFormation(CENTER, 0.7f, 2, a, 4, slotsA);
	computeLineFormation(CENTER, 0.7f, 2, b, 4, slotsB);
	const Int aOfB[4] = { 2, 0, 3, 1 };
	for (Int i = 0; i < 4; ++i)
	{
		EXPECT_FLOAT_EQ(slotsB[i].x, slotsA[aOfB[i]].x);
		EXPECT_FLOAT_EQ(slotsB[i].y, slotsA[aOfB[i]].y);
	}
}

// Mixed sizes in several rows: no two slots closer than the two units' radii.
TEST(LineFormation, UnitsDoNotOverlap)
{
	std::vector<FormationMember> m;
	for (UnsignedInt i = 0; i < 9; ++i)
	{
		m.push_back(member(i + 1, (Real)(i * 37 % 11) * 20.0f, (Real)(i * 53 % 7) * 20.0f, (i % 3 == 0) ? 25.0f : 6.0f,
			(Real)(i % 4) * 100.0f));
	}
	std::vector<Coord3D> slots(m.size());
	computeLineFormation(CENTER, 2.3f, 3, &m[0], (Int)m.size(), &slots[0]);
	for (size_t i = 0; i < m.size(); ++i)
	{
		for (size_t j = i + 1; j < m.size(); ++j)
		{
			const Real dx = slots[i].x - slots[j].x;
			const Real dy = slots[i].y - slots[j].y;
			EXPECT_GE(std::sqrt(dx * dx + dy * dy) + 1e-3f, m[i].radius + m[j].radius) << i << " " << j;
		}
	}
}

// Asking for more rows than units, or none, is clamped.
TEST(LineFormation, RowCountIsClamped)
{
	const FormationMember m[2] = { member(1, 0.0f, 0.0f), member(2, 0.0f, 50.0f) };
	Coord3D one[2];
	Coord3D zero[2];
	Coord3D many[2];
	computeLineFormation(CENTER, 0.0f, 1, m, 2, one);
	computeLineFormation(CENTER, 0.0f, 0, m, 2, zero);
	computeLineFormation(CENTER, 0.0f, 9, m, 2, many);
	EXPECT_FLOAT_EQ(zero[0].y, one[0].y);
	EXPECT_NEAR(many[0].y, 100.0f, 1e-3f);	// 2 rows of 1, both on the centre line
	EXPECT_NEAR(many[1].y, 100.0f, 1e-3f);
	EXPECT_NE(many[0].x, many[1].x);
}

// Wheels drive in through a point behind the slot: at least the minimum, more for big units.
TEST(LineFormation, ApproachPointLiesBehindTheSlot)
{
	EXPECT_FLOAT_EQ(formationApproachDistance(2.0f), FORMATION_APPROACH_MIN);
	EXPECT_FLOAT_EQ(formationApproachDistance(20.0f), 20.0f * FORMATION_APPROACH_RADII);

	const Coord3D slot = { 100.0f, 100.0f, 5.0f };
	const Coord3D east = formationApproachPoint(slot, 0.0f, 30.0f);	// facing +x: approach from -x
	EXPECT_NEAR(east.x, 70.0f, 1e-3f);
	EXPECT_NEAR(east.y, 100.0f, 1e-3f);
	EXPECT_FLOAT_EQ(east.z, 5.0f);
	const Coord3D north = formationApproachPoint(slot, HALF_PI, 30.0f);
	EXPECT_NEAR(north.x, 100.0f, 1e-3f);
	EXPECT_NEAR(north.y, 70.0f, 1e-3f);
}

// Coming in from behind, inside the cone and far enough: no detour. From the front, the side, or too close: detour.
TEST(LineFormation, ApproachOnlyWhenNotComingInFromBehind)
{
	const Coord3D slot = { 100.0f, 100.0f, 0.0f };
	const Coord3D behind = { 0.0f, 110.0f, 0.0f };
	const Coord3D front = { 200.0f, 100.0f, 0.0f };
	const Coord3D side = { 100.0f, 0.0f, 0.0f };
	const Coord3D wideBehind = { 50.0f, 160.0f, 0.0f };	// 50 behind, 60 aside: outside 30 degrees
	const Coord3D closeBehind = { 90.0f, 100.0f, 0.0f };
	EXPECT_FALSE(needsFormationApproach(behind, slot, 0.0f, 30.0f));
	EXPECT_TRUE(needsFormationApproach(front, slot, 0.0f, 30.0f));
	EXPECT_TRUE(needsFormationApproach(side, slot, 0.0f, 30.0f));
	EXPECT_TRUE(needsFormationApproach(wideBehind, slot, 0.0f, 30.0f));
	EXPECT_TRUE(needsFormationApproach(closeBehind, slot, 0.0f, 30.0f));
}

// The pathfinder snapping a slot within its own cell (10) means open ground: go there exactly. Moved further
// (building, cliff, another unit's goal): keep the pathfinder's spot.
TEST(LineFormation, ExactOnlyWhenThePathfinderKeepsTheCell)
{
	const Coord3D slot = { 103.0f, 107.0f, 0.0f };
	const Coord3D ownCell = { 105.0f, 105.0f, 0.0f };
	const Coord3D nextCell = { 115.0f, 105.0f, 0.0f };
	const Coord3D farAway = { 160.0f, 107.0f, 0.0f };
	EXPECT_TRUE(canMoveExactlyToSlot(slot, slot));
	EXPECT_TRUE(canMoveExactlyToSlot(slot, ownCell));
	EXPECT_FALSE(canMoveExactlyToSlot(slot, nextCell));
	EXPECT_FALSE(canMoveExactlyToSlot(slot, farAway));
}

namespace
{
// Do the straight paths a->b and c->d properly cross?
bool pathsCross(const Coord3D &a, const Coord3D &b, const Coord3D &c, const Coord3D &d)
{
	auto side = [](const Coord3D &p, const Coord3D &q, const Coord3D &r) {
		return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
	};
	const Real d1 = side(a, b, c), d2 = side(a, b, d), d3 = side(c, d, a), d4 = side(c, d, b);
	return ((d1 > 1e-3f && d2 < -1e-3f) || (d1 < -1e-3f && d2 > 1e-3f)) && ((d3 > 1e-3f && d4 < -1e-3f) || (d3 < -1e-3f && d4 > 1e-3f));
}

// A small fixed pseudo-random sequence, so the test is the same every run.
Real nextRandom(UnsignedInt &seed)
{
	seed = seed * 1103515245u + 12345u;
	return (Real)((seed >> 8) % 10000) / 10000.0f;
}
} // namespace

// Identical units (one band) coming from anywhere, any facing, 1-3 rows: no two straight paths cross.
TEST(LineFormation, SameBandPathsNeverCross)
{
	UnsignedInt seed = 7;
	for (Int round = 0; round < 40; ++round)
	{
		std::vector<FormationMember> m;
		const Int n = 3 + round % 10;
		for (Int i = 0; i < n; ++i)
		{
			m.push_back(member((UnsignedInt)(i + 1), nextRandom(seed) * 600.0f - 300.0f, nextRandom(seed) * 600.0f - 300.0f));
		}
		const Real angle = nextRandom(seed) * 6.2831853f;
		const Int rows = 1 + round % 3;
		std::vector<Coord3D> slots(m.size());
		computeLineFormation(CENTER, angle, rows, &m[0], n, &slots[0]);
		for (Int i = 0; i < n; ++i)
		{
			for (Int j = i + 1; j < n; ++j)
			{
				EXPECT_FALSE(pathsCross(m[i].pos, slots[i], m[j].pos, slots[j])) << "round " << round << " units " << i << "," << j;
			}
		}
	}
}

// ToughFront: tanks take the front row even when the infantry start nearer to it.
TEST(LineFormation, BandsKeepTheToughestInFront)
{
	// facing +x; front row at x > 100. Infantry stand ahead (x = 300), tanks behind (x = -200).
	const FormationMember m[4] = {
		member(1, 300.0f, 90.0f, 6.0f, 100.0f), member(2, 300.0f, 110.0f, 6.0f, 100.0f),
		member(3, -200.0f, 90.0f, 15.0f, 400.0f), member(4, -200.0f, 110.0f, 15.0f, 380.0f) };
	Coord3D slots[4];
	computeLineFormation(CENTER, 0.0f, 2, m, 4, slots);
	EXPECT_GT(slots[2].x, 100.0f);
	EXPECT_GT(slots[3].x, 100.0f);
	EXPECT_LT(slots[0].x, 100.0f);
	EXPECT_LT(slots[1].x, 100.0f);
}

// Nearest: the same group takes the nearest slots instead - infantry in front - and rows fit the largest unit.
TEST(LineFormation, NearestIgnoresToughness)
{
	const FormationMember m[4] = {
		member(1, 300.0f, 90.0f, 6.0f, 100.0f), member(2, 300.0f, 110.0f, 6.0f, 100.0f),
		member(3, -200.0f, 90.0f, 15.0f, 400.0f), member(4, -200.0f, 110.0f, 15.0f, 380.0f) };
	Coord3D slots[4];
	computeLineFormation(CENTER, 0.0f, 2, m, 4, slots, FORMATION_STYLE_NEAREST);
	EXPECT_GT(slots[0].x, 100.0f);
	EXPECT_GT(slots[1].x, 100.0f);
	EXPECT_LT(slots[2].x, 100.0f);
	EXPECT_LT(slots[3].x, 100.0f);
	for (Int i = 0; i < 4; ++i)
	{
		for (Int j = i + 1; j < 4; ++j)
		{
			const Real dx = slots[i].x - slots[j].x;
			const Real dy = slots[i].y - slots[j].y;
			EXPECT_GE(std::sqrt(dx * dx + dy * dy) + 1e-3f, m[i].radius + m[j].radius);
		}
	}
}

// Units within the band width of each other share a band and may swap rows; a bigger gap starts a new band.
TEST(LineFormation, SimilarToughnessSharesABand)
{
	// facing +x: 4 units, 2 rows. Toughness 400 and 360 (10% apart) share a band, so the two front slots go to
	// whoever is nearer - here the 360 ones standing ahead.
	const FormationMember m[4] = {
		member(1, -200.0f, 90.0f, 10.0f, 400.0f), member(2, -200.0f, 110.0f, 10.0f, 400.0f),
		member(3, 300.0f, 90.0f, 10.0f, 360.0f), member(4, 300.0f, 110.0f, 10.0f, 360.0f) };
	Coord3D slots[4];
	computeLineFormation(CENTER, 0.0f, 2, m, 4, slots);
	EXPECT_GT(slots[2].x, 100.0f);
	EXPECT_GT(slots[3].x, 100.0f);
	EXPECT_LT(slots[0].x, 100.0f);
}

// Defaults (keys absent or unknown): ToughFront, DriveIn, MarchTogether - the family's formation move.
TEST(LineFormation, StyleFromOptions)
{
	const UnsignedInt march = FORMATION_STYLE_SPEED_MARCH;
	EXPECT_EQ(parseFormationStyle(nullptr, nullptr, nullptr), march);
	EXPECT_EQ(parseFormationStyle("ToughFront", "DriveIn", "MarchTogether"), march);
	EXPECT_EQ(parseFormationStyle("nearest", nullptr, nullptr), (UnsignedInt)FORMATION_STYLE_NEAREST | march);
	EXPECT_EQ(parseFormationStyle(nullptr, "TurnInPlace", nullptr), (UnsignedInt)FORMATION_STYLE_WHEELS_TURN_IN_PLACE | march);
	EXPECT_EQ(parseFormationStyle(nullptr, nullptr, "Own"), (UnsignedInt)FORMATION_STYLE_SPEED_OWN);
	EXPECT_EQ(parseFormationStyle(nullptr, nullptr, "arrivetogether"), (UnsignedInt)FORMATION_STYLE_SPEED_ARRIVE);
	EXPECT_EQ(parseFormationStyle("bogus", "bogus", "bogus"), march);
}

// MarchTogether: everyone faster than the slowest is held to it; the slowest itself, and Own, have no limit (0).
TEST(LineFormation, MarchAtTheSlowestSpeed)
{
	EXPECT_FLOAT_EQ(formationUnitSpeed(FORMATION_STYLE_SPEED_MARCH, 300.0f, 60.0f, 20.0f, 15.0f), 20.0f);
	EXPECT_FLOAT_EQ(formationUnitSpeed(FORMATION_STYLE_SPEED_MARCH, 300.0f, 20.0f, 20.0f, 15.0f), 0.0f);
	EXPECT_FLOAT_EQ(formationUnitSpeed(FORMATION_STYLE_SPEED_OWN, 300.0f, 60.0f, 20.0f, 15.0f), 0.0f);
	EXPECT_FLOAT_EQ(formationUnitSpeed(0, 300.0f, 60.0f, 20.0f, 15.0f), 0.0f);	// orders from before styles
}

// ArriveTogether: distance / the group's arrival time, never below 40% of its own speed, never above it.
TEST(LineFormation, ArriveTogetherSpreadsTheSpeeds)
{
	const UnsignedInt arrive = FORMATION_STYLE_SPEED_ARRIVE;
	EXPECT_FLOAT_EQ(formationUnitSpeed(arrive, 150.0f, 60.0f, 20.0f, 5.0f), 30.0f);	// 150 / 5 s
	EXPECT_FLOAT_EQ(formationUnitSpeed(arrive, 300.0f, 60.0f, 20.0f, 5.0f), 0.0f);	// the one setting the time: full speed
	EXPECT_FLOAT_EQ(formationUnitSpeed(arrive, 10.0f, 60.0f, 20.0f, 5.0f), 24.0f);	// floor: 40% of 60
	EXPECT_FLOAT_EQ(formationUnitSpeed(arrive, 100.0f, 60.0f, 20.0f, 0.0f), 0.0f);	// nobody has to move
}
