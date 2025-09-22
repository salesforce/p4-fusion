/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

class Communicator
{
protected:
	Communicator() = default;

public:
	virtual ~Communicator() = default;

	enum class UploadResult
	{
		Uploaded,
		AlreadyExists,
		Error
	};

	virtual UploadResult UploadFile(const std::vector<char>& fileContents) const = 0;
};
