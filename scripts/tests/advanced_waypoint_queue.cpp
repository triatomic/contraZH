#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

typedef bool Bool;
typedef int Int;
typedef unsigned int UnsignedInt;
typedef unsigned short UnsignedShort;
typedef unsigned char UnsignedByte;
typedef UnsignedByte XferVersion;
typedef float Real;
typedef int ObjectID;
typedef int WeaponSlotType;
typedef int SpecialPowerType;
typedef int GuardMode;
const Int INVALID_ID = 0;
const Int NO_MAX_SHOTS_LIMIT = -1;
const Int SC_INVALID_DATA = 1;
const Int CMD_FROM_PLAYER = 0;
const Int AI_FOLLOW_PATH = 1;
enum XferMode { XFER_SAVE, XFER_LOAD };
enum CommandVerifyStatus
{
	STATUS_NONE, STATUS_FRESH, STATUS_AWAITING, STATUS_FOLLOW_PATH_VERIFIED,
	STATUS_EXIT_VERIFIED, STATUS_VERIFIED
};
struct Coord3D
{
	Real x, y, z;
};
typedef std::vector<Coord3D> VecCoord3D;

class AsciiString : public std::string
{
public:
	Bool isNotEmpty() const
	{
		return !empty();
	}
};

class Xfer
{
public:
	XferMode mode;
	std::vector<unsigned char> bytes;
	size_t offset = 0;

	Xfer(XferMode value) : mode(value)
	{
	}

	XferMode getXferMode()
	{
		return mode;
	}

	template <class T> void scalar(T *value)
	{
		if (mode == XFER_LOAD)
		{
			assert(offset + sizeof(T) <= bytes.size());
			std::memcpy(value, &bytes[offset], sizeof(T));
			offset += sizeof(T);
		}
		else
		{
			const unsigned char *data = reinterpret_cast<const unsigned char *>(value);
			bytes.insert(bytes.end(), data, data + sizeof(T));
		}
	}

	void xferVersion(XferVersion *version, XferVersion maximum)
	{
		scalar(version);
		assert(*version <= maximum);
	}

	void xferInt(Int *value) { scalar(value); }
	void xferUnsignedInt(UnsignedInt *value) { scalar(value); }
	void xferUnsignedShort(UnsignedShort *value) { scalar(value); }
	void xferObjectID(ObjectID *value) { scalar(value); }
	void xferCoord3D(Coord3D *value) { scalar(value); }
	void xferReal(Real *value) { scalar(value); }
	void xferBool(Bool *value) { scalar(value); }
	void xferAsciiString(AsciiString *value)
	{
		UnsignedInt count = value->size();
		scalar(&count);
		value->resize(count);
		for (UnsignedInt i = 0; i < count; ++i)
		{
			scalar(&(*value)[i]);
		}
	}
	template <class T> void xferSnapshot(T *) {}
};

class Snapshot {};
class SubsystemInterface {};
class Drawable {};
class Money
{
public:
	UnsignedInt balance = 0;
	void deposit(UnsignedInt amount, Bool, Bool)
	{
		balance += amount;
	}
};
class Player
{
public:
	Money money;
	Money *getMoney()
	{
		return &money;
	}
};
class AIUpdateInterface
{
public:
	Int getAIStateType() { return AI_FOLLOW_PATH; }
	void aiFollowPath(VecCoord3D *, void *, Int, Bool, UnsignedInt) {}
	void aiFollowPathAppend(const Coord3D *, Int, Bool, UnsignedInt) {}
};
class Object
{
public:
	ObjectID id;
	Player *player;
	UnsignedInt current = 0, last = 0, streak = 0;
	CommandVerifyStatus status = STATUS_NONE;

	Object(ObjectID value, Player *owner) : id(value), player(owner) {}
	ObjectID getID() { return id; }
	UnsignedInt getCurrentCommand() { return current; }
	UnsignedInt getLastCommand() { return last; }
	UnsignedInt getLastCommandInMoveStreak() { return streak; }
	CommandVerifyStatus getCommandVerifyStatus() { return status; }
	void setCurrentCommand(UnsignedInt value) { current = value; }
	void setLastCommand(UnsignedInt value) { last = value; }
	void setLastCommandInMoveStreak(UnsignedInt value) { streak = value; }
	void setCommandVerifyStatus(CommandVerifyStatus value) { status = value; }
	void clearPendingExitList() {}
	Player *getControllingPlayer() { return player; }
	AIUpdateInterface *getAI() { return nullptr; }
};
class GameLogic
{
public:
	std::map<ObjectID, Object *> objects;
	Object *findObjectByID(ObjectID id)
	{
		return objects.count(id) ? objects[id] : nullptr;
	}
};
class GameClient
{
public:
	void destroyDrawable(Drawable *) {}
};
class ThingTemplate
{
public:
	Bool isEquivalentTo(const ThingTemplate *other) const
	{
		return this == other;
	}
};
class ThingFactory
{
public:
	ThingTemplate building;
	const ThingTemplate *findByTemplateID(UnsignedShort)
	{
		return &building;
	}
};
GameLogic gameLogic;
GameLogic *TheGameLogic = &gameLogic;
GameClient gameClient;
GameClient *TheGameClient = &gameClient;
ThingFactory thingFactory;
ThingFactory *TheThingFactory = &thingFactory;

#define private public
#define protected public
#define override
#include "command_sequence.h"
#undef override
#undef protected
#undef private
#include "command_sequence_methods.h"
void CommandSequence::update() {}
void CommandSequence::crc(Xfer *) {}
void CommandSequence::loadPostProcess() {}

struct PathPointInfo
{
	Bool m_isReverse;
	UnsignedInt m_cmdNodeID;
	PathPointInfo(Bool reverse = false, UnsignedInt command = 0) : m_isReverse(reverse), m_cmdNodeID(command) {}
};
typedef UnsignedInt StateID;
const StateID INVALID_STATE_ID = ~0U;
class Squad {};
class State
{
public:
	StateID getID() { return INVALID_STATE_ID; }
};
class Waypoint
{
public:
	AsciiString getName() { return AsciiString(); }
};
class TerrainLogic
{
public:
	Waypoint *getWaypointByName(const AsciiString &) { return nullptr; }
};
TerrainLogic terrain;
TerrainLogic *TheTerrainLogic = &terrain;
class StateMachine
{
public:
	void xfer(Xfer *) {}
};
class AIStateMachine : public StateMachine
{
public:
	std::vector<Coord3D> m_goalPath;
	std::vector<PathPointInfo> m_goalPathInfo;
	Waypoint *m_goalWaypoint = nullptr;
	Squad *m_goalSquad = nullptr;
	State *m_temporaryState = nullptr;
	UnsignedInt m_temporaryStateFramEnd = 0;
	State *internalGetState(StateID) { return nullptr; }
	void xfer(Xfer *);
	const PathPointInfo *getPathPointInfo(Int) const;
};
#define newInstance(type) new type
#define DEBUG_ASSERTCRASH(condition, message) assert(condition)
#include "path_methods.h"

int main()
{
	Player owner, opponent;
	Object builder(1, &owner), enemy(2, &opponent);
	gameLogic.objects[1] = &builder;
	gameLogic.objects[2] = &enemy;
	CommandSequence queue;
	queue.init();

	UnsignedInt empty = queue.newCommandNode(CommandSequence::COMMAND_TOGGLE_HOLD_FIRE)->m_nodeID;
	queue.cleanUpEmptyCommand(empty);
	queue.cleanUpEmptyCommand(empty);
	assert(queue.m_allNodes.empty() && queue.m_commandNodeCount == 0);
	queue.reset();

	CommandSequence::CommandNode *guard = queue.newCommandNode(CommandSequence::COMMAND_DO_GUARD_OBJECT);
	assert(queue.includeUnitInCommand(guard->m_nodeID, &builder));
	assert(!queue.includeUnitInCommand(guard->m_nodeID, &builder));
	assert(guard->m_nextCommand[builder.id] == 0);
	assert(queue.removeCurrentCommand(&builder) == 0);
	assert(queue.m_allNodes.empty());

	CommandSequence::CommandNode *build = queue.newCommandNode(CommandSequence::COMMAND_DOZER_CONSTRUCT);
	assert(queue.includeUnitInCommand(build->m_nodeID, &builder));
	build->m_templateID = 42;
	build->m_cashWithdraw = 800;
	CommandSequence::CommandNode *resume = queue.newCommandNode(CommandSequence::COMMAND_RESUME_CONSTRUCTION);
	resume->m_targetID = 123;
	assert(queue.includeUnitInCommand(resume->m_nodeID, &builder));
	assert(queue.getConstructionShadowCount(&thingFactory.building, &owner) == 1);
	assert(queue.getConstructionShadowCount(&thingFactory.building, &opponent) == 0);
	queue.m_constructionShadows.clear();
	assert(queue.isPendingConstruction(build, &owner));
	builder.status = STATUS_VERIFIED;
	assert(!queue.isPendingConstruction(build, &owner));
	builder.status = STATUS_FRESH;

	Xfer saved(XFER_SAVE);
	queue.xfer(&saved);
	CommandSequence loaded;
	loaded.init();
	Xfer input(XFER_LOAD);
	input.bytes = saved.bytes;
	loaded.xfer(&input);
	assert(input.offset == input.bytes.size());
	assert(loaded.findCommandByID(build->m_nodeID)->m_nodeID == build->m_nodeID);
	assert(loaded.findCommandByID(build->m_nodeID)->m_cashWithdraw == 800);
	assert(loaded.findCommandByID(resume->m_nodeID)->m_targetID == 123);
	loaded.clearObjectCommand(&builder);
	assert(owner.money.balance == 800);
	assert(loaded.m_allNodes.empty() && loaded.m_commandNodeCount == 0);
	assert(builder.current == 0 && builder.last == 0);
	loaded.reset();

	CommandSequence::CommandNode *terminal = queue.newCommandNode(CommandSequence::COMMAND_DO_GUARD_POSITION);
	assert(queue.includeUnitInCommand(terminal->m_nodeID, &enemy));
	CommandSequence::CommandNode *rejected = queue.newCommandNode(CommandSequence::COMMAND_DOZER_CONSTRUCT);
	UnsignedInt rejectedID = rejected->m_nodeID;
	assert(!queue.includeUnitInCommand(rejectedID, &enemy));
	assert(!queue.isPendingConstruction(rejected, &opponent));
	queue.cleanUpEmptyCommand(rejectedID);
	assert(queue.findCommandByID(rejectedID) == nullptr);
	queue.reset();

	AIStateMachine path;
	path.m_goalPath.push_back(Coord3D{1, 2, 3});
	path.m_goalPath.push_back(Coord3D{4, 5, 6});
	path.m_goalPathInfo.push_back(PathPointInfo(false, 11));
	path.m_goalPathInfo.push_back(PathPointInfo(true, 12));
	Xfer pathSave(XFER_SAVE);
	path.xfer(&pathSave);
	AIStateMachine restoredPath;
	Xfer pathLoad(XFER_LOAD);
	pathLoad.bytes = pathSave.bytes;
	restoredPath.xfer(&pathLoad);
	assert(pathLoad.offset == pathLoad.bytes.size());
	assert(restoredPath.m_goalPath.size() == 2);
	assert(restoredPath.m_goalPath[1].x == 4);
	assert(restoredPath.getPathPointInfo(0)->m_cmdNodeID == 11);
	assert(restoredPath.getPathPointInfo(1)->m_cmdNodeID == 12);
	assert(restoredPath.getPathPointInfo(1)->m_isReverse);
	assert(restoredPath.getPathPointInfo(2) == nullptr);

	Xfer legacyPath(XFER_SAVE);
	XferVersion legacyVersion = 1;
	legacyPath.xferVersion(&legacyVersion, 1);
	Int legacyCount = 1;
	legacyPath.xferInt(&legacyCount);
	Coord3D legacyPosition = {7, 8, 9};
	legacyPath.xferCoord3D(&legacyPosition);
	AsciiString noWaypoint;
	legacyPath.xferAsciiString(&noWaypoint);
	Bool noSquad = false;
	legacyPath.xferBool(&noSquad);
	StateID noState = INVALID_STATE_ID;
	legacyPath.xferUnsignedInt(&noState);
	UnsignedInt noFrame = 0;
	legacyPath.xferUnsignedInt(&noFrame);
	Xfer legacyLoad(XFER_LOAD);
	legacyLoad.bytes = legacyPath.bytes;
	restoredPath.xfer(&legacyLoad);
	assert(legacyLoad.offset == legacyLoad.bytes.size());
	assert(restoredPath.m_goalPath.size() == 1);
	assert(restoredPath.m_goalPath[0].x == 7);
	assert(!restoredPath.getPathPointInfo(0)->m_isReverse);
	assert(restoredPath.getPathPointInfo(0)->m_cmdNodeID == 0);
	std::cout << "Advanced waypoint queue regression checks passed\n";
}
