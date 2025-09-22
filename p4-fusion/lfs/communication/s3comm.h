/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "communicator.h"

class S3Comm : public Communicator
{
public:
	S3Comm(const std::string& serverURL, const std::string& bucket, const std::string& repository, const std::string& username, const std::string& password);
	virtual ~S3Comm() = default;

	UploadResult UploadFile(const std::vector<char>& fileContents) const override;

private:
	const std::string m_ServerURL;
	const std::string m_Bucket;
	const std::string m_Repository;
	const std::string m_Username;
	const std::string m_Password;
};
