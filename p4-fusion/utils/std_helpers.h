/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>

class STDHelpers
{
public:
	static bool EndsWith(const std::string& str, const std::string& checkStr);
	static bool StartsWith(const std::string& str, const std::string& checkStr);
	static bool Contains(const std::string& str, const std::string& subStr);
	static void Erase(std::string& source, const std::string& subStr);
};
