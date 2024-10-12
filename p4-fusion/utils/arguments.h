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
	std::vector<std::string> GetParameterList(const std::string& argName) const;

public:
	static Arguments* GetSingleton();

	void RequiredParameter(const std::string& name, const std::string& helpText);
	void OptionalParameter(const std::string& name, const std::string& defaultValue, const std::string& helpText);
	void OptionalParameterList(const std::string& name, const std::string& helpText);
	void Initialize(int argc, char** argv);
	bool IsValid() const;
	std::string Help();

	const ParameterData& GetParameterData(const std::string& name) const { return m_Parameters.at(name); }

	std::string GetPort() const { return GetParameter("--port"); };
	std::string GetUsername() const { return GetParameter("--user"); };
	std::string GetDepotPath() const { return GetParameter("--path"); };
	std::string GetSourcePath() const { return GetParameter("--src"); };
	std::string GetClient() const { return GetParameter("--client"); };
	std::string GetNetworkThreads() const { return GetParameter("--networkThreads"); };
	std::string GetFileSystemThreads() const { return GetParameter("--fileSystemThreads"); };
	std::string GetPrintBatch() const { return GetParameter("--printBatch"); };
	std::string GetLookAhead() const { return GetParameter("--lookAhead"); };
	std::string GetRetries() const { return GetParameter("--retries"); };
	std::string GetRefresh() const { return GetParameter("--refresh"); };
	std::string GetFsyncEnable() const { return GetParameter("--fsyncEnable"); };
	std::string GetIncludeBinaries() const { return GetParameter("--includeBinaries"); };
	std::string GetMaxChanges() const { return GetParameter("--maxChanges"); };
	std::string GetFlushRate() const { return GetParameter("--flushRate"); };
	std::string GetNoColor() const { return GetParameter("--noColor"); };
	std::string GetNoMerge() const { return GetParameter("--noMerge"); };
	std::string GetStreamMappings() const { return GetParameter("--streamMappings"); };
	std::vector<std::string> GetBranches() const { return GetParameterList("--branch"); };
};
