/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <vector>
#include <string>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include "common.h"

#include "change_list.h"
#include "result.h"

class ChangesResult : public Result
{
private:
	std::vector<ChangeList> m_Changes;

public:
	std::vector<ChangeList>& GetChanges() { return m_Changes; }
	void SkipFirst();

	void OutputStat(StrDict* varList) override;
};
