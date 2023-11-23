/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <vector>
#include <functional>
#include "common.h"
#include "result.h"

class PrintResult : public Result
{
private:
	std::function<void()> onNextFile;
	std::function<void(const char*, int)> onFileContentChunk;

public:
	PrintResult() = delete;
	/*
	 * PrintResult is passed to the P4 API to get the contents of files. The two passed
	 * callbacks can be used to retrieve these files data.
	 * The files are printed by the server in the order in they appear when talking to the
	 * helix API.
	 *
	 * Before each file, the onNextFile callback will be called.
	 * Then, onFileContentChunk is called until the whole file is printed.
	 * Once done, no more invocations are done.
	 * Think of this like a tar archive reader: File Header, Content, File Header, Content, end.
	 */
	PrintResult(std::function<void()> onNextFile, std::function<void(const char*, int)> onFileContentChunk);
	void OutputStat(StrDict* varList) override;
	void OutputText(const char* data, int length) override;
	void OutputBinary(const char* data, int length) override;
};
