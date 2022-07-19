/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

class Log
{
public:
	static const char* ColorRed;
	static const char* ColorYellow;
	static const char* ColorGreen;
	static const char* ColorNormal;

	static void DisableColoredOutput();
};

#define PRINT(x) std::cout << "[ PRINT @ " << __func__ << ":" << __LINE__ << " ] " << x << std::endl

#define ERR(x)                                                        \
	std::cerr << Log::ColorRed                                        \
	          << "[ ERROR @ " << __func__ << ":" << __LINE__ << " ] " \
	          << x << Log::ColorNormal << std::endl

#define WARN(x) std::cerr << Log::ColorYellow                                       \
	                      << "[ WARNING @ " << __func__ << ":" << __LINE__ << " ] " \
	                      << x << Log::ColorNormal << std::endl

#define SUCCESS(x) std::cerr << Log::ColorGreen                                        \
	                         << "[ SUCCESS @ " << __func__ << ":" << __LINE__ << " ] " \
	                         << x << Log::ColorNormal << std::endl
