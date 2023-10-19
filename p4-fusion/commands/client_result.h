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
#include "result.h"

class ClientResult : public Result
{
public:
	struct ClientSpecData
	{
		std::string client;
		std::vector<std::string> mapping;
	};

private:
	ClientSpecData m_Data;

public:
	const ClientSpecData& GetClientSpec() const { return m_Data; }

	void OutputStat(StrDict* varList) override;
};
