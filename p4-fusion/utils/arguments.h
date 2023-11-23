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

	std::string GetParameter(const std::string& argName) const;
	int GetParameterInt(const std::string& argName) const;
	bool GetParameterBool(const std::string& argName) const;
	std::vector<std::string> GetParameterList(const std::string& argName) const;

public:
	Arguments(int argc, char** argv);
	Arguments() = delete;

	void RequiredParameter(const std::string& name, const std::string& helpText);
	void OptionalParameter(const std::string& name, const std::string& defaultValue, const std::string& helpText);
	void OptionalParameterList(const std::string& name, const std::string& helpText);
	bool IsValid() const;
	std::string Help() const;

	const ParameterData& GetParameterData(const std::string& name) const { return m_Parameters.at(name); }

	std::string GetPort() const { return GetParameter("--port"); };
	std::string GetUsername() const { return GetParameter("--user"); };
	std::string GetDepotPath() const { return GetParameter("--path"); };
	std::string GetSourcePath() const { return GetParameter("--src"); };
	std::string GetClient() const { return GetParameter("--client"); };
	int GetNetworkThreads() const { return GetParameterInt("--networkThreads"); };
	int GetPrintBatch() const { return GetParameterInt("--printBatch"); };
	int GetLookAhead() const { return GetParameterInt("--lookAhead"); };
	int GetRetries() const { return GetParameterInt("--retries"); };
	int GetRefresh() const { return GetParameterInt("--refresh"); };
	bool GetFsyncEnable() const { return GetParameterBool("--fsyncEnable"); };
	bool GetIncludeBinaries() const { return GetParameterBool("--includeBinaries"); };
	int GetMaxChanges() const { return GetParameterInt("--maxChanges"); };
	int GetFlushRate() const { return GetParameterInt("--flushRate"); };
	bool GetNoColor() const { return GetParameterBool("--noColor"); };
	bool GetNoMerge() const { return GetParameterBool("--noMerge"); };
	bool GetNoBaseCommit() const { return GetParameterBool("--noBaseCommit"); };
	std::vector<std::string> GetBranches() const { return GetParameterList("--branch"); };
};
