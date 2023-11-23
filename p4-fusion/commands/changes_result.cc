/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "changes_result.h"

void ChangesResult::OutputStat(StrDict* varList)
{
	m_Changes.emplace_back(
	    varList->GetVar("change")->Atoi(),
	    varList->GetVar("desc")->Text(),
	    varList->GetVar("user")->Text(),
	    varList->GetVar("time")->Atoi64());
}
