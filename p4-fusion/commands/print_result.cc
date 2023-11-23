/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "print_result.h"

#include <utility>

PrintResult::PrintResult(std::function<void()> _onNextFile, std::function<void(const char*, int)> _onFileContentChunk)
    : onNextFile(std::move(_onNextFile))
    , onFileContentChunk(std::move(_onFileContentChunk))
{
}

void PrintResult::OutputStat(StrDict* varList)
{
	onNextFile();
}

void PrintResult::OutputText(const char* data, int length)
{
	onFileContentChunk(data, length);
}

void PrintResult::OutputBinary(const char* data, int length)
{
	OutputText(data, length);
}
