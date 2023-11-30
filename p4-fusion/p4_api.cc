/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "p4_api.h"
#include <csignal>
#include "utils/std_helpers.h"
#include "p4/p4libs.h"
#include "p4/signaler.h"
#include "minitrace.h"

ClientResult::ClientSpecData P4API::ClientSpec;
std::string P4API::P4PORT;
std::string P4API::P4USER;
std::string P4API::P4CLIENT;
int P4API::CommandRetries = 1;
int P4API::CommandRefreshThreshold = 1;
std::mutex P4API::InitializationMutex;

P4LibrariesRAII::P4LibrariesRAII()
{
	Error e;
	P4Libraries::Initialize(P4LIBRARIES_INIT_ALL, &e);
	if (e.Test())
	{
		StrBuf msg;
		e.Fmt(&msg);
		ERR(msg.Text())
		throw std::runtime_error("failed to initialize P4Libraries");
	}

	// We disable the default signaler to stop it from deleting memory from the wrong heap
	// https://www.perforce.com/manuals/p4api/Content/P4API/chapter.clientprogramming.signaler.html
	std::signal(SIGINT, SIG_DFL);
	signaler.Disable();

	SUCCESS("Initialized P4Libraries successfully")
}

P4LibrariesRAII::~P4LibrariesRAII()
{
	Error e;
	P4Libraries::Shutdown(P4LIBRARIES_INIT_ALL, &e);
	if (e.Test())
	{
		StrBuf msg;
		e.Fmt(&msg);
		ERR("Failed to shut down P4Libraries gracefully")
		ERR(msg.Text())
		return;
	}
	SUCCESS("Shut down P4Libraries successfully")
}

P4API::P4API()
{
	{
		// Acquire InitializationMutex lock to ensure thread-safe initialization:
		std::lock_guard<std::mutex> lock(P4API::InitializationMutex);
		m_ClientAPI = std::make_unique<ClientApi>();
	}

	if (!Initialize())
	{
		ERR("Could not initialize P4API")
		throw std::runtime_error("Could not initialize P4API");
	}

	AddClientSpecView(ClientSpec.mapping);
}

bool P4API::Initialize()
{
	MTR_SCOPE("P4", __func__);

	// Acquire InitializationMutex lock to ensure thread-safe initialization:
	std::lock_guard<std::mutex> lock(P4API::InitializationMutex);

	m_Usage = 0;

	m_ClientAPI->SetPort(P4PORT.c_str());
	m_ClientAPI->SetUser(P4USER.c_str());
	m_ClientAPI->SetClient(P4CLIENT.c_str());
	m_ClientAPI->SetProtocol("tag", "");

	Error e;
	m_ClientAPI->Init(&e);

	if (!CheckErrors(e))
	{
		ERR("Could not initialize Helix Core C/C++ API")
		return false;
	}

	return true;
}

bool P4API::Deinitialize()
{
	Error e;
	m_ClientAPI->Final(&e);
	return CheckErrors(e);
}

bool P4API::Reinitialize()
{
	MTR_SCOPE("P4", __func__);

	bool status = Deinitialize() && Initialize();
	return status;
}

P4API::~P4API()
{
	if (!Deinitialize())
	{
		ERR("P4API context was not destroyed successfully")
	}
}

bool P4API::IsDepotPathValid(const std::string& depotPath)
{
	return STDHelpers::EndsWith(depotPath, "/...") && STDHelpers::StartsWith(depotPath, "//");
}

bool P4API::IsDepotPathUnderClientSpec(const std::string& depotPath)
{
	return m_ClientMapping.IsInLeft(depotPath);
}

bool P4API::CheckErrors(Error& e)
{
	if (e.Test())
	{
		StrBuf msg;
		e.Fmt(&msg);
		ERR(msg.Text())
		return false;
	}
	return true;
}

void P4API::AddClientSpecView(const std::vector<std::string>& viewStrings)
{
	m_ClientMapping.InsertTranslationMapping(viewStrings);
}

ClientResult P4API::Client()
{
	return Run<ClientResult>("client", { "-o" }, []() -> ClientResult
	    { return {}; });
}

TestResult P4API::TestConnection(const int retries)
{
	return RunEx<TestResult>("changes", { "-m", "1", "//..." }, retries, []() -> TestResult
	    { return {}; });
}

ChangesResult P4API::Changes(const std::string& path, const std::string& from, int32_t maxCount)
{
	std::vector<std::string> args = {
		"-l", // Get full descriptions instead of sending cut-short ones
		"-s", "submitted", // Only include submitted CLs
		"-r" // Send CLs in chronological order
	};

	// This needs to be declared outside the if scope below to
	// keep the internal character array alive till the p4 call is made
	std::string maxCountStr;
	if (maxCount != -1)
	{
		maxCountStr = std::to_string(maxCount);

		args.emplace_back("-m"); // Only send max this many number of CLs
		args.push_back(maxCountStr);
	}

	std::string pathAddition;
	if (!from.empty())
	{
		// Appending "@CL_NUMBER,@now" seems to include the current CL,
		// which makes this awkward to deal with in general. So instead,
		// we append "@>CL_NUMBER" so that we only receive the CLs after
		// the current one.
		pathAddition = "@>" + from;
	}

	args.push_back(path + pathAddition);

	return Run<ChangesResult>("changes", args, []() -> ChangesResult
	    { return {}; });
}

DescribeResult P4API::Describe(const int cl)
{
	MTR_SCOPE("P4", __func__);

	return Run<DescribeResult>("describe", { "-s", // Omit the diffs
	                                           std::to_string(cl) },
	    []() -> DescribeResult
	    { return {}; });
}

FileLogResult P4API::FileLog(const int changelist)
{
	return Run<FileLogResult>("filelog", {
	                                         "-c", // restrict output to a single changelist
	                                         std::to_string(changelist),
	                                         "-m1", // don't get the full history, just the first entry.
	                                         "//..." // rather than require the path to be passed in, just list all files.
	                                     },
	    []() -> FileLogResult
	    { return {}; });
}

PrintResult P4API::PrintFiles(const std::vector<std::string>& fileRevisions, const std::function<void()>& onStat, const std::function<void(const char*, int)>& onOutput)
{
	MTR_SCOPE("P4", __func__);

	if (fileRevisions.empty())
	{
		return { onStat, onOutput };
	}

	return Run<PrintResult>("print", fileRevisions, [&onStat, &onOutput]() -> PrintResult
	    { return { onStat, onOutput }; });
}

UsersResult P4API::Users()
{
	return Run<UsersResult>("users", {
	                                     "-a" // Include service accounts
	                                 },
	    []() -> UsersResult
	    { return {}; });
}

InfoResult P4API::Info()
{
	return Run<InfoResult>("info", {}, []() -> InfoResult
	    { return {}; });
}
