/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "timer.h"

const std::chrono::high_resolution_clock Timer::s_Clock;

Timer::Timer()
    : m_StartTime(Now())
{
}
