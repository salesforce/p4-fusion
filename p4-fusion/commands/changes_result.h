/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <queue>

#include "common.h"

#include "change_list.h"
#include "result.h"

class ChangesResult : public Result
{
private:
	std::deque<ChangeList> m_Changes;

public:
	std::deque<ChangeList>& GetChanges() { return m_Changes; }

	void OutputStat(StrDict* varList) override;
};
