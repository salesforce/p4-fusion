/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "users_result.h"

void UsersResult::OutputStat(StrDict* varList)
{
	StrPtr* userIDPtr = varList->GetVar("User");
	StrPtr* emailPtr = varList->GetVar("Email");

	if (!userIDPtr || !emailPtr)
	{
		// TODO: We don't actually throw here.
		ERR("UserID or email not found for a Perforce user")
		return;
	}

	UserID userID = userIDPtr->Text();
	UserData userData;

	userData.email = emailPtr->Text();

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
