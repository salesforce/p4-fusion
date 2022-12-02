/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "fstat_change_result.h"


// Should be called once per varlist in the call return.
void FstatChangelistResult::OutputStat(StrDict* varList)
{
    std::string changelist = varList->GetVar("headChange")->Text();
    m_Changelists.push_back(changelist);
}
