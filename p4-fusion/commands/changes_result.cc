/*
 * Copyright (c) 2022, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "changes_result.h"

void ChangesResult::OutputStat(StrDict* varList)
{
	m_Changes.push_back({});
	ChangeList& data = m_Changes.back();

	data.number = varList->GetVar("change")->Text();
	data.description = varList->GetVar("desc")->Text();
	data.user = varList->GetVar("user")->Text();
	data.timestamp = varList->GetVar("time")->Atoi64();
	data.filesDownloaded = std::make_shared<std::atomic<int>>(-1);
	data.canDownload = std::make_shared<std::atomic<bool>>(false);
	data.canDownloadMutex = std::make_shared<std::mutex>();
	data.canDownloadCV = std::make_shared<std::condition_variable>();
	data.commitMutex = std::make_shared<std::mutex>();
	data.commitCV = std::make_shared<std::condition_variable>();
}
