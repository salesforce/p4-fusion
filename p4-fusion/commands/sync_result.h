/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <vector>
#include <string>

#include "common.h"
#include "result.h"

class SyncResult : public Result
{
public:
	struct SyncData
	{
		std::string depotFile;
		std::string revision;
	};

private:
	std::vector<SyncData> m_SyncData;

public:
	const std::vector<SyncData>& GetSyncData() const { return m_SyncData; }

	void OutputStat(StrDict* varList) override;
};
