#include "PreRTS.h"

#include "GameNetwork/GenOnline/GenOnlineForkMatchSettings.h"

#include "Common/FramePacer.h"
#include "Common/GlobalData.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/Online/IOnlineGameHooks.h"

class GenOnlineGame;
extern GenOnlineGame* TheGenOnlineGame;

static Bool s_saved = FALSE;
static Bool s_matchStarted = FALSE;
static Int s_pacerFpsLimit = 0;
static Bool s_useFpsLimit = FALSE;
static Int s_framesPerSecondLimit = 0;
static Real s_minCameraHeight = 0.0f;
static Real s_maxCameraHeight = 0.0f;

Bool GenOnlineForkIsOnlineMatch()
{
	return TheGenOnlineGame != nullptr && TheGameLogic != nullptr && TheGameLogic->isInInternetGame();
}

void GenOnlineForkSaveMatchSettings()
{
	if (s_saved || TheFramePacer == nullptr || TheWritableGlobalData == nullptr)
	{
		return;
	}

	s_pacerFpsLimit = TheFramePacer->getFramesPerSecondLimit();
	s_useFpsLimit = TheGlobalData->m_useFpsLimit;
	s_framesPerSecondLimit = TheGlobalData->m_framesPerSecondLimit;
	s_minCameraHeight = TheGlobalData->m_minCameraHeight;
	s_maxCameraHeight = TheGlobalData->m_maxCameraHeight;
	s_saved = TRUE;
}

static void restoreForkSettings()
{
	TheFramePacer->setFramesPerSecondLimit(s_pacerFpsLimit);
	TheWritableGlobalData->m_useFpsLimit = s_useFpsLimit;
	TheWritableGlobalData->m_framesPerSecondLimit = s_framesPerSecondLimit;
	TheWritableGlobalData->m_minCameraHeight = s_minCameraHeight;
	TheWritableGlobalData->m_maxCameraHeight = s_maxCameraHeight;
	s_saved = FALSE;
	s_matchStarted = FALSE;
}

void GenOnlineForkUpdateMatchSettings()
{
	if (TheFramePacer == nullptr || TheWritableGlobalData == nullptr)
	{
		return;
	}

	if (GenOnlineForkIsOnlineMatch())
	{
		GenOnlineForkSaveMatchSettings();
		s_matchStarted = TRUE;

		Int matchFpsLimit = 0;
		if (GetOnlineGameHooks()->getMatchFrameRateLimit(matchFpsLimit))
		{
			TheFramePacer->setFramesPerSecondLimit(matchFpsLimit);
			TheWritableGlobalData->m_useFpsLimit = matchFpsLimit != 0;
		}
	}
	// The launch saves before the match starts, so only an ended match or a closed lobby restores.
	else if (s_saved && (s_matchStarted || TheGenOnlineGame == nullptr))
	{
		restoreForkSettings();
	}
}
