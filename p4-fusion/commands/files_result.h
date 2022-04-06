/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"

class FilesResult : public Result
{
public:
	struct FileData
	{
		std::string depotFile;
		std::string revision;
		std::string change;
		std::string action;
		std::string type;
		std::string time;
		std::string size;
	};

private:
	std::vector<FileData> m_Files;

public:
	std::vector<FileData>& GetFilesResult() { return m_Files; }

	void OutputStat(StrDict* varList) override;
};
