/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <chrono>

typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

class Timer
{
	TimePoint m_StartTime;

public:
	static TimePoint Now() { return std::chrono::high_resolution_clock::now(); }

	Timer();

	/// Get time spent since construction of this object in seconds
	[[nodiscard]] float GetTimeS() const { return (float)(std::chrono::high_resolution_clock::now() - m_StartTime).count() * 1e-9; }
};
