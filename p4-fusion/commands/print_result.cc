/*
 * Copyright (c) 2022, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "print_result.h"

void PrintResult::OutputStat(StrDict* varList)
{
	m_Data.push_back(PrintData {});
}

void PrintResult::OutputText(const char* data, int length)
{
	std::vector<char>& fileContent = m_Data.back().contents;
	fileContent.insert(fileContent.end(), data, data + length);
}

void PrintResult::OutputBinary(const char* data, int length)
{
	OutputText(data, length);
}
