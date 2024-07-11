/*
* Copyright (c) 2024 Sourcegraph, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "label_result.h"

void LabelResult::OutputStat(StrDict* varList)
{
	StrPtr* labelPtr = varList->GetVar("Label");
	StrPtr* revisionPtr = varList->GetVar("Revision");

	if (!labelPtr || !revisionPtr)
	{
		// TODO: We don't actually throw here.
		ERR("Label or Revision field not found for a Perforce label")
		return;
	}

	label = labelPtr->Text();
	revision = revisionPtr->Text();

	int i = 0;
	StrPtr* view;
	while (true)
	{
		std::string indexString = std::to_string(i++);
		view = varList->GetVar(("View" + indexString).c_str());

		if (!view)
		{
			break;
		}

		std::string viewStr = view->Text();
		views.emplace_back(viewStr);
	}
}
