#define MAYBE_UNUSED
#define deleteInstance(p) delete (p)
enum GameMessageDisposition { KEEP_MESSAGE, DESTROY_MESSAGE };
struct TestUI
{
	Int getSelectCount() const { return 4; }
	void setSelecting(Bool) {}
	void endAreaSelectHint(void *) {}
} testUI;
TestUI *TheInGameUI = &testUI;
struct TestView
{
	Bool screenToTerrain(const ICoord2D *, Coord3D *pos) { pos->x = pos->y = pos->z = 0; return TRUE; }
	void setMouseLock(Bool) {}
} testView;
TestView *TheTacticalView = &testView;
struct TestGlobalData { Bool m_facingDrag; } testGlobalData = { TRUE };
TestGlobalData *TheGlobalData = &testGlobalData;
struct SelectionTranslator
{
	Bool m_dragSelectBlocked, m_leftMouseButtonIsDown, m_dragSelecting;
	ICoord2D m_leftMouseDownAnchor;
	SelectionTranslator() : m_dragSelectBlocked(FALSE), m_leftMouseButtonIsDown(FALSE), m_dragSelecting(FALSE) {}
	void setDragSelectBlocked(Bool);
	GameMessageDisposition onRawMouseLeftButtonDown(const GameMessage *);
} testSelection;
SelectionTranslator *TheSelectionTranslator = &testSelection;
struct LookAtTranslator
{
	enum { SCROLL_NONE, SCROLL_RMB };
	Bool m_rightMouseScrollBlocked;
	Int m_scrollType;
	LookAtTranslator() : m_rightMouseScrollBlocked(FALSE), m_scrollType(SCROLL_NONE) {}
	void stopScrolling() { m_scrollType = SCROLL_NONE; }
	void setRightMouseScrollBlocked(Bool);
} testLook;
LookAtTranslator *TheLookAtTranslator = &testLook;
struct FacingDragTranslator
{
	FacingDragState m_state;
	Bool m_blocking, m_anchorValid, m_swallowClick[2];
	Coord3D m_anchorWorld;
	Int orders;
	FacingDragTranslator() : m_blocking(FALSE), m_anchorValid(FALSE), orders(0)
	{
		m_swallowClick[0] = m_swallowClick[1] = FALSE;
	}
	GameMessageDisposition translateGameMessage(const GameMessage *);
	GameMessageDisposition onButton(FacingDragButton, Bool, const GameMessage *);
	void blockOtherTranslators(Bool);
	void beginPreview() {}
	void updatePreview(const ICoord2D &) {}
	void endPreview() {}
	void giveOrder(const ICoord2D &) { ++orders; }
};

enum CommandSourceType { CMD_FROM_PLAYER, CMD_FROM_SCRIPT, CMD_FROM_AI };
enum { AICMD_IDLE, AICMD_MOVE_TO_POSITION, AICMD_FOLLOW_PATH, AICMD_MOVE_AWAY_FROM_UNIT };
struct AICommandParms
{
	Int m_cmd;
	CommandSourceType m_cmdSource;
	AICommandParms(Int cmd, CommandSourceType source) : m_cmd(cmd), m_cmdSource(source) {}
};
static const Real FAST_AS_POSSIBLE = 999999.0f;
struct Locomotor
{
	Bool ultra;
	Real pivot;
	Locomotor() : ultra(FALSE), pivot(0.0f) {}
	void setUltraAccurate(Bool value) { ultra = value; }
	Real getTurnPivotOffset() const { return pivot; }
};
struct Object
{
	Coord3D pos;
	Object() { pos.x = pos.y = pos.z = 0; }
	const Coord3D *getPosition() const { return &pos; }
};
struct TestPathfinder
{
	Bool blocked;
	TestPathfinder() : blocked(FALSE) {}
	void snapPosition(Object *, Coord3D *pos)
	{
		pos->x = floorf(pos->x / 10.0f) * 10.0f + 5.0f;
		pos->y = floorf(pos->y / 10.0f) * 10.0f + 5.0f;
	}
	Bool adjustDestination(Object *obj, Int, Coord3D *pos)
	{
		snapPosition(obj, pos);
		if (blocked)
		{
			pos->x += 10.0f;
		}
		return TRUE;
	}
};
struct TestAI
{
	TestPathfinder finder;
	TestPathfinder *pathfinder() { return &finder; }
} testAI;
TestAI *TheAI = &testAI;
struct TestGameLogic { UnsignedInt getFrame() const { return 100; } } testGameLogic;
TestGameLogic *TheGameLogic = &testGameLogic;
void clampWaypointPosition(Coord3D &, Int) {}
struct AIUpdateInterface
{
	Bool m_faceOnArrival, m_facingAroundCenter, m_exactFormationMove, m_issuingFormationMove;
	Bool m_hasFormationSlot, m_formationReturnPending, exactAtMove, acceptsCommands;
	UnsignedInt m_facingOrderFrame, m_formationReturnFrame;
	Coord3D m_formationSlot;
	Real m_facingOnArrival, m_formationAngle, m_formationRadius, m_formationSpeed, desiredSpeed;
	UnsignedInt m_formationStyle;
	Int m_formationReturns;
	Object unit;
	Locomotor loco;
	AIUpdateInterface() : m_faceOnArrival(FALSE), m_facingAroundCenter(FALSE), m_exactFormationMove(FALSE),
		m_issuingFormationMove(FALSE), m_hasFormationSlot(FALSE), m_formationReturnPending(FALSE),
		exactAtMove(FALSE), acceptsCommands(TRUE), m_facingOrderFrame(0), m_formationReturnFrame(0),
		m_formationSpeed(0), desiredSpeed(FAST_AS_POSSIBLE), m_formationReturns(0) {}
	Object *getObject() { return &unit; }
	Locomotor *getCurLocomotor() { return &loco; }
	Bool isDoingGroundMovement() const { return TRUE; }
	Int getLocomotorSet() const { return 0; }
	Bool isAllowedToRespondToAiCommands(const AICommandParms *) const { return acceptsCommands; }
	void setDesiredSpeed(Real speed) { desiredSpeed = speed; }
	void setFacingOnArrival(Real angle)
	{
		m_faceOnArrival = TRUE;
		m_facingOnArrival = angle;
		m_facingOrderFrame = TheGameLogic->getFrame();
	}
	void aiMoveToPosition(const Coord3D *, CommandSourceType source)
	{
		AICommandParms parms(AICMD_MOVE_TO_POSITION, source);
		aiDoCommand(&parms);
		exactAtMove = m_exactFormationMove && loco.ultra;
	}
	void aiFollowPath(std::vector<Coord3D> *, void *, CommandSourceType source)
	{
		aiMoveToPosition(NULL, source);
	}
	void aiDoCommand(const AICommandParms *);
	void cancelFacingMove();
	void releaseFormationSpeed();
	void setExactFormationMove();
	void moveToFormationSlot(const Coord3D &, Real, Real, UnsignedInt, Real, CommandSourceType, Bool);
};

struct GameWindow
{
	Int x, y, width, height;
	GameWindow *parent, *firstChild, *next;
	std::wstring label;
	GameWindow(Int px, Int py, Int w, Int h, GameWindow *p = NULL) :
		x(px), y(py), width(w), height(h), parent(p), firstChild(NULL), next(NULL)
	{
		if (parent)
		{
			next = parent->firstChild;
			parent->firstChild = this;
		}
	}
	GameWindow *winGetParent() { return parent; }
	GameWindow *winGetChild() { return firstChild; }
	GameWindow *winGetNext() { return next; }
	void winGetPosition(Int *px, Int *py) { *px = x; *py = y; }
	void winGetSize(Int *w, Int *h) { *w = width; *h = height; }
	void winSetPosition(Int px, Int py) { x = px; y = py; }
	void winSetSize(Int w, Int h) { width = w; height = h; }
	void winSetTooltip(const wchar_t *) {}
};
GameWindow *WinGameOptions = NULL;
GameWindow *checkSmartCommandGroup = NULL;
GameWindow *existingFacingDragCheck = NULL;
GameWindow *findOptionsWindow(const char *) { return existingFacingDragCheck; }
GameWindow *CreateCheckBoxLike(GameWindow *anchor, GameWindow *parent, Int x, Int y, const wchar_t *, const char *)
{
	existingFacingDragCheck = new GameWindow(x, y, anchor->width, anchor->height, parent);
	return existingFacingDragCheck;
}
void GadgetCheckBoxSetText(GameWindow *check, const wchar_t *label) { check->label = label; }
struct TestGameText
{
	const wchar_t *fetchOrSubstitute(const char *, const wchar_t *fallback) { return fallback; }
} testGameText;
TestGameText *TheGameText = &testGameText;
