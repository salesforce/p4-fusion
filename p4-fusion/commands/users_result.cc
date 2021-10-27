/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "users_result.h"

void UsersResult::OutputStat(StrDict* varList)
{
	m_Users.insert({ varList->GetVar("User")->Text(), varList->GetVar("Email")->Text() });
}
