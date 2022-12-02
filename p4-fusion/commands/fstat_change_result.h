/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>

#include "common.h"
#include "result.h"


// Just the changelist for the requested files.
class FstatChangelistResult : public Result
{
	std::vector<std::string> m_Changelists;

public:
	std::vector<std::string>& GetChangelists() { return m_Changelists; }

	void OutputStat(StrDict* varList) override;
};
