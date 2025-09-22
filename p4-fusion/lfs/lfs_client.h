/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>
#include "lfs/communication/communicator.h"

#include "git2/pathspec.h"

#include "git_api.h"

// Thread-safe class for handling Git LFS (Large File Storage) operations
class LFSClient
{
public:
	LFSClient(GitAPI& gitAPI, std::unique_ptr<Communicator> communicator, const std::vector<std::string>& lfsPatterns);

	std::vector<char> CreatePointerFileContents(const std::vector<char>& fileContents) const;

	static std::string CalcOID(const std::vector<char>& fileContents);
	Communicator::UploadResult UploadFile(const std::vector<char>& fileContents) const;

	bool IsLFSTracked(const std::string& filePath) const;

	std::vector<char> GetGitAttributesContents() const;

private:
	GitAPI& m_GitAPI;

	std::unique_ptr<Communicator> m_Communicator;

	std::vector<std::string> m_LFSPatterns;
	UniqueGitPathSpec m_LFSPathSpec;
};