/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "sync_result.h"

void SyncResult::OutputStat(StrDict* varList)
{
	m_SyncData.push_back(SyncData { varList->GetVar("depotFile")->Text(), varList->GetVar("rev")->Text() });
}
