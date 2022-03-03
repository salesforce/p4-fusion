/*
 * Copyright (c) 2022, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "files_result.h"

void FilesResult::OutputStat(StrDict* varList)
{
	FileData data;

	data.depotFile = varList->GetVar("depotFile")->Text();
	data.revision = varList->GetVar("rev")->Text();
	data.change = varList->GetVar("change")->Text();
	data.action = varList->GetVar("action")->Text();
	data.time = varList->GetVar("time")->Text();
	data.type = varList->GetVar("type")->Text();

	m_Files.push_back(data);
}
