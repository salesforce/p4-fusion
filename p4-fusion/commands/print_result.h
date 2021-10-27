/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <vector>

#include "common.h"
#include "result.h"

class PrintResult : public Result
{
public:
	struct PrintData
	{
		std::vector<char> contents;
	};

private:
	std::vector<PrintData> m_Data;

public:
	const std::vector<PrintData>& GetPrintData() const { return m_Data; }

	void OutputStat(StrDict* varList) override;
	void OutputText(const char* data, int length) override;
	void OutputBinary(const char* data, int length) override;
};
