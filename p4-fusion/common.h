/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

#include "p4/clientapi.h"

#define PRINT(x) std::cout << "[ PRINT @ " << __func__ << ":" << __LINE__ << " ] " << x << std::endl

#define ERR(x)                                                        \
	std::cerr << "\033[91m"                                           \
	          << "[ ERROR @ " << __func__ << ":" << __LINE__ << " ] " \
	          << x << "\033[0m" << std::endl

#define WARN(x) std::cerr << "\033[93m"                                             \
	                      << "[ WARNING @ " << __func__ << ":" << __LINE__ << " ] " \
	                      << x << "\033[0m" << std::endl

#define SUCCESS(x) std::cerr << "\033[32m"                                             \
	                         << "[ SUCCESS @ " << __func__ << ":" << __LINE__ << " ] " \
	                         << x << "\033[0m" << std::endl
