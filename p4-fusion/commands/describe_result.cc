/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "describe_result.h"

DescribeResult& DescribeResult::operator=(const DescribeResult& other)
{
	if (this == &other)
	{
		return *this;
	}

	m_FileData = other.m_FileData;

	return *this;
}

void DescribeResult::OutputStat(StrDict* varList)
{
}

int DescribeResult::OutputStatPartial(StrDict* varList)
{
	const std::string indexString = std::to_string(m_FileData.size());

	StrPtr* depotFile = varList->GetVar(("depotFile" + indexString).c_str());
	if (!depotFile)
	{
		// Done processing all depotFiles in the output.
		return 1;
	}
	std::string depotFileStr = depotFile->Text();
	std::string type = varList->GetVar(("type" + indexString).c_str())->Text();
	std::string revision = varList->GetVar(("rev" + indexString).c_str())->Text();
	std::string action = varList->GetVar(("action" + indexString).c_str())->Text();

	m_FileData.emplace_back(depotFileStr, revision, action, type);

	return 1;
}

void DescribeResult::OutputText(const char* data, int length)
{
}

void DescribeResult::OutputBinary(const char* data, int length)
{
}
