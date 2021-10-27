/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>

#include "common.h"
#include "result.h"

class SizesResult : public Result
{
	std::string m_Size;

public:
	std::string GetSize() { return m_Size; }

	void OutputStat(StrDict* varList) override;
};
