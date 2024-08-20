/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "arguments.h"

#include <algorithm>
#include <thread>

Arguments::Arguments(int argc, char** argv)
{
	RequiredParameter("--path", "P4 depot path to convert to a Git repo.  If used with '--branch', this is the base path for the branches.");
	RequiredParameter("--src", "Relative path where the git repository should be created. This path should be empty before running p4-fusion for the first time in a directory.");
	RequiredParameter("--port", "Specify which P4PORT to use.");
	RequiredParameter("--user", "Specify which P4USER to use. Please ensure that the user is logged in.");
	RequiredParameter("--client", "Name/path of the client workspace specification.");
	OptionalParameter("--lookAhead", "1", "How many CLs in the future, at most, shall we keep downloaded by the time it is to commit them?");
	OptionalParameter("--noBaseCommit", "false", "Whether an empty base commit should be created so that branches have a common merge base.");
	OptionalParameterList("--branch", "A branch to migrate under the depot path.  May be specified more than once.  If at least one is given and the noMerge option is false, then the Git repository will include merges between branches in the history.  You may use the formatting 'depot/path:git-alias', separating the Perforce branch sub-path from the git alias name by a ':'; if the depot path contains a ':', then you must provide the git branch alias.");
	OptionalParameter("--noMerge", "false", "Disable performing a Git merge when a Perforce branch integrates (or copies, etc) into another branch.");
	OptionalParameter("--networkThreads", std::to_string(std::thread::hardware_concurrency()), "Specify the number of threads in the threadpool for running network calls. Defaults to the number of logical CPUs.");
	OptionalParameter("--printBatch", "1", "Specify the p4 print batch size.");
	OptionalParameter("--maxChanges", "-1", "Specify the max number of changelists which should be processed in a single run. -1 signifies unlimited range.");
	OptionalParameter("--retries", "10", "Specify how many times a command should be retried before the process exits in a failure.");
	OptionalParameter("--refresh", "100", "Specify how many times a connection should be reused before it is refreshed.");
	OptionalParameter("--fsyncEnable", "false", "Enable fsync() while writing objects to disk to ensure they get written to permanent storage immediately instead of being cached. This is to mitigate data loss in events of hardware failure.");
	OptionalParameter("--includeBinaries", "false", "Do not discard binary files while downloading changelists.");
	OptionalParameter("--flushRate", "30", "Interval in seconds at which the profiling data is flushed to the disk.");
	OptionalParameter("--noColor", "false", "Disable colored output.");
	OptionalParameter("--noConvertLabels", "false", "Whether or not to disable label to tag conversion.");
	OptionalParameter("--labelCache", "", "Absolute path to a label cache file. If not specified, labels will not be cached.");

	for (int i = 1; i < argc - 1; i += 2)
	{
		std::string name = argv[i];

		if (m_Parameters.find(name) != m_Parameters.end())
		{
			m_Parameters.at(name).valueList.emplace_back(argv[i + 1]);
			m_Parameters.at(name).isSet = true;
		}
		else
		{
			// TODO: Throw?
			WARN("Unknown argument: " << name)
		}
	}
}

void Arguments::Print() const
{
	auto depotPath = GetDepotPath();
	auto srcPath = GetSourcePath();
	auto fsyncEnable = GetFsyncEnable();
	auto includeBinaries = GetIncludeBinaries();
	auto maxChanges = GetMaxChanges();
	auto flushRate = GetFlushRate();
	auto branchNames = GetBranches();
	auto noColor = GetNoColor();
	auto P4PORT = GetPort();
	auto P4USER = GetUsername();
	auto P4CLIENT = GetClient();
	auto CommandRetries = GetRetries();
	auto CommandRefreshThreshold = GetRefresh();
	auto networkThreads = GetNetworkThreads();
	auto printBatch = GetPrintBatch();
	auto lookAhead = GetLookAhead();
	bool profiling(false);
#if MTR_ENABLED
	profiling = true;
#endif
	const std::string tracePath = (srcPath + (srcPath.back() == '/' ? "" : "/") + "trace.json");

	PRINT("Perforce Port: " << P4PORT)
	PRINT("Perforce User: " << P4USER)
	PRINT("Perforce Client: " << P4CLIENT)
	PRINT("Depot Path: " << depotPath)
	PRINT("Network Threads: " << networkThreads)
	PRINT("Print Batch: " << printBatch)
	PRINT("Look Ahead: " << lookAhead)
	PRINT("Max Retries: " << CommandRetries)
	PRINT("Max Changes: " << maxChanges)
	PRINT("Refresh Threshold: " << CommandRefreshThreshold)
	PRINT("Fsync Enable: " << fsyncEnable)
	PRINT("Include Binaries: " << includeBinaries)
	PRINT("Profiling: " << profiling << " (" << tracePath << ")")
	PRINT("Profiling Flush Rate: " << flushRate)
	PRINT("No Colored Output: " << noColor)
}

std::string Arguments::GetParameter(const std::string& argName) const
{
	if (m_Parameters.find(argName) != m_Parameters.end())
	{
		if (m_Parameters.at(argName).valueList.empty())
		{
			return "";
		}
		// Use the last specified version of the parameter.
		return m_Parameters.at(argName).valueList.back();
	}
	return "";
}

// Returns the parameters value for the given argument, or 0 if not present or
// cannot be parsed.
int Arguments::GetParameterInt(const std::string& argName) const
{
	auto val = GetParameter(argName);
	return std::atoi(val.c_str());
}

bool Arguments::GetParameterBool(const std::string& argName) const
{
	auto val = GetParameter(argName);
	return val != "false";
}

std::vector<std::string> Arguments::GetParameterList(const std::string& argName) const
{
	if (m_Parameters.find(argName) != m_Parameters.end())
	{
		return m_Parameters.at(argName).valueList;
	}
	return {};
}

bool Arguments::IsValid() const
{
	for (auto& arg : m_Parameters)
	{
		const ParameterData& argData = arg.second;
		if (argData.isRequired && !argData.isSet)
		{
			return false;
		}
	}
	return true;
}

std::string Arguments::Help() const
{
	std::string text = "\n";

	for (auto& param : m_Parameters)
	{
		const std::string& name = param.first;
		const ParameterData& paramData = param.second;

		text += name + " ";
		if (paramData.isRequired)
		{
			text += "\033[91m[Required]\033[0m";
		}
		else
		{
			text += "\033[93m[Optional, Default is " + (paramData.valueList.empty() ? "empty" : paramData.valueList.back()) + "]\033[0m";
		}
		text += "\n        " + paramData.helpText + "\n\n";
	}

	return text;
}

void Arguments::RequiredParameter(const std::string& name, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = true;
	paramData.isSet = false;
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}

void Arguments::OptionalParameter(const std::string& name, const std::string& defaultValue, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = false;
	paramData.isSet = false;
	paramData.valueList.push_back(defaultValue);
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}

void Arguments::OptionalParameterList(const std::string& name, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = false;
	paramData.isSet = false;
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}
