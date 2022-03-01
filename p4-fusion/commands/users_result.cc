/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "users_result.h"

void UsersResult::OutputStat(StrDict* varList)
{
	UserID userID = varList->GetVar("User")->Text();
	UserData userData;

	userData.email = varList->GetVar("Email")->Text();

	StrPtr* fullNamePtr = varList->GetVar("FullName");
	if (fullNamePtr)
	{
		userData.fullName = fullNamePtr->Text();
	}
	else
	{
		userData.fullName = userID;
	}

	m_Users.insert({ userID, userData });
}
