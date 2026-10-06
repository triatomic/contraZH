#pragma once

#include "Common/GameType.h"
#include "Common/MessageStream.h"
#include <map>

class Object;
class Xfer;

//=============================================================================
// CommandSequence - the advanced waypoint system.
//
// Designed to triatomic/contraZH issue #122 "Define new waypoint system".
// The idea is that a unit's queued intentions are not just MovementGoals in the
// AI's path, but an explicit sequence of GUI-level commands: build here, then
// move there, then attack that, then guard this. The sequence is the single
// source of truth, and the AI path is merely how the *current* node is reached.
//
// Scope notes (from the issue):
//   * Only the command messages on the whitelist below may enter a sequence.
//     Group-shaped commands such as scatter are deliberately excluded: several
//     units sharing a command tree do not start that command on the same frame,
//     so "scatter" has no well defined meaning per unit.
//   * A command that never terminates (guard, force attack ground, internet
//     hack, a weapon fired with infinite shots, ...) is an *end command*: it
//     closes the sequence and nothing may be appended after it.
//   * Formation mode and cyclic sequences (patrol) are intentionally not
//     supported for now.
//
// LOCKSTEP DISCIPLINE: everything the logic side decides here (appending,
// advancing, dropping a node whose target became untraceable) must be driven by
// broadcast GameMessages and by logic-side state only. The client may keep a
// local *pending* sequence for preview, but it must never mutate an already
// committed one.
//=============================================================================

#define COMMAND_SEQUENCE_MAX_NODES_PER_SUBJECT	64

/// How many units may share one commit message. A group order names every unit it
/// applies to, and they all travel with a single copy of the chain.
#define COMMAND_SEQUENCE_MAX_SUBJECTS_PER_MESSAGE	128

//-----------------------------------------------------------------------------
// One command in a sequence.
//-----------------------------------------------------------------------------
class CommandNode
{
public:

	CommandNode();
	~CommandNode();

	GameMessage::Type	getCommandType() const		{ return m_cmdType; }
	ObjectID					getTargetID() const			{ return m_targetID; }
	const Coord3D	   *getLocation() const			{ return &m_location; }
	Int								getCommandParam() const	{ return m_param; }
	Real							getAngle() const				{ return m_angle; }

	CommandNode	     *getNext() const					{ return m_next; }
	CommandNode	     *getFirstChild() const		{ return m_firstChild; }

private:

	// only CommandSequence builds and owns nodes
	friend class CommandSequence;

	GameMessage::Type	m_cmdType;			///< whitelisted GUI command message
	ObjectID					m_targetID;			///< target object, for the *_AT_OBJECT commands
	Coord3D						m_location;			///< target position, for the location commands
	Int								m_param;				///< weapon slot, special power id, template key, ...
	Real							m_angle;				///< placement angle, for the dozer build commands
	Bool							m_immediate;		///< a state toggle: done the moment it is dispatched

	CommandNode	     *m_next;
	CommandNode	     *m_firstChild;	///< reserved for the command tree (not used yet)
};

//-----------------------------------------------------------------------------
// The ordered commands belonging to one unit.
//
// m_pendingHead is the client-side "I am still plotting this" list: it is what
// the preview draws and what the player is still free to edit. m_activeHead is
// the committed list the logic executes, one node at a time.
//-----------------------------------------------------------------------------
class CommandSequence
{
public:

	CommandSequence( ObjectID subject );
	~CommandSequence();

	ObjectID getSubject() const { return m_subject; }

	/// A priority-target list is what an immobile defence gets instead of an errand queue:
	/// it never advances and nothing is dispatched from it, it is simply a standing,
	/// ordered "kill these first" list that the unit's target scan consults (issue R7).
	Bool isPriorityTargetList() const { return m_priorityTargetList; }
	void setPriorityTargetList( Bool setting ) { m_priorityTargetList = setting; }

	// --- pending (plotted, not committed) ------------------------------------
	//
	// Returns FALSE when the command cannot be appended: it is not whitelisted,
	// the sequence already ends in an end command, or the cap was reached.
	Bool appendPending( GameMessage::Type type, ObjectID targetID, const Coord3D *pos, Int param, Real angle = 0.0f );
	Int  getPendingCount() const { return m_pendingCount; }
	CommandNode *getPendingHead() const { return m_pendingHead; }
	void clearPending();

	/// TRUE when every node of the active chain has been consumed -- the system then
	/// destroys the sequence, so isExecutingSequence() does not stay stuck on TRUE.
	Bool isFinished() const { return m_current == nullptr; }
	/// The node currently being executed (nullptr when idle/finished); the executing
	/// route preview draws from here to the end of the active chain.
	CommandNode *getCurrentNode() const { return m_current; }

	// --- commit -------------------------------------------------------------
	//
	// Moves the plotted chain over to the active chain. P3 turns this into the
	// point where the sequence is broadcast; today it is a local hand-over.
	Bool commit();

	// --- active (committed, executing) --------------------------------------
	Int  getActiveCount() const { return m_activeCount; }
	CommandNode *getActiveHead() const { return m_activeHead; }
	CommandNode *getCurrent() const { return m_current; }

	void clear();

	// --- execution (logic side) ---------------------------------------------
	//
	// Hands the current node to the subject's AI and advances once that command
	// has finished. Only the logic side calls this, and only for committed
	// sequences -- the client's plotted chain never runs through here.
	void update( Object *subject );

	// --- target loss (issue R6) ---------------------------------------------
	//
	// A node whose target is gone must not stay queued: it would chase nothing, and
	// keeping it around would leak the position of something the player is no longer
	// entitled to see (an enemy that walked into fog, for instance).
	// A target that transformed instead of dying keeps its orders -- see the .cpp.
	void onTargetInvalid( ObjectID targetID, Bool wasKilled, const Coord3D *lastKnownPos );

private:

	static CommandNode *removeTargetFromChain( CommandNode *head, ObjectID targetID,
																	 Int *countOut, CommandNode **tailOut );
	Bool dispatchCurrent( Object *subject );	///< FALSE when the node cannot run and should be skipped

	CommandNode *buildNode( GameMessage::Type type, ObjectID targetID, const Coord3D *pos, Int param, Real angle ) const;
	static void destroyChain( CommandNode *head );

	ObjectID				m_subject;

	CommandNode	   *m_pendingHead;
	CommandNode	   *m_pendingTail;
	Int						m_pendingCount;

	CommandNode	   *m_activeHead;
	CommandNode	   *m_activeTail;
	Int						m_activeCount;
	CommandNode	   *m_current;
	Bool					m_currentDispatched;
	Bool					m_priorityTargetList;	///< the current node has been handed to the AI already
	ObjectID				m_activeBuildTargetID;	///< foundation created by the current build node (P4 completion)
};

//-----------------------------------------------------------------------------
// TheCommandSequence - single instance sub-system holding every sequence.
//-----------------------------------------------------------------------------
class CommandSequenceSystem
{
public:

	CommandSequenceSystem();
	~CommandSequenceSystem();

	void init();
	void reset();
	void update();											///< advances the sequences (P3)

	CommandSequence *createSequence( ObjectID subject );
	CommandSequence *getSequence( ObjectID subject ) const;
	void destroySequence( ObjectID subject );

	/// Convenience wrapper: find (or create) the subject's sequence and append.
	Bool appendCommand( ObjectID subject, GameMessage::Type type, ObjectID targetID = INVALID_ID,
											const Coord3D *pos = nullptr, Int param = 0 );

	/// Applies a sequence committed by a player (MSG_COMMAND_SEQUENCE_COMMIT).
	Bool onCommitMessage( const GameMessage *msg );

	/// A target stopped being usable (issue R6): dropped if it died, re-anchored to
	/// lastKnownPos if it transformed into something else.
	void onTargetInvalid( ObjectID targetID, Bool wasKilled, const Coord3D *lastKnownPos );

	/// The priority target a unit should engage right now, or null when nothing on its
	/// ordered list is reachable -- in which case the caller falls back to normal
	/// target acquisition (issue R7).
	Object *getPriorityTarget( Object *unit ) const;

	/// TRUE while the subject still has committed commands left to run. Smart cast uses it
	/// to avoid pulling a unit away from the route the player is steering it along.
	Bool isExecutingSequence( ObjectID subject ) const;

	/// Player orders that REPLACE a plotted sequence instead of joining it: a unit handed
	/// a fresh move/attack/build order must not keep firing the rest of an old route later.
	static Bool isCancellingPlayerOrder( GameMessage::Type type );

	/// Contributes the queued sequences to the frame CRC. Without this a divergence in
	/// the queues would go unnoticed until the units behaved differently.
	void crc( Xfer *xfer );

	Int getSequenceCount() const;

	// --- command classification --------------------------------------------
	static Bool isAllowedCommand( GameMessage::Type type );

	/// TRUE for commands that are a state flip rather than an errand: they complete the
	/// instant they are dispatched, so the sequence must not wait for the unit to go idle.
	static Bool isImmediateCommand( GameMessage::Type type );

private:

	std::map< ObjectID, CommandSequence * > m_sequences;
};

extern CommandSequenceSystem *TheCommandSequence;
