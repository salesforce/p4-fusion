/*
 * Copyright (c) 2024 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once
#include <iosfwd>
#include <string>
#include <vector>

#include "common.h"
#include "result.h"

class StreamResult : public Result
{
public:
	typedef enum
	{
		EStreamShare,
		EStreamExclude,
		EStreamImport,
		EStreamIsolate,
		EStreamUnknown
	} StreamRule;

	struct MappingData
	{
		StreamRule rule;
		std::string stream1;
		std::string stream2;
		friend std::ostream& operator<<(std::ostream& os, StreamResult::MappingData const& val);
	};

	// Currently Only the mapping stuff is important but this is subject to change.
	struct StreamSpecData
	{
		std::vector<MappingData> mapping;
	};

private:
	StreamSpecData m_Data;

public:
	const StreamSpecData& GetStreamSpec() const { return m_Data; };

	void OutputStat(StrDict* varList) override;
};

inline std::string StreamRuleToString(StreamResult::StreamRule rule)
{
	switch (rule)
	{
	case StreamResult::EStreamShare:
		return "Share";
	case StreamResult::EStreamExclude:
		return "Exclude";
	case StreamResult::EStreamImport:
		return "Import";
	case StreamResult::EStreamIsolate:
		return "Isolate";
	default:
		return "Unknown";
	}
}
