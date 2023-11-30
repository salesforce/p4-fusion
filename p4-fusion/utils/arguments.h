/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <map>
#include <string>
#include <vector>

#include "common.h"

class Arguments
{
	struct ParameterData
	{
		bool isRequired;
		bool isSet;
		std::vector<std::string> valueList;
		std::string helpText;
	};

	std::map<std::string, ParameterData> m_Parameters;

	[[nodiscard]] std::string GetParameter(const std::string& argName) const;
	[[nodiscard]] int GetParameterInt(const std::string& argName) const;
	[[nodiscard]] bool GetParameterBool(const std::string& argName) const;
	[[nodiscard]] std::vector<std::string> GetParameterList(const std::string& argName) const;

public:
	Arguments(int argc, char** argv);
	Arguments() = delete;

	void Print() const;
	void RequiredParameter(const std::string& name, const std::string& helpText);
	void OptionalParameter(const std::string& name, const std::string& defaultValue, const std::string& helpText);
	void OptionalParameterList(const std::string& name, const std::string& helpText);
	[[nodiscard]] bool IsValid() const;
	[[nodiscard]] std::string Help() const;

	[[nodiscard]] std::string GetPort() const { return GetParameter("--port"); };
	[[nodiscard]] std::string GetUsername() const { return GetParameter("--user"); };
	[[nodiscard]] std::string GetDepotPath() const { return GetParameter("--path"); };
	[[nodiscard]] std::string GetSourcePath() const { return GetParameter("--src"); };
	[[nodiscard]] std::string GetClient() const { return GetParameter("--client"); };
	[[nodiscard]] int GetNetworkThreads() const { return GetParameterInt("--networkThreads"); };
	[[nodiscard]] int GetPrintBatch() const { return GetParameterInt("--printBatch"); };
	[[nodiscard]] int GetLookAhead() const { return GetParameterInt("--lookAhead"); };
	[[nodiscard]] int GetRetries() const { return GetParameterInt("--retries"); };
	[[nodiscard]] int GetRefresh() const { return GetParameterInt("--refresh"); };
	[[nodiscard]] bool GetFsyncEnable() const { return GetParameterBool("--fsyncEnable"); };
	[[nodiscard]] bool GetIncludeBinaries() const { return GetParameterBool("--includeBinaries"); };
	[[nodiscard]] int GetMaxChanges() const { return GetParameterInt("--maxChanges"); };
	[[nodiscard]] int GetFlushRate() const { return GetParameterInt("--flushRate"); };
	[[nodiscard]] bool GetNoColor() const { return GetParameterBool("--noColor"); };
	[[nodiscard]] bool GetNoMerge() const { return GetParameterBool("--noMerge"); };
	[[nodiscard]] bool GetNoBaseCommit() const { return GetParameterBool("--noBaseCommit"); };
	[[nodiscard]] std::vector<std::string> GetBranches() const { return GetParameterList("--branch"); };
};
