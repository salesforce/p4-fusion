/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "result.h"

class TestResult : public Result
{
public:
	void OutputStat(StrDict* varList) override;
	void OutputText(const char* data, int length) override;
	void OutputBinary(const char* data, int length) override;
};
