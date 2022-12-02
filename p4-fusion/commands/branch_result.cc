/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "branch_result.h"

void BranchResult::OutputStat(StrDict* varList)
{
	m_Data.name = varList->GetVar("Branch")->Text();
	m_Data.description = varList->GetVar("Description")->Text();

	int i = 0;
	StrPtr* view = nullptr;

	while (true)
	{
		std::string indexString = std::to_string(i++);
		view = varList->GetVar(("View" + indexString).c_str());

		if (!view)
		{
			break;
		}

		m_Data.view.push_back(view->Text());
	}
}
