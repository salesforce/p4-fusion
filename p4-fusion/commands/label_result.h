/*
 * Copyright (c) 2024 Sourcegraph, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"

class LabelResult : public Result
{
public:
	std::string label;
	std::string revision;
	std::string description;
	std::vector<std::string> views;

public:
	void OutputStat(StrDict* varList) override;
};
