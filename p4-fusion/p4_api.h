/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <thread>
#include <chrono>
#include <cstdint>

#include "common.h"

#include "commands/file_map.h"
#include "commands/changes_result.h"
#include "commands/describe_result.h"
#include "commands/filelog_result.h"
#include "commands/sizes_result.h"
#include "commands/sync_result.h"
#include "commands/print_result.h"
#include "commands/users_result.h"
#include "commands/info_result.h"
#include "commands/client_result.h"
#include "commands/test_result.h"
#include "commands/stream_result.h"

class P4API
{
	ClientApi m_ClientAPI;
	FileMap m_ClientMapping;
	int m_Usage;

	bool Initialize();
	bool Deinitialize();
	bool Reinitialize();
	bool CheckErrors(Error& e, StrBuf& msg);

	template <class T>
	T Run(const char* command, const std::vector<std::string>& stringArguments);
	template <class T>
	T RunEx(const char* command, const std::vector<std::string>& stringArguments, const int commandRetries);

public:
	static std::string P4PORT;
	static std::string P4USER;
	static std::string P4CLIENT;
	static ClientResult::ClientSpecData ClientSpec;
	static int CommandRetries;
	static int CommandRefreshThreshold;

	// Helix Core C++ API seems to crash while making connections parallely.
	static std::mutex InitializationMutex;

	static bool InitializeLibraries();
	static bool ShutdownLibraries();

	P4API();
	~P4API();

	bool IsDepotPathValid(const std::string& depotPath);
	bool IsFileUnderDepotPath(const std::string& fileRevision, const std::string& depotPath);
	bool IsDepotPathUnderClientSpec(const std::string& depotPath);
	bool IsFileUnderClientSpec(const std::string& fileRevision);

	void AddClientSpecView(const std::vector<std::string>& viewStrings);

	TestResult TestConnection(const int retries);
	ChangesResult ShortChanges(const std::string& path);
	ChangesResult Changes(const std::string& path);
	ChangesResult Changes(const std::string& path, const std::string& from, int32_t maxCount);
	ChangesResult ChangesFromTo(const std::string& path, const std::string& from, const std::string& to);
	ChangesResult LatestChange(const std::string& path);
	ChangesResult OldestChange(const std::string& path);
	DescribeResult Describe(const std::string& cl);
	FileLogResult FileLog(const std::string& changelist);
	SizesResult Size(const std::string& file);
	Result Sync();
	Result Sync(const std::string& path);
	SyncResult GetFilesToSyncAtCL(const std::string& path, const std::string& cl);
	PrintResult PrintFile(const std::string& filePathRevision);
	PrintResult PrintFiles(const std::vector<std::string>& fileRevisions);
	void UpdateClientSpec();
	ClientResult Client();
	StreamResult Stream(const std::string& path);
	UsersResult Users();
	InfoResult Info();
};

template <class T>
inline T P4API::RunEx(const char* command, const std::vector<std::string>& stringArguments, const int commandRetries)
{
	std::string argsString;
	for (const std::string& stringArg : stringArguments)
	{
		argsString = argsString + " " + stringArg;
	}

	std::vector<char*> argsCharArray;
	for (const std::string& arg : stringArguments)
	{
		argsCharArray.push_back((char*)arg.c_str());
	}

	T clientUser;

	m_ClientAPI.SetArgv(argsCharArray.size(), argsCharArray.data());
	m_ClientAPI.Run(command, &clientUser);

	int retries = commandRetries;
	while (m_ClientAPI.Dropped() || clientUser.GetError().IsError())
	{
		if (retries == 0)
		{
			break;
		}

		ERR("Connection dropped or command errored, retrying in 5 seconds.");
		std::this_thread::sleep_for(std::chrono::seconds(5));

		if (Reinitialize())
		{
			SUCCESS("Reinitialized P4API");
		}
		else
		{
			ERR("Could not reinitialize P4API");
		}

		WARN("Retrying: p4 " << command << argsString);

		clientUser = T();

		m_ClientAPI.SetArgv(argsCharArray.size(), argsCharArray.data());
		m_ClientAPI.Run(command, &clientUser);

		retries--;
	}

	if (m_ClientAPI.Dropped() || clientUser.GetError().IsFatal())
	{
		ERR("Exiting due to receiving errors even after retrying " << CommandRetries << " times");
		Deinitialize();
		std::exit(1);
	}

	m_Usage++;
	if (m_Usage > CommandRefreshThreshold)
	{
		int refreshRetries = CommandRetries;
		while (refreshRetries > 0)
		{
			WARN("Trying to refresh the connection due to age (" << m_Usage << " > " << CommandRefreshThreshold << ").");
			if (Reinitialize())
			{
				SUCCESS("Connection was refreshed");
				break;
			}
			ERR("Could not refresh connection due to old age. Retrying in 5 seconds");
			std::this_thread::sleep_for(std::chrono::seconds(5));

			refreshRetries--;
		}

		if (refreshRetries == 0)
		{
			ERR("Could not refresh the connection after " << CommandRetries << " retries. Exiting.");
			std::exit(1);
		}
	}

	return clientUser;
}

template <class T>
inline T P4API::Run(const char* command, const std::vector<std::string>& stringArguments)
{
	return RunEx<T>(command, stringArguments, CommandRetries);
}
