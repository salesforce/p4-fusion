/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"

class UsersResult : public Result
{
	std::unordered_map<std::string, std::string> m_Users;

public:
	const std::unordered_map<std::string, std::string>& GetUserEmails() const { return m_Users; }

	void OutputStat(StrDict* varList) override;
};
