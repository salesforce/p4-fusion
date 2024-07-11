/*
* Copyright (c) 2024 Sourcegraph, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "labels_result.h"

void LabelsResult::OutputStat(StrDict* varList)
{
	StrPtr* labelIDPtr = varList->GetVar("label");

	if (!labelIDPtr)
	{
		// TODO: We don't actually throw here.
		ERR("LabelID not found for a Perforce label")
		return;
	}

	LabelID labelID = labelIDPtr->Text();
	LabelData labelData;

	labelData.label = labelIDPtr->Text();

	m_Labels.insert({ labelID, labelData });
}
