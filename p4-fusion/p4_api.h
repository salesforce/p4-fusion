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
#include <memory>

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
	std::unique_ptr<T> Run(const char* command, const std::vector<std::string>& stringArguments);
	template <class T>
	std::unique_ptr<T> RunEx(const char* command, const std::vector<std::string>& stringArguments, const int commandRetries);

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

	std::unique_ptr<TestResult> TestConnection(const int retries);
	std::unique_ptr<ChangesResult> ShortChanges(const std::string& path);
	std::unique_ptr<ChangesResult> Changes(const std::string& path);
	std::unique_ptr<ChangesResult> Changes(const std::string& path, const std::string& from, int32_t maxCount);
	std::unique_ptr<ChangesResult> ChangesFromTo(const std::string& path, const std::string& from, const std::string& to);
	std::unique_ptr<ChangesResult> LatestChange(const std::string& path);
	std::unique_ptr<ChangesResult> OldestChange(const std::string& path);
	std::unique_ptr<DescribeResult> Describe(const std::string& cl);
	std::unique_ptr<FileLogResult> FileLog(const std::string& changelist);
	std::unique_ptr<SizesResult> Size(const std::string& file);
	std::unique_ptr<Result> Sync();
	std::unique_ptr<Result> Sync(const std::string& path);
	std::unique_ptr<SyncResult> GetFilesToSyncAtCL(const std::string& path, const std::string& cl);
	std::unique_ptr<PrintResult> PrintFile(const std::string& filePathRevision);
	std::unique_ptr<PrintResult> PrintFiles(const std::vector<std::string>& fileRevisions);
	void UpdateClientSpec();
	std::unique_ptr<ClientResult> Client();
	std::unique_ptr<StreamResult> Stream(const std::string& path);
	std::unique_ptr<UsersResult> Users();
	std::unique_ptr<InfoResult> Info();
};

template <class T>
inline std::unique_ptr<T> P4API::RunEx(const char* command, const std::vector<std::string>& stringArguments, const int commandRetries)
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

	std::unique_ptr<T> clientUser = std::unique_ptr<T>(new T());

	m_ClientAPI.SetArgv(argsCharArray.size(), argsCharArray.data());
	m_ClientAPI.Run(command, clientUser.get());

	int retries = commandRetries;
	while (m_ClientAPI.Dropped() || clientUser->GetError().IsError())
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

		clientUser = std::unique_ptr<T>(new T());

		m_ClientAPI.SetArgv(argsCharArray.size(), argsCharArray.data());
		m_ClientAPI.Run(command, clientUser.get());

		retries--;
	}

	if (m_ClientAPI.Dropped() || clientUser->GetError().IsFatal())
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
inline std::unique_ptr<T> P4API::Run(const char* command, const std::vector<std::string>& stringArguments)
{
	return RunEx<T>(command, stringArguments, CommandRetries);
}
