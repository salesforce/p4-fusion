/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "communicator.h"
#include "credentials.h"

class LFSComm : public Communicator
{
public:
	LFSComm(const std::string& serverURL, const Credentials& creds);

	virtual ~LFSComm() = default;

	UploadResult UploadFile(const std::vector<char>& fileContents) const override;

private:
	const std::string m_ServerURL;
	const Credentials m_Creds;
};
