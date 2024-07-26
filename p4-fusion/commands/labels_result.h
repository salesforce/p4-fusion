/*
 * Copyright (c) 2024 Sourcegraph, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"
#include <list>

class LabelsResult : public Result
{
	std::list<std::string> m_Labels;

public:
	[[nodiscard]] const std::list<std::string>& GetLabels() const { return m_Labels; }

	void OutputStat(StrDict* varList) override;
};
