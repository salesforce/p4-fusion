/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "log.h"

#define COLOR_RED "\033[91m"
#define COLOR_YELLOW "\033[93m"
#define COLOR_GREEN "\033[32m"
#define COLOR_NORMAL "\033[0m"

const char* Log::ColorRed = COLOR_RED;
const char* Log::ColorYellow = COLOR_YELLOW;
const char* Log::ColorGreen = COLOR_GREEN;
const char* Log::ColorNormal = COLOR_NORMAL;
std::mutex Log::mutex;

void Log::DisableColoredOutput()
{
	ColorRed = "";
	ColorYellow = "";
	ColorGreen = "";
	ColorNormal = "";
}
