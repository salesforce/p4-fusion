/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include "credentials.h"

Credentials::Credentials()
    : m_Type(Type::None)
{
}

Credentials::Credentials(const std::string& username, const std::string& password)
    : m_Type(Type::UserNameAndPass)
    , m_Username(username)
    , m_PasswordOrToken(password)
{
}

Credentials::Credentials(const std::string& token)
    : m_Type(Type::Token)
    , m_Username()
    , m_PasswordOrToken(token)
{
}

Credentials::Type Credentials::GetType() const
{
	return m_Type;
}

const std::string& Credentials::GetUsername() const
{
	return m_Username;
}

const std::string& Credentials::GetPassword() const
{
	return m_PasswordOrToken;
}

const std::string& Credentials::GetToken() const
{
	return m_PasswordOrToken;
}