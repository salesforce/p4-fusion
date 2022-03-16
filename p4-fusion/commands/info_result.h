/*
 * Copyright (c) 2022, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"

class InfoResult : public Result
{
	int m_TimezoneMinutes;

public:
	InfoResult();

	int GetServerTimezoneMinutes() const { return m_TimezoneMinutes; };

	void OutputStat(StrDict* varList) override;
};
