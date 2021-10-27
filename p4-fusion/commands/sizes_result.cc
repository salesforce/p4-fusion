/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "sizes_result.h"

void SizesResult::OutputStat(StrDict* varList)
{
	m_Size = varList->GetVar("fileSize")->Text();
}
