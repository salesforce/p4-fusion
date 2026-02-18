/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "p4_api.h"

#include <csignal>
#include <memory>

#include "commands/stream_result.h"
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

P4API::P4API()
{
	if (!Initialize())
	{
		ERR("Could not initialize P4API");
		return;
	}

	AddClientSpecView(ClientSpec.mapping);
}

bool P4API::Initialize()
{
	MTR_SCOPE("P4", __func__);

	// Helix Core C++ API seems to crash while making connections parallely.
	std::unique_lock<std::mutex> lock(InitializationMutex);

	Error e;
	StrBuf msg;

	m_Usage = 0;
	m_ClientAPI.SetPort(P4PORT.c_str());
	m_ClientAPI.SetUser(P4USER.c_str());
	m_ClientAPI.SetClient(P4CLIENT.c_str());
	m_ClientAPI.SetProtocol("tag", "");
	m_ClientAPI.Init(&e);

	if (!CheckErrors(e, msg))
	{
		ERR("Could not initialize Helix Core C/C++ API");
		return false;
	}

	return true;
}

bool P4API::Deinitialize()
{
	std::unique_lock<std::mutex> lock(InitializationMutex);

	Error e;
	StrBuf msg;

	m_ClientAPI.Final(&e);
	CheckErrors(e, msg);

	return true;
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
		ERR("P4API context was not destroyed successfully");
	}
}

bool P4API::IsDepotPathValid(const std::string& depotPath)
{
	return STDHelpers::EndsWith(depotPath, "/...") && STDHelpers::StartsWith(depotPath, "//");
}

bool P4API::IsFileUnderDepotPath(const std::string& fileRevision, const std::string& depotPath)
{
	return STDHelpers::Contains(fileRevision, depotPath.substr(0, depotPath.size() - 3)); // -3 to remove the trailing "..."
}

bool P4API::IsDepotPathUnderClientSpec(const std::string& depotPath)
{
	return m_ClientMapping.IsInLeft(depotPath);
}

bool P4API::IsFileUnderClientSpec(const std::string& fileRevision)
{
	return m_ClientMapping.IsInRight(fileRevision);
}

bool P4API::CheckErrors(Error& e, StrBuf& msg)
{
	if (e.Test())
	{
		e.Fmt(&msg);
		ERR(msg.Text());
		return false;
	}
	return true;
}

bool P4API::InitializeLibraries()
{
	Error e;
	StrBuf msg;
	P4Libraries::Initialize(P4LIBRARIES_INIT_ALL, &e);
	if (e.Test())
	{
		e.Fmt(&msg);
		ERR(msg.Text());
		ERR("Failed to initialize P4Libraries");
		return false;
	}

	// We disable the default signaler to stop it from deleting memory from the wrong heap
	// https://www.perforce.com/manuals/p4api/Content/P4API/chapter.clientprogramming.signaler.html
	std::signal(SIGINT, SIG_DFL);
	signaler.Disable();

	SUCCESS("Initialized P4Libraries successfully");
	return true;
}

bool P4API::ShutdownLibraries()
{
	Error e;
	StrBuf msg;
	P4Libraries::Shutdown(P4LIBRARIES_INIT_ALL, &e);
	if (e.Test())
	{
		e.Fmt(&msg);
		ERR(msg.Text());
		return false;
	}
	return true;
}

void P4API::AddClientSpecView(const std::vector<std::string>& viewStrings)
{
	m_ClientMapping.InsertTranslationMapping(viewStrings);
}

void P4API::UpdateClientSpec()
{
	Run<Result>("client", {});
}

std::unique_ptr<ClientResult> P4API::Client()
{
	return Run<ClientResult>("client", { "-o" });
}

std::unique_ptr<StreamResult> P4API::Stream(const std::string& path)
{
	return Run<StreamResult>("stream", { "-o", path });
}

std::unique_ptr<TestResult> P4API::TestConnection(const int retries)
{
	return RunEx<TestResult>("changes", { "-m", "1", "//..." }, retries);
}

std::unique_ptr<ChangesResult> P4API::ShortChanges(const std::string& path)
{
	std::unique_ptr<ChangesResult> changes = Run<ChangesResult>("changes", {
	                                                          "-s", "submitted", // Only include submitted CLs
	                                                          path // Depot path to get CLs from
	                                                      });
	changes->reverse();
	return changes;
}

std::unique_ptr<ChangesResult> P4API::Changes(const std::string& path)
{
	MTR_SCOPE("P4", __func__);
	return Run<ChangesResult>("changes", {
	                                         "-l", // Get full descriptions instead of sending cut-short ones
	                                         "-s", "submitted", // Only include submitted CLs
	                                         path // Depot path to get CLs from
	                                     });
}

std::unique_ptr<ChangesResult> P4API::Changes(const std::string& path, const std::string& from, int32_t maxCount)
{
	std::vector<std::string> args = {
		"-l", // Get full descriptions instead of sending cut-short ones
		"-s", "submitted", // Only include submitted CLs
	};

	// This needs to be declared outside the if scope below to
	// keep the internal character array alive till the p4 call is made
	std::string maxCountStr;
	if (maxCount != -1)
	{
		maxCountStr = std::to_string(maxCount);

		args.push_back("-m"); // Only send max this many number of CLs
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

	std::unique_ptr<ChangesResult> result = Run<ChangesResult>("changes", args);
	result->reverse();
	return result;
}

std::unique_ptr<ChangesResult> P4API::ChangesFromTo(const std::string& path, const std::string& from, const std::string& to)
{
	std::string pathArg = path + "@" + from + "," + to;
	return Run<ChangesResult>("changes", {
	                                         "-s", "submitted", // Only include submitted CLs
	                                         pathArg // Depot path to get CLs from
	                                     });
}

std::unique_ptr<ChangesResult> P4API::LatestChange(const std::string& path)
{
	MTR_SCOPE("P4", __func__);
	return Run<ChangesResult>("changes", {
	                                         "-s", "submitted", // Only include submitted CLs,
	                                         "-m", "1", // Get top-most change
	                                         path // Depot path to get CLs from
	                                     });
}

std::unique_ptr<ChangesResult> P4API::OldestChange(const std::string& path)
{
	std::unique_ptr<ChangesResult> changes = Run<ChangesResult>("changes", {
	                                                          "-s", "submitted", // Only include submitted CLs,
	                                                          "-m", "1", // Get top-most change
	                                                          path // Depot path to get CLs from
	                                                      });
	changes->reverse();
	return changes;
}

std::unique_ptr<DescribeResult> P4API::Describe(const std::string& cl)
{
	MTR_SCOPE("P4", __func__);
	return Run<DescribeResult>("describe", { "-s", // Omit the diffs
	                                           cl });
}

std::unique_ptr<FileLogResult> P4API::FileLog(const std::string& changelist)
{
	return Run<FileLogResult>("filelog", {
	                                         "-c", // restrict output to a single changelist
	                                         changelist,
	                                         "-m1", // don't get the full history, just the first entry.
	                                         "//..." // rather than require the path to be passed in, just list all files.
	                                     });
}

std::unique_ptr<SizesResult> P4API::Size(const std::string& file)
{
	return Run<SizesResult>("sizes", { "-a", "-s", file });
}

std::unique_ptr<Result> P4API::Sync()
{
	return Run<Result>("sync", {});
}

std::unique_ptr<SyncResult> P4API::GetFilesToSyncAtCL(const std::string& path, const std::string& cl)
{
	std::string clCommand = "@" + cl;
	return Run<SyncResult>("sync", {
	                                   "-n", // Only preview the files to sync. Don't send file contents...yet
	                                   clCommand,
	                               });
}

std::unique_ptr<PrintResult> P4API::PrintFile(const std::string& filePathRevision)
{
	return Run<PrintResult>("print", {
	                                     filePathRevision,
	                                 });
}

std::unique_ptr<PrintResult> P4API::PrintFiles(const std::vector<std::string>& fileRevisions)
{
	MTR_SCOPE("P4", __func__);

	if (fileRevisions.empty())
	{
		return std::unique_ptr<PrintResult>(new PrintResult());
	}

	return Run<PrintResult>("print", fileRevisions);
}

std::unique_ptr<Result> P4API::Sync(const std::string& path)
{
	return Run<Result>("sync", {
	                               path // Sync a particular depot path
	                           });
}

std::unique_ptr<UsersResult> P4API::Users()
{
	return Run<UsersResult>("users", {
	                                     "-a" // Include service accounts
	                                 });
}

std::unique_ptr<InfoResult> P4API::Info()
{
	return Run<InfoResult>("info", {});
}
