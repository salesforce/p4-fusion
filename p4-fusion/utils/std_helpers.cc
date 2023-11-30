/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "std_helpers.h"

bool STDHelpers::EndsWith(const std::string& str, const std::string& checkStr)
{
	if (checkStr.size() > str.size())
	{
		return false;
	}

	return std::string(str.end() - checkStr.size(), str.end()) == checkStr;
}

bool STDHelpers::StartsWith(const std::string& str, const std::string& checkStr)
{
	if (checkStr.size() > str.size())
	{
		return false;
	}

	return str.substr(0, checkStr.size()) == checkStr;
}

bool STDHelpers::Contains(const std::string& str, const std::string& subStr)
{
	return str.find(subStr) != std::string::npos;
}

void STDHelpers::StripSurrounding(std::string& source, const char c)
{
	const size_t size = source.size();
	size_t start = 0;
	size_t end = size;
	while (start < end && source[start] == c)
	{
		start++;
	}
	while (end - 1 > start && source[end - 1] == c)
	{
		end--;
	}
	if (end < size)
	{
		source.erase(end, size - end);
	}
	if (start > 0)
	{
		source.erase(0, start);
	}
}
