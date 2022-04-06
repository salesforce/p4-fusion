/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "describe_result.h"

void DescribeResult::OutputStat(StrDict* varList)
{
}

int DescribeResult::OutputStatPartial(StrDict* varList)
{
	std::string indexString = std::to_string(m_FileData.size());

	StrPtr* depotFile = varList->GetVar(("depotFile" + indexString).c_str());
	if (!depotFile)
	{
		// Quick exit if the object returned is not a file
		return 0;
	}
	StrPtr* type = varList->GetVar(("type" + indexString).c_str());
	StrPtr* revision = varList->GetVar(("rev" + indexString).c_str());
	StrPtr* action = varList->GetVar(("action" + indexString).c_str());

	m_FileData.push_back(FileData {});
	FileData* fileData = &m_FileData.back();

	fileData->depotFile = depotFile->Text();
	fileData->revision = revision->Text();
	fileData->type = type->Text();
	fileData->action = action->Text();

	return 1;
}

void DescribeResult::OutputText(const char* data, int length)
{
}

void DescribeResult::OutputBinary(const char* data, int length)
{
	OutputText(data, length);
}
