/*
* Copyright (c) 2024 Sourcegraph, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"

class LabelsResult : public Result
{
public:
	using LabelID = std::string;

	struct LabelData
	{
		std::string label;
	};

private:
	std::unordered_map<LabelID, LabelData> m_Labels;

public:
	[[nodiscard]] const std::unordered_map<LabelID, LabelData>& GetLabels() const { return m_Labels; }

	void OutputStat(StrDict* varList) override;
};
