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

void STDHelpers::Erase(std::string& source, const std::string& subStr)
{
	if (!Contains(source, subStr))
	{
		return;
	}

	source.erase(source.find(subStr), subStr.size());
}

std::array<std::string, 2> STDHelpers::SplitAt(const std::string& source, const char c, const size_t startAt)
{
	size_t pos = source.find(c, startAt);
	if (pos != std::string::npos && pos < source.size())
	{
		return { source.substr(startAt, pos - startAt), source.substr(pos + 1) };
	}
	return { source, "" };
}
