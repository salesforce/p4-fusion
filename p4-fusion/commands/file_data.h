/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "git2/object.h"

struct FileData
{
	std::string depotFile;
	std::string revision;
	std::string action;
	std::string type;
	std::vector<char> contents;
	git_oid blob = {};
	bool shouldCommit = false;

	void Clear()
	{
		depotFile.clear();
		revision.clear();
		action.clear();
		type.clear();
		contents.clear();
	}
};
