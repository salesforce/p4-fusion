/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"

class Result : public ClientUser
{
	Error m_Error;

public:
	void HandleError(Error* e) override;

	const Error& GetError() const { return m_Error; }
	/*
	 * HasError returns true when the result contains an error.
	 */
	bool HasError() const
	{
		return GetError().IsError();
	}
	/*
	 * PrintError formats the contained error, if any, as a std::string.
	 */
	std::string PrintError() const
	{
		if (!HasError())
		{
			return "";
		}
		StrBuf str;
		m_Error.Fmt(&str);
		return { str.Text() };
	}
};
