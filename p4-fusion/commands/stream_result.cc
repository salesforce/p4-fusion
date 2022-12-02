/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "stream_result.h"

void StreamResult::OutputStat(StrDict* varList)
{
	m_Data.stream = varList->GetVar("Stream")->Text();
	m_Data.parent = varList->GetVar("Parent")->Text();

	// Read the paths.
	int i = 0;
	StrPtr* line = nullptr;

	while (true)
	{
		std::string indexString = std::to_string(i++);
		line = varList->GetVar(("Paths" + indexString).c_str());

		if (!line)
		{
			break;
		}

		m_Data.paths.push_back(line->Text());
	}

	// Read the remapped.
	i = 0;
	line = nullptr;
	while (true)
	{
		std::string indexString = std::to_string(i++);
		line = varList->GetVar(("Remapped" + indexString).c_str());

		if (!line)
		{
			break;
		}

		m_Data.remapped.push_back(line->Text());
	}

	// Read the ignored.
	i = 0;
	line = nullptr;
	while (true)
	{
		std::string indexString = std::to_string(i++);
		line = varList->GetVar(("Ignored" + indexString).c_str());

		if (!line)
		{
			break;
		}

		m_Data.ignored.push_back(line->Text());
	}
}
