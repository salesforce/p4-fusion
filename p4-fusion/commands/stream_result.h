/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <vector>
#include <string>

#include "common.h"
#include "file_data.h"
#include "result.h"

class StreamResult : public Result
{
public:
	struct StreamData
	{
		// The depot path (//depotname/streamname)
		std::string stream;

		// "Name" value is the human readable short name for the stream.  Not used.

		// Parent stream; "none" for no parent, or the depot path for the stream.
		std::string parent;

		// List of paths in the depot, prefixed with the path type;
		//	one of share/isolate/import/import+/exclude
		std::vector<std::string> paths;

		std::vector<std::string> remapped;

		std::vector<std::string> ignored;
	};

private:
	StreamData m_Data;

public:
	const StreamData& GetStreamData() const { return m_Data; }

	void OutputStat(StrDict* varList) override;
};
