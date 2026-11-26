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
#include "PreRTS.h"

#include "Common/FramePacer.h"

#include "GameClient/View.h"

#if defined(GENONLINE)
#include "Common/GameEngine.h"

#endif
#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

#include "GameNetwork/NetworkDefs.h"
#include "GameNetwork/NetworkInterface.h"

#include "WW3D2/ww3d.h"


FramePacer* TheFramePacer = nullptr;

FramePacer::FramePacer()
{
	// Set the time slice size to 1 ms.
	timeBeginPeriod(1);

	m_maxFPS = BaseFps;
#if RTS_ZEROHOUR
	m_renderFpsLimit = -1;
#endif
	m_logicTimeScaleFPS = LOGICFRAMES_PER_SECOND;
	m_defaultGameSpeed = BaseFps;
	m_updateTime = 1.0f / (Real)BaseFps; // initialized to something to avoid division by zero on first use
	m_logicFramePhase = 1.0f;
	m_enableFpsLimit = FALSE;
	m_enableLogicTimeScale = FALSE;
	m_isTimeFrozen = FALSE;
	m_isGameHalted = FALSE;
#if defined(GENONLINE)
	m_gameSpeedFps = LOGICFRAMES_PER_SECOND;
	m_savedLogicTimeScaleFPS = LOGICFRAMES_PER_SECOND;
	m_savedEnableLogicTimeScale = FALSE;
	m_isGameSpeedActive = FALSE;
#endif
}

FramePacer::~FramePacer()
{
	// Restore the previous time slice for Windows.
	timeEndPeriod(1);
}

void FramePacer::update()
{
	// Uses a high resolution counter to cap the frame rate more accurately to the desired limit than retail did.
	UnsignedInt maxFps = getActualFramesPerSecondLimit();

	// Vertical sync paces a limit at or above the refresh rate. The limiter then only catches a Present that does not wait.
	const UnsignedInt refreshRate = WW3D::Get_VSync_Refresh_Rate();

	if (refreshRate != 0 && maxFps >= refreshRate)
	{
		maxFps = max(maxFps, refreshRate + refreshRate / 50);
	}

	m_updateTime = m_frameRateLimit.wait(maxFps);

	if (TheGameLogic != nullptr)
	{
		// Set or advance the logic frame phase by the render step that the next update will draw.
		// It is capped at a whole logic frame, because the render steps in between can add up to more than one.
		// Consumers are expected to interpolate towards the next logic frame and not extrapolate past it.
		const Real timeScale = getActualLogicTimeScaleOverFpsRatio();

		if (TheGameLogic->hasUpdated())
		{
			m_logicFramePhase = timeScale;
		}
		else
		{
			m_logicFramePhase = min(1.0f, m_logicFramePhase + timeScale);
		}
	}
}

void FramePacer::reset()
{
	m_frameRateLimit.reset();
	m_updateTime = 1.0f / (Real)getActualFramesPerSecondLimit();
	m_logicFramePhase = 1.0f;
}

void FramePacer::setFramesPerSecondLimit( Int fps )
{
	DEBUG_LOG(("FramePacer::setFramesPerSecondLimit() - setting max fps to %d (TheGlobalData->m_useFpsLimit == %d)", fps, TheGlobalData->m_useFpsLimit));
#if defined(GENONLINE)

	// clamp FPS to at least render at same as logic
#if defined(GENONLINE)
	fps = std::max<int>(fps, LOGICFRAMES_PER_SECOND);
#endif

#endif
	m_maxFPS = fps;
}

Int FramePacer::getFramesPerSecondLimit()  const
{
	return m_maxFPS;
}

#if RTS_ZEROHOUR
void FramePacer::setRenderFPSLimit(Int fps)
{
	m_renderFpsLimit = fps;
}
#endif

void FramePacer::enableFramesPerSecondLimit( Bool enable )
{
	m_enableFpsLimit = enable;
}

Bool FramePacer::isFramesPerSecondLimitEnabled() const
{
	return m_enableFpsLimit;
}

Bool FramePacer::isFastForwardAvailable() const
{
	if (TheGameLogic == nullptr)
	{
		return FALSE;
	}
#if RTS_ZEROHOUR && (defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE))
	if (!TheGameLogic->isInMultiplayerGame())
	{
		return TRUE;
	}
#endif
	return TheGameLogic->isInReplayGame();
}

Bool FramePacer::isActualFramesPerSecondLimitEnabled() const
{
	Bool allowFpsLimit = true;

	if (TheTacticalView != nullptr)
	{
		allowFpsLimit &= TheTacticalView->getTimeMultiplier()<=1 && !TheScriptEngine->isTimeFast();
	}

	if (TheGameLogic != nullptr)
	{
		allowFpsLimit &= !(!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode && isFastForwardAvailable());
	}

#if RTS_ZEROHOUR
	allowFpsLimit &= m_renderFpsLimit >= 0 ? m_renderFpsLimit != 0 : TheGlobalData->m_useFpsLimit;
#else
	allowFpsLimit &= TheGlobalData->m_useFpsLimit;
#endif
	allowFpsLimit &= isFramesPerSecondLimitEnabled();

	return allowFpsLimit;
}

Int FramePacer::getActualFramesPerSecondLimit() const
{
#if defined(GENONLINE)
#if RTS_ZEROHOUR
	if (isActualFramesPerSecondLimitEnabled() && m_renderFpsLimit > 0)
	{
		Int logicFps = LOGICFRAMES_PER_SECOND;
		if (isLogicTimeScaleEnabled() || TheNetwork != nullptr)
		{
			logicFps = getActualLogicTimeScaleFps();
		}
		return getSituationCappedFramesPerSecondLimit(max(m_renderFpsLimit, logicFps));
	}
#endif
	const Int fps = isActualFramesPerSecondLimitEnabled() ? getFramesPerSecondLimit() : RenderFpsPreset::UncappedFpsValue;
	return getSituationCappedFramesPerSecondLimit(fps);
#else
#if RTS_ZEROHOUR
	if (isActualFramesPerSecondLimitEnabled() && m_renderFpsLimit > 0)
	{
		Int logicFps = LOGICFRAMES_PER_SECOND;
		if (isLogicTimeScaleEnabled() || TheNetwork != nullptr)
		{
			logicFps = getActualLogicTimeScaleFps();
		}
		return max(m_renderFpsLimit, logicFps);
	}
#endif
	return isActualFramesPerSecondLimitEnabled() ? getFramesPerSecondLimit() : RenderFpsPreset::UncappedFpsValue;
#endif
}

#if defined(GENONLINE)
#if defined(GENONLINE)
// the most the menus and the shell map render at
static const Int MENU_FPS_LIMIT = 60;

Int FramePacer::getSituationCappedFramesPerSecondLimit(Int fps) const
{
	// A match renders at the chosen limit, uncapped included, the menus and the shell map gain nothing from more than 60.
	const Bool inMatch = TheGameLogic != nullptr && TheGameLogic->isInGame() && TheGameLogic->getGameMode() != GAME_SHELL;
	if (!inMatch)
	{
		fps = std::min<Int>(fps, MENU_FPS_LIMIT);
	}

	// Without focus nobody watches the match at a high frame rate.
	if (TheGameEngine != nullptr && !TheGameEngine->isActive())
	{
		fps = std::min<Int>(fps, LOGICFRAMES_PER_SECOND);
	}

	// Logic is capped by the render frame rate, so never render slower than the logic time scale or the network rate.
	if (TheNetwork != nullptr || isLogicTimeScaleEnabled())
	{
		fps = std::max<Int>(fps, getActualLogicTimeScaleFps(IgnoreFrozenTime | IgnoreHaltedGame));
	}
	return fps;
}
#endif

#endif
Real FramePacer::getUpdateTime()  const
{
	return m_updateTime;
}

Real FramePacer::getUpdateFps()  const
{
	return 1.0f / m_updateTime;
}

Real FramePacer::getBaseOverUpdateFpsRatio(Real minUpdateFps)
{
	// Update fps is floored to default 5 fps, 200 ms.
	// Useful to prevent insane ratios on frame spikes/stalls.
	return (Real)BaseFps / std::max(getUpdateFps(), minUpdateFps);
}

void FramePacer::setTimeFrozen(Bool frozen)
{
	m_isTimeFrozen = frozen;
}

void FramePacer::setGameHalted(Bool halted)
{
	m_isGameHalted = halted;
}

Bool FramePacer::isTimeFrozen() const
{
	return m_isTimeFrozen;
}

Bool FramePacer::isGameHalted() const
{
	return m_isGameHalted;
}

void FramePacer::setLogicTimeScaleFps( Int fps )
{
	m_logicTimeScaleFPS = fps;
}

Int FramePacer::getLogicTimeScaleFps() const
{
	return m_logicTimeScaleFPS;
}

void FramePacer::enableLogicTimeScale( Bool enable )
{
	m_enableLogicTimeScale = enable;
}

Bool FramePacer::isLogicTimeScaleEnabled() const
{
	return m_enableLogicTimeScale;
}

#if defined(GENONLINE)
#if defined(GENONLINE)
void FramePacer::setGameSpeedFps( Int fps )
{
	m_gameSpeedFps = fps;
}

void FramePacer::resetGameSpeed()
{
	m_gameSpeedFps = LOGICFRAMES_PER_SECOND;
}

void FramePacer::beginGameSpeed()
{
	endGameSpeed();

	// The normal game speed leaves the logic time scale of the player alone.
	if (m_gameSpeedFps == LOGICFRAMES_PER_SECOND)
	{
		return;
	}

	m_savedLogicTimeScaleFPS = m_logicTimeScaleFPS;
	m_savedEnableLogicTimeScale = m_enableLogicTimeScale;
	m_isGameSpeedActive = TRUE;

	// The render limit rises with the logic time scale, so the game speed applies without touching it.
	m_logicTimeScaleFPS = m_gameSpeedFps >= 1000 ? (Int)RenderFpsPreset::UncappedFpsValue : std::max<Int>(m_gameSpeedFps, 1);
	m_enableLogicTimeScale = TRUE;
}

void FramePacer::endGameSpeed()
{
	if (!m_isGameSpeedActive)
	{
		return;
	}

	m_logicTimeScaleFPS = m_savedLogicTimeScaleFPS;
	m_enableLogicTimeScale = m_savedEnableLogicTimeScale;
	m_isGameSpeedActive = FALSE;
}
#endif

#endif
Int FramePacer::getActualLogicTimeScaleFps(LogicTimeQueryFlags flags) const
{
	if (m_isTimeFrozen && (flags & IgnoreFrozenTime) == 0)
	{
		return 0;
	}

	if (m_isGameHalted && (flags & IgnoreHaltedGame) == 0)
	{
		return 0;
	}

	if (TheNetwork != nullptr)
	{
		return TheNetwork->getFrameRate();
	}

	// Scripted fast time uncaps the render and expects the logic to follow it.
	const Bool timeFast = TheTacticalView != nullptr && (TheTacticalView->getTimeMultiplier() > 1 || TheScriptEngine->isTimeFast());

	if (isLogicTimeScaleEnabled() && !timeFast)
	{
		return getLogicTimeScaleFps();
	}

	// Returns uncapped value to align with the render update as per the original game behavior.
	return RenderFpsPreset::UncappedFpsValue;
}

Real FramePacer::getActualLogicTimeScaleRatio(LogicTimeQueryFlags flags) const
{
	return (Real)getActualLogicTimeScaleFps(flags) / LOGICFRAMES_PER_SECONDS_REAL;
}

Real FramePacer::getActualLogicTimeScaleOverFpsRatio(LogicTimeQueryFlags flags) const
{
	// Clamps ratio to min 1, because the logic frame rate is currently capped by the render frame rate.
	return min(1.0f, (Real)getActualLogicTimeScaleFps(flags) / getUpdateFps());
}

Real FramePacer::getLogicTimeStepSeconds(LogicTimeQueryFlags flags) const
{
	return SECONDS_PER_LOGICFRAME_REAL * getActualLogicTimeScaleOverFpsRatio(flags);
}

Real FramePacer::getLogicTimeStepMilliseconds(LogicTimeQueryFlags flags) const
{
	return MSEC_PER_LOGICFRAME_REAL * getActualLogicTimeScaleOverFpsRatio(flags);
}

Real FramePacer::getLogicFramePhase() const
{
	return m_logicFramePhase;
}

void FramePacer::setDefaultGameSpeed( Int speed )
{
	m_defaultGameSpeed = speed;
	setGameSpeed(speed);
}

void FramePacer::setGameSpeed( Int speed )
{
	if (speed <= 0)
	{
		speed = m_defaultGameSpeed;
	}

	enableLogicTimeScale(TRUE);
	setLogicTimeScaleFps(speed * LOGICFRAMES_PER_SECOND / BaseFps);
	setFramesPerSecondLimit(max(speed, TheGlobalData->m_framesPerSecondLimit));
}

Int FramePacer::getGameSpeed() const
{
	if (isLogicTimeScaleEnabled())
	{
		return getLogicTimeScaleFps() * BaseFps / LOGICFRAMES_PER_SECOND;
	}
	return getFramesPerSecondLimit();
}
