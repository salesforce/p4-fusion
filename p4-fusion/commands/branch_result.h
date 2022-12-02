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


class BranchResult : public Result
{
public:
	struct BranchData
	{
		std::string name;
		std::string description;
		std::vector<std::string> view;
	};

private:
	BranchData m_Data;

public:
	BranchData& GetBranchData() { return m_Data; };

	void OutputStat(StrDict* varList) override;
};
