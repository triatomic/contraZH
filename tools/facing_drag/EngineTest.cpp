namespace
{
void appendInput(GameMessage &head, GameMessage::Type type)
{
	GameMessage *tail = &head;
	while (tail->next())
	{
		tail = tail->next();
	}
	GameMessage *message = new GameMessage(type);
	tail->following = message;
	message->previous = tail;
}

void runInputBatch(FacingDragTranslator &drag, GameMessage &head)
{
	for (GameMessage *message = head.next(), *next; message; message = next)
	{
		const GameMessageDisposition disposition = drag.translateGameMessage(message);
		next = message->next();
		if (disposition == DESTROY_MESSAGE)
		{
			delete message;
		}
	}
	for (GameMessage *message = head.next(), *next; message; message = next)
	{
		next = message->next();
		if (message->getType() == GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN)
		{
			testSelection.onRawMouseLeftButtonDown(message);
		}
		else if (message->getType() == GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP)
		{
			testSelection.m_leftMouseButtonIsDown = FALSE;
		}
		else if (message->getType() == GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN && !testLook.m_rightMouseScrollBlocked)
		{
			testLook.m_scrollType = LookAtTranslator::SCROLL_RMB;
		}
		else if (message->getType() == GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP)
		{
			testLook.stopScrolling();
		}
		delete message;
	}
}
}

TEST(FacingDrag, SameBatchPressesAndReleasesDoNotLatchInput)
{
	for (Int rightFirst = 0; rightFirst < 2; ++rightFirst)
	{
		for (Int releaseRightFirst = 0; releaseRightFirst < 2; ++releaseRightFirst)
		{
			FacingDragTranslator drag;
			testSelection = SelectionTranslator();
			testLook = LookAtTranslator();
			GameMessage head(GameMessage::MSG_DO_MOVETO);
			appendInput(head, rightFirst ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN);
			appendInput(head, rightFirst ? GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN);
			appendInput(head, releaseRightFirst ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP);
			appendInput(head, releaseRightFirst ? GameMessage::MSG_MOUSE_RIGHT_CLICK : GameMessage::MSG_MOUSE_LEFT_CLICK);
			appendInput(head, releaseRightFirst ? GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP);
			appendInput(head, releaseRightFirst ? GameMessage::MSG_MOUSE_LEFT_CLICK : GameMessage::MSG_MOUSE_RIGHT_CLICK);
			runInputBatch(drag, head);
			EXPECT_EQ(drag.orders, 1);
			EXPECT_FALSE(testSelection.m_leftMouseButtonIsDown);
			EXPECT_EQ(testLook.m_scrollType, LookAtTranslator::SCROLL_NONE);
			EXPECT_FALSE(drag.m_blocking);
		}
	}
}

TEST(FacingDrag, APressAfterTheChordStillReachesSelection)
{
	FacingDragTranslator drag;
	testSelection = SelectionTranslator();
	testLook = LookAtTranslator();
	GameMessage head(GameMessage::MSG_DO_MOVETO);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN);
	runInputBatch(drag, head);
	EXPECT_TRUE(testSelection.m_leftMouseButtonIsDown);
}

TEST(FacingDrag, APressFromThePreviousBatchIsCancelled)
{
	FacingDragTranslator drag;
	testSelection = SelectionTranslator();
	testLook = LookAtTranslator();
	GameMessage head(GameMessage::MSG_DO_MOVETO);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN);
	runInputBatch(drag, head);
	EXPECT_TRUE(testSelection.m_leftMouseButtonIsDown);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP);
	appendInput(head, GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP);
	runInputBatch(drag, head);
	EXPECT_FALSE(testSelection.m_leftMouseButtonIsDown);
}

TEST(FacingMove, APlayerOrderCancelsTheFormationInTheSameFrame)
{
	for (Int command = AICMD_IDLE; command <= AICMD_MOVE_TO_POSITION; ++command)
	{
		AIUpdateInterface ai;
		testAI.finder.blocked = FALSE;
		const Coord3D slot = { 109.0f, 105.0f, 0.0f };
		ai.moveToFormationSlot(slot, 0.5f, 10.0f, 0, 3.0f, CMD_FROM_PLAYER, FALSE);
		EXPECT_TRUE(ai.exactAtMove);
		EXPECT_TRUE(ai.m_faceOnArrival);
		EXPECT_TRUE(ai.m_hasFormationSlot);
		EXPECT_FALSE(ai.m_issuingFormationMove);
		AICommandParms parms(command, CMD_FROM_PLAYER);
		ai.aiDoCommand(&parms);
		EXPECT_FALSE(ai.m_faceOnArrival);
		EXPECT_FALSE(ai.m_hasFormationSlot);
		EXPECT_FALSE(ai.m_exactFormationMove);
		EXPECT_FALSE(ai.loco.ultra);
		EXPECT_FLOAT_EQ(ai.m_formationSpeed, 0.0f);
		EXPECT_FLOAT_EQ(ai.desiredSpeed, FAST_AS_POSSIBLE);
	}
}

TEST(FacingMove, ABlockedSlotKeepsNormalPathfinding)
{
	AIUpdateInterface ai;
	testAI.finder.blocked = TRUE;
	const Coord3D slot = { 109.0f, 105.0f, 0.0f };
	ai.moveToFormationSlot(slot, 0.5f, 10.0f, 0, 0.0f, CMD_FROM_PLAYER, FALSE);
	EXPECT_FALSE(ai.exactAtMove);
	EXPECT_FALSE(ai.m_exactFormationMove);
	testAI.finder.blocked = FALSE;
}

TEST(FacingMove, AReplacementFormationClearsThePreviousExactMove)
{
	AIUpdateInterface ai;
	testAI.finder.blocked = FALSE;
	const Coord3D slot = { 109.0f, 105.0f, 0.0f };
	ai.moveToFormationSlot(slot, 0.5f, 10.0f, 0, 3.0f, CMD_FROM_PLAYER, FALSE);
	EXPECT_TRUE(ai.exactAtMove);
	testAI.finder.blocked = TRUE;
	ai.moveToFormationSlot(slot, 1.0f, 10.0f, 0, 0.0f, CMD_FROM_PLAYER, FALSE);
	EXPECT_FALSE(ai.exactAtMove);
	EXPECT_FLOAT_EQ(ai.m_formationSpeed, 0.0f);
	EXPECT_FLOAT_EQ(ai.desiredSpeed, FAST_AS_POSSIBLE);
	testAI.finder.blocked = FALSE;
}

TEST(FacingDrag, CheckboxFitsBelowTheInputControls)
{
	GameWindow panel(151, 68, 485, 469);
	GameWindow anchor(9, 385, 230, 20, &panel);
	GameWindow actionLabel(9, 406, 95, 20, &panel);
	GameWindow actionCombo(107, 406, 132, 20, &panel);
	GameWindow accept(144, 432, 159, 29, &panel);
	GameWindow back(315, 432, 159, 29, &panel);
	GameWindow radar(245, 352, 230, 20, &panel);
	WinGameOptions = &panel;
	checkSmartCommandGroup = &anchor;
	existingFacingDragCheck = NULL;
	GameWindow *check = createFacingDragCheck();
	EXPECT_TRUE(check != NULL);
	EXPECT_EQ(check->parent, &panel);
	EXPECT_EQ(check->label, std::wstring(L"Drag to Face"));
	EXPECT_GE(check->y, anchor.y + anchor.height);
	EXPECT_LE(check->y + check->height, actionLabel.y);
	EXPECT_EQ(actionLabel.y, actionCombo.y);
	EXPECT_LE(actionCombo.y + actionCombo.height, accept.y);
	EXPECT_LE(back.y + back.height, panel.height);
	EXPECT_EQ(panel.height, 490);
	EXPECT_EQ(radar.y, 352);
	EXPECT_EQ(createFacingDragCheck(), check);
	EXPECT_EQ(panel.height, 490);
	delete check;
	existingFacingDragCheck = NULL;
	WinGameOptions = NULL;
	checkSmartCommandGroup = NULL;
}
