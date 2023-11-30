/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <mutex>
#include <iostream>

class Log
{
public:
	static const char* ColorRed;
	static const char* ColorYellow;
	static const char* ColorGreen;
	static const char* ColorNormal;
	static std::mutex mutex;

	static void DisableColoredOutput();
};

#define PRINT(x)                                                                             \
	{                                                                                        \
		std::lock_guard<std::mutex> lk(Log::mutex);                                          \
		std::cout << "[ PRINT @ " << __func__ << ":" << __LINE__ << " ] " << x << std::endl; \
	}

#define ERR(x)                                                            \
	{                                                                     \
		std::lock_guard<std::mutex> lk(Log::mutex);                       \
		std::cerr << Log::ColorRed                                        \
		          << "[ ERROR @ " << __func__ << ":" << __LINE__ << " ] " \
		          << x << Log::ColorNormal << std::endl;                  \
	}

#define WARN(x)                                                             \
	{                                                                       \
		std::lock_guard<std::mutex> lk(Log::mutex);                         \
		std::cerr << Log::ColorYellow                                       \
		          << "[ WARNING @ " << __func__ << ":" << __LINE__ << " ] " \
		          << x << Log::ColorNormal << std::endl;                    \
	}

#define SUCCESS(x)                                                          \
	{                                                                       \
		std::lock_guard<std::mutex> lk(Log::mutex);                         \
		std::cerr << Log::ColorGreen                                        \
		          << "[ SUCCESS @ " << __func__ << ":" << __LINE__ << " ] " \
		          << x << Log::ColorNormal << std::endl;                    \
	}