/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <array>

class STDHelpers
{
public:
	static bool EndsWith(const std::string& str, const std::string& checkStr);
	static bool StartsWith(const std::string& str, const std::string& checkStr);
	static bool Contains(const std::string& str, const std::string& subStr);
	static void Erase(std::string& source, const std::string& subStr);
	static void StripSurrounding(std::string& source, const char c);

	// Split the source into two strings at the first character 'c' after position 'startAt'.  The 'c' character is
	// not included in the returned strings.  Text before the 'startAt' will not be included.
	static std::array<std::string, 2> SplitAt(const std::string& source, const char c, const size_t startAt = 0);
};
