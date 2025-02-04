/*
 * Copyright (c) 2024 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <iostream>
#include <vector>

#include "common.h"
#include "stream_result.h"
#include "utils/std_helpers.h"

void StreamResult::OutputStat(StrDict* varList)
{
	int i = 0;
	StrPtr* view = nullptr;

	while (true)
	{
		std::string indexString = std::to_string(i++);
		view = varList->GetVar(("Paths" + indexString).c_str());

		if (!view)
		{
			break;
		}

		auto strSplit = STDHelpers::SplitOnDelim(view->Text(), ' ');
		if (strSplit.size() <= 1)
		{
			continue;
		}

		// Get the enum value
		StreamResult::StreamRule rule = StreamResult::EStreamUnknown;
		if (strSplit[0] == "share")
		{
			rule = StreamResult::EStreamShare;
		}
		else if (strSplit[0] == "exclude")
		{
			rule = StreamResult::EStreamExclude;
		}
		else if (strSplit[0] == "import")
		{
			rule = StreamResult::EStreamImport;
		}
		else if (strSplit[0] == "isolate")
		{
			rule = StreamResult::EStreamIsolate;
		}

		if (strSplit.size() < 3)
		{
			m_Data.mapping.push_back(StreamResult::MappingData { rule, strSplit[1], "" });
		}
		else
		{
			m_Data.mapping.push_back(StreamResult::MappingData { rule, strSplit[1], strSplit[2] });
		}
	}
}

std::ostream& operator<<(std::ostream& os, StreamResult::MappingData const& val)
{
	os << StreamRuleToString(val.rule) << ": Stream1: " << val.stream1 << " Stream2: " << val.stream2;
	return os;
}
