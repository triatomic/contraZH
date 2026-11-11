/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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
#include "Common/FrameRateLimit.h"


#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

FrameRateLimit::FrameRateLimit()
{
	LARGE_INTEGER freq;
	LARGE_INTEGER start;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	m_freq = freq.QuadPart;
	m_start = start.QuadPart;
	m_last = start.QuadPart;

	// Fails before Windows 10 1803, which leaves wait on Sleep.
	m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
}

FrameRateLimit::~FrameRateLimit()
{
	if (m_timer != nullptr)
	{
		CloseHandle(m_timer);
	}
}

Real FrameRateLimit::wait(UnsignedInt maxFps)
{
	PROFILER_SECTION;
	LARGE_INTEGER tick;
	QueryPerformanceCounter(&tick);

	// The deadline always lies past the last return, so the returned time is never zero.
	const Int64 period = m_freq / maxFps;
	const Int64 deadline = std::max(m_start + period, m_last + 1);

	if (tick.QuadPart < deadline)
	{
		// Sleep until just short of the deadline. The timer can wake about 1 ms late, Sleep about 2 ms.
		if (m_timer != nullptr)
		{
			const Int64 sleepTicks = deadline - tick.QuadPart - m_freq / 1000;

			if (sleepTicks > 0)
			{
				LARGE_INTEGER due;
				due.QuadPart = -(sleepTicks * 10000000 / m_freq);

				if (SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE))
				{
					WaitForSingleObject(m_timer, INFINITE);
				}
			}
		}
		else
		{
			const Int64 sleepMilliseconds = (deadline - tick.QuadPart) * 1000 / m_freq - 2;

			if (sleepMilliseconds > 0)
			{
				Sleep(static_cast<DWORD>(sleepMilliseconds));
			}
		}

		// Busy wait for remaining time
		QueryPerformanceCounter(&tick);

		while (tick.QuadPart < deadline)
		{
			YieldProcessor();
			QueryPerformanceCounter(&tick);
		}
	}

	// A frame that ran up to half a period late shortens the next one, which holds the average rate on the limit.
	if (tick.QuadPart - deadline > period / 2)
	{
		m_start = tick.QuadPart;
	}
	else
	{
		m_start = deadline;
	}

	const double elapsedSeconds = static_cast<double>(tick.QuadPart - m_last) / m_freq;
	m_last = tick.QuadPart;
	return (Real)elapsedSeconds;
}

void FrameRateLimit::reset()
{
	LARGE_INTEGER tick;
	QueryPerformanceCounter(&tick);
	m_start = tick.QuadPart;
	m_last = tick.QuadPart;
}


#if defined(GENERALS_ONLINE_HIGH_FPS_SERVER)
const UnsignedInt RenderFpsPreset::s_fpsValues[] = {
	60, 65, 70, 72, 75, 80, 85, 90, 100, 110, 120, 144, 240, 480, UncappedFpsValue };

static_assert(LOGICFRAMES_PER_SECOND <= 60, "Min FPS values need to be revisited!");
#else
const UnsignedInt RenderFpsPreset::s_fpsValues[] = {
	30, 50, 56, 60, 65, 70, 72, 75, 80, 85, 90, 100, 110, 120, 144, 240, 480, UncappedFpsValue };

static_assert(LOGICFRAMES_PER_SECOND <= 30, "Min FPS values need to be revisited!");
#endif

UnsignedInt RenderFpsPreset::getNextFpsValue(UnsignedInt value)
{
	const Int first = 0;
	const Int last = ARRAY_SIZE(s_fpsValues) - 1;
	for (Int i = first; i < last; ++i)
	{
		if (value >= s_fpsValues[i] && value < s_fpsValues[i + 1])
		{
			return s_fpsValues[i + 1];
		}
	}
	return s_fpsValues[last];
}

UnsignedInt RenderFpsPreset::getPrevFpsValue(UnsignedInt value)
{
	const Int first = 0;
	const Int last = ARRAY_SIZE(s_fpsValues) - 1;
	for (Int i = last; i > first; --i)
	{
		if (value <= s_fpsValues[i] && value > s_fpsValues[i - 1])
		{
			return s_fpsValues[i - 1];
		}
	}
	return s_fpsValues[first];
}

UnsignedInt RenderFpsPreset::changeFpsValue(UnsignedInt value, FpsValueChange change)
{
	switch (change)
	{
	default:
	case FpsValueChange_Increase: return getNextFpsValue(value);
	case FpsValueChange_Decrease: return getPrevFpsValue(value);
	}
}


UnsignedInt LogicTimeScaleFpsPreset::getNextFpsValue(UnsignedInt value)
{
	return value + StepFpsValue;
}

UnsignedInt LogicTimeScaleFpsPreset::getPrevFpsValue(UnsignedInt value)
{
	if (value - StepFpsValue < MinFpsValue)
	{
		return MinFpsValue;
	}
	else
	{
		return value - StepFpsValue;
	}
}

UnsignedInt LogicTimeScaleFpsPreset::changeFpsValue(UnsignedInt value, FpsValueChange change)
{
	switch (change)
	{
	default:
	case FpsValueChange_Increase: return getNextFpsValue(value);
	case FpsValueChange_Decrease: return getPrevFpsValue(value);
	}
}
