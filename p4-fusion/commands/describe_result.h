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
#include "file_data.h"
#include "result.h"

class DescribeResult : public Result
{
private:
	std::vector<FileData> m_FileData;

public:
	DescribeResult& operator=(const DescribeResult& other);
	[[nodiscard]] const std::vector<FileData>& GetFileData() const { return m_FileData; }

	void OutputStat(StrDict* varList) override;
	int OutputStatPartial(StrDict* varList) override;
	void OutputText(const char* data, int length) override;
	void OutputBinary(const char* data, int length) override;
};
