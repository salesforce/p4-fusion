/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"

class UsersResult : public Result
{
public:
	using UserID = std::string;

	struct UserData
	{
		std::string fullName;
		std::string email;
	};

private:
	std::unordered_map<UserID, UserData> m_Users;

public:
	[[nodiscard]] const std::unordered_map<UserID, UserData>& GetUserEmails() const { return m_Users; }

	void OutputStat(StrDict* varList) override;
};
