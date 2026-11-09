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

// Timestamps for the STARTUP lines of the debug log.

#pragma once

#include "Common/Debug.h"

// Milliseconds since the process started, so the STARTUP lines also cover static initialization.
inline double getStartupElapsedMs()
{
	static Bool s_started = FALSE;
	static double s_baseMs = 0.0;
	static LARGE_INTEGER s_start;
	static LARGE_INTEGER s_frequency;

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	if (!s_started)
	{
		s_started = TRUE;
		s_start = now;
		QueryPerformanceFrequency(&s_frequency);

		FILETIME creationTime, exitTime, kernelTime, userTime, currentTime;
		if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
		{
			GetSystemTimeAsFileTime(&currentTime);
			ULARGE_INTEGER created;
			ULARGE_INTEGER current;
			created.LowPart = creationTime.dwLowDateTime;
			created.HighPart = creationTime.dwHighDateTime;
			current.LowPart = currentTime.dwLowDateTime;
			current.HighPart = currentTime.dwHighDateTime;
			s_baseMs = (double)(current.QuadPart - created.QuadPart) / 10000.0;
		}
	}

	return s_baseMs + (double)(now.QuadPart - s_start.QuadPart) * 1000.0 / (double)s_frequency.QuadPart;
}

// Logs how long each step of a long function took, measured from the previous mark.
class StartupStepClock
{
public:
	explicit StartupStepClock(const char* owner)
		: m_owner(owner)
		, m_lastMs(getStartupElapsedMs())
	{
	}

	void mark(const char* step)
	{
		const double nowMs = getStartupElapsedMs();
		DEBUG_LOG(("STARTUP: [%8.1f ms]   %s %s in %.1f ms", nowMs, m_owner, step, nowMs - m_lastMs));
		m_lastMs = nowMs;
	}

private:
	const char* m_owner;
	double m_lastMs;
};
