/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <sstream>
#include <thread>

#include "common.h"

#include "commands/file_map.h"
#include "commands/changes_result.h"
#include "commands/describe_result.h"
#include "commands/filelog_result.h"
#include "commands/print_result.h"
#include "commands/users_result.h"
#include "commands/info_result.h"
#include "commands/client_result.h"
#include "commands/test_result.h"

/*
 * class P4LibrariesRAII can be used in the main function of the program
 * to initialize and destruct the P4 API in a RAII fashion.
 */
class P4LibrariesRAII
{
public:
	P4LibrariesRAII();
	~P4LibrariesRAII();
};

class P4API
{
private:
	// Helix Core C++ API doesn't seem to be fully thread-safe when creating a new
	// client with SSL, so let's initialize them in sequence.
	static std::mutex InitializationMutex;

	std::unique_ptr<ClientApi> m_ClientAPI;
	FileMap m_ClientMapping;
	int m_Usage = 0;

	bool Initialize();
	bool Deinitialize();
	bool Reinitialize();
	bool CheckErrors(Error& e);

	template <class T>
	T Run(const char* command, const std::vector<std::string>& stringArguments, const std::function<T()>& creatorFunc);
	template <class T>
	T RunEx(const char* command, const std::vector<std::string>& stringArguments, int commandRetries, const std::function<T()>& creatorFunc);
	void AddClientSpecView(const std::vector<std::string>& viewStrings);

public:
	static std::string P4PORT;
	static std::string P4USER;
	static std::string P4CLIENT;
	static ClientResult::ClientSpecData ClientSpec;
	static int CommandRetries;
	static int CommandRefreshThreshold;

	P4API();
	~P4API();

	bool IsDepotPathValid(const std::string& depotPath);
	bool IsDepotPathUnderClientSpec(const std::string& depotPath);

	TestResult TestConnection(int retries);
	ChangesResult Changes(const std::string& path, const std::string& from, int32_t maxCount);
	DescribeResult Describe(int cl);
	FileLogResult FileLog(int changelist);
	PrintResult PrintFiles(const std::vector<std::string>& fileRevisions, const std::function<void()>& onStat, const std::function<void(const char*, int)>& onOutput);
	ClientResult Client();
	UsersResult Users();
	InfoResult Info();
};

template <class T>
inline T P4API::RunEx(const char* command, const std::vector<std::string>& stringArguments, const int commandRetries, const std::function<T()>& creatorFunc)
{
	std::string argsString;
	for (const std::string& stringArg : stringArguments)
	{
		argsString.append(" ");
		argsString.append(stringArg);
	}

	std::vector<char*> argsCharArray;
	argsCharArray.reserve(stringArguments.size());
	for (const std::string& arg : stringArguments)
	{
		argsCharArray.push_back((char*)arg.c_str());
	}

	T clientUser = creatorFunc();

	m_ClientAPI->SetArgv(argsCharArray.size(), argsCharArray.data());
	m_ClientAPI->Run(command, &clientUser);

	int retries = commandRetries;
	while (m_ClientAPI->Dropped() || clientUser.GetError().IsError())
	{
		if (retries == 0)
		{
			break;
		}

		ERR("Connection dropped or command errored, retrying in 5 seconds.")
		std::this_thread::sleep_for(std::chrono::seconds(5));

		if (Reinitialize())
		{
			SUCCESS("Reinitialized P4API")
		}
		else
		{
			ERR("Could not reinitialize P4API")
		}

		WARN("Retrying: p4 " << command << argsString)

		clientUser = std::move(creatorFunc());

		m_ClientAPI->SetArgv(argsCharArray.size(), argsCharArray.data());
		m_ClientAPI->Run(command, &clientUser);

		retries--;
	}

	if (m_ClientAPI->Dropped() || clientUser.GetError().IsFatal())
	{
		Deinitialize();
		std::ostringstream oss;
		oss << "P4 client: Received errors even after retrying " << CommandRetries << " times";
		throw std::runtime_error(oss.str());
	}

	m_Usage++;
	if (m_Usage > CommandRefreshThreshold)
	{
		int refreshRetries = CommandRetries;
		while (refreshRetries > 0)
		{
			WARN("Trying to refresh the connection due to age (" << m_Usage << " > " << CommandRefreshThreshold << ").")
			if (Reinitialize())
			{
				SUCCESS("Connection was refreshed")
				break;
			}
			ERR("Could not refresh connection due to old age. Retrying in 5 seconds")
			std::this_thread::sleep_for(std::chrono::seconds(5));

			refreshRetries--;
		}

		if (refreshRetries == 0)
		{
			std::ostringstream oss;
			oss << "Could not refresh the connection after " << CommandRetries << " retries. Exiting.";
			throw std::runtime_error(oss.str());
		}
	}

	return clientUser;
}

template <class T>
inline T P4API::Run(const char* command, const std::vector<std::string>& stringArguments, const std::function<T()>& creatorFunc)
{
	return RunEx<T>(command, stringArguments, CommandRetries, creatorFunc);
}
