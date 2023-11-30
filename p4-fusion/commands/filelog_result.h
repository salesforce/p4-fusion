/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <vector>
#include <string>

#include "common.h"
#include "result.h"
#include "file_data.h"
#include "utils/std_helpers.h"
#include "git_api.h"

// Very limited to just a single file log entry per file.
class FileLogResult : public Result
{
private:
	std::vector<FileData> m_FileData;

public:
	FileLogResult& operator=(const FileLogResult& other);
	[[nodiscard]] const std::vector<FileData>& GetFileData() const { return m_FileData; }

	void OutputStat(StrDict* varList) override;
};
