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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

/** FrameMetrics.cpp */

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <numeric>

#include "GameNetwork/FrameMetrics.h"
#include "GameClient/Display.h"
#include "GameNetwork/networkutil.h"

FrameMetrics::FrameMetrics()
{
	m_averageFps = 0.0f;
	m_averageLatency = 0.0f;
	m_latencySampleCount = 0;
	m_latencyDeviation = 0.0f;
	m_cushionIndex = 0;
	m_fpsListIndex = 0;
	m_lastFpsTimeThing = 0;
	m_minimumCushion = 0;

#if defined(USE_NEW_FRAMEMETRIC_LOGIC)
	oldestLatencyInMap = -1;
	oldestPendingLatencyInMap = -1;
	m_mapLatenciesLookup.clear();
	m_mapLatenciesSorted.clear();
	m_mapPendingLatenciesLookup.clear();
	m_mapPendingLatenciesSorted.clear();
#else
	m_pendingLatencies = NEW time_t[MAX_FRAMES_AHEAD];
	for(Int i = 0; i < MAX_FRAMES_AHEAD; i++)
		m_pendingLatencies[i] = 0;
	m_latencyList = NEW Real[TheGlobalData->m_networkLatencyHistoryLength];
#endif
	m_fpsList = NEW Real[TheGlobalData->m_networkFPSHistoryLength];
}

FrameMetrics::~FrameMetrics() {
	delete m_fpsList;
	m_fpsList = nullptr;

#if defined(USE_NEW_FRAMEMETRIC_LOGIC)
	m_mapLatenciesLookup.clear();
	m_mapLatenciesSorted.clear();
	m_mapPendingLatenciesLookup.clear();
	m_mapPendingLatenciesSorted.clear();
#else
	delete m_latencyList;
	m_latencyList = nullptr;

	delete[] m_pendingLatencies;
	m_pendingLatencies = nullptr;
#endif
}

void FrameMetrics::init() {
	m_averageFps = 30;
	m_averageLatency = (Real)0.2;
	m_minimumCushion = -1;

	UnsignedInt i = 0;
	for (; i < TheGlobalData->m_networkFPSHistoryLength; ++i) {
		m_fpsList[i] = 30.0;
	}
	m_fpsListIndex = 0;
	// The history holds measured round trips only; the average above stands in until the first one arrives
	m_latencySampleCount = 0;
	m_latencyDeviation = 0.0f;
#if !defined(USE_NEW_FRAMEMETRIC_LOGIC)
	for (i = 0; i < TheGlobalData->m_networkLatencyHistoryLength; ++i) {
		m_latencyList[i] = 0.0f;
	}
#endif
	m_cushionIndex = 0;
}

void FrameMetrics::reset() {
	init();
}

void FrameMetrics::doPerFrameMetrics(UnsignedInt frame) {
	// Do the measurement of the fps.
	time_t curTime = timeGetTime();
	if ((curTime - m_lastFpsTimeThing) >= 1000) {
//		if ((m_fpsListIndex % 16) == 0) {
//			DEBUG_LOG(("FrameMetrics::doPerFrameMetrics - adding %f to fps history. average before: %f ", m_fpsList[m_fpsListIndex], m_averageFps));
//		}
		m_averageFps -= ((m_fpsList[m_fpsListIndex])) / TheGlobalData->m_networkFPSHistoryLength; // subtract out the old value from the average.
		m_fpsList[m_fpsListIndex] = TheDisplay->getAverageFPS();
//		m_fpsList[m_fpsListIndex] = TheGameClient->getFrame() - m_fpsStartingFrame;
		m_averageFps += ((Real)(m_fpsList[m_fpsListIndex])) / TheGlobalData->m_networkFPSHistoryLength; // add the new value to the average.
//		DEBUG_LOG(("average after: %f", m_averageFps));
		++m_fpsListIndex;
		m_fpsListIndex %= TheGlobalData->m_networkFPSHistoryLength;
		m_lastFpsTimeThing = curTime;
	}

#if defined(USE_NEW_FRAMEMETRIC_LOGIC)
	m_mapPendingLatenciesSorted[frame] = curTime;
	m_mapPendingLatenciesLookup[frame] = curTime;

	// initialize
	if (oldestPendingLatencyInMap == -1)
	{
		oldestPendingLatencyInMap = frame;
	}

	// beyond size limit? remove the oldest
	if (m_mapPendingLatenciesSorted.size() > TheGlobalData->m_networkLatencyHistoryLength)
	{
		// remove oldest frame
		UnsignedInt oldestFrame = m_mapPendingLatenciesSorted.begin()->first;
		m_mapPendingLatenciesSorted.erase(m_mapPendingLatenciesSorted.begin());
		m_mapPendingLatenciesLookup.erase(oldestFrame);
	}
#else
	Int pendingLatenciesIndex = frame % MAX_FRAMES_AHEAD;
	m_pendingLatencies[pendingLatenciesIndex] = curTime;
#endif

}

void FrameMetrics::processLatencyResponse(UnsignedInt frame) {
#if defined(USE_NEW_FRAMEMETRIC_LOGIC)
	if (!m_mapPendingLatenciesLookup.contains(frame))
	{
		NetworkLog(ELogVerbosity::LOG_DEBUG, "WARNING: Frame hasn't been requested yet, frame %ld", frame);
		return;
	}
#endif

	time_t curTime = timeGetTime();
	if (m_latencySampleCount < TheGlobalData->m_networkLatencyHistoryLength)
	{
		++m_latencySampleCount;
	}
#if defined(USE_NEW_FRAMEMETRIC_LOGIC)
	time_t timeDiff = curTime - m_mapPendingLatenciesLookup[frame];

	// initialize
	if (oldestLatencyInMap == -1)
	{
		oldestLatencyInMap = frame;
	}

	// beyond size limit? remove the oldest
	if (m_mapLatenciesLookup.size() > TheGlobalData->m_networkLatencyHistoryLength)
	{
		// remove oldest frame
		UnsignedInt oldestFrame = m_mapLatenciesSorted.begin()->first;
		m_mapLatenciesSorted.erase(m_mapLatenciesSorted.begin());
		m_mapLatenciesLookup.erase(oldestFrame);
	}

	m_mapLatenciesLookup[frame] = (Real)timeDiff / (Real)1000; // convert to seconds from milliseconds.
	m_mapLatenciesSorted[frame] = (Real)timeDiff / (Real)1000; // convert to seconds from milliseconds.

	// calculate average
	m_averageLatency = 0.0f;
	for (auto kvPair : m_mapLatenciesLookup)
	{
		m_averageLatency += kvPair.second;
	}
	m_averageLatency /= m_mapLatenciesLookup.size();

	Real variance = 0.0f;
	for (auto kvPair : m_mapLatenciesLookup)
	{
		const Real deviation = kvPair.second - m_averageLatency;
		variance += deviation * deviation;
	}
	m_latencyDeviation = sqrtf(variance / m_mapLatenciesLookup.size());

	if (timeDiff > 1000)
	{
		NetworkLog(ELogVerbosity::LOG_DEBUG, "WARNING: HIGH processLatencyResponse");
	}
#else
	Int pendingIndex = frame % MAX_FRAMES_AHEAD;
	time_t timeDiff = curTime - m_pendingLatencies[pendingIndex];

	Int latencyListIndex = frame % TheGlobalData->m_networkLatencyHistoryLength;
	m_latencyList[latencyListIndex] = (Real)timeDiff / (Real)1000; // convert to seconds from milliseconds.
	const Real latencySum = std::accumulate(m_latencyList, m_latencyList + TheGlobalData->m_networkLatencyHistoryLength, 0.0f);
	m_averageLatency = latencySum / (Real)m_latencySampleCount;
#endif

	if (frame % 16 == 0) {
//		DEBUG_LOG(("ConnectionManager::processFrameInfoAck - average latency = %f", m_averageLatency));
	}
}

void FrameMetrics::addCushion(Int cushion) {
	++m_cushionIndex;
	m_cushionIndex %= TheGlobalData->m_networkCushionHistoryLength;
	if (m_cushionIndex == 0) {
		m_minimumCushion = -1;
	}
	if ((cushion < m_minimumCushion) || (m_minimumCushion == -1)) {
		m_minimumCushion = cushion;
	}
}

Int FrameMetrics::getAverageFPS() {
	return (Int)m_averageFps;
}

Real FrameMetrics::getAverageLatency() {
	return m_averageLatency;
}

// Two standard deviations above the mean covers about 95% of round trips, so a jittery link does not stall on every swing
Real FrameMetrics::getRunAheadLatency()
{
	return m_averageLatency + 2.0f * m_latencyDeviation;
}

Int FrameMetrics::getMinimumCushion() {
	return m_minimumCushion;
}

