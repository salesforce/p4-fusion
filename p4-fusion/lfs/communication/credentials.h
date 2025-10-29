/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>

class Credentials
{
public:
	enum class Type
	{
		None,
		UserNameAndPass,
		Token
	};

	Credentials();
	Credentials(const std::string& username, const std::string& password);
	Credentials(const std::string& token);

	Type GetType() const;

	const std::string& GetUsername() const;
	const std::string& GetPassword() const;
	const std::string& GetToken() const;

private:
	Type m_Type = Type::None;

	const std::string m_Username;
	const std::string m_PasswordOrToken;
};