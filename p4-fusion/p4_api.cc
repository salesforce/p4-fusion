/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "p4_api.h"

#include "p4/p4libs.h"
#include "minitrace.h"

ClientResult::ClientSpecData P4API::ClientSpec;
std::string P4API::P4PORT;
std::string P4API::P4USER;
std::string P4API::P4CLIENT;
int P4API::CommandRetries = 1;
int P4API::CommandRefreshThreshold = 1;
std::mutex P4API::ReinitializationMutex;

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
	Error e;
	StrBuf msg;

	m_ClientAPI.Final(&e);
	CheckErrors(e, msg);
	return true;
}

bool P4API::Reinitialize()
{
	MTR_SCOPE("P4", __func__);

	// Helix Core C++ API seems to crash while making connections parallely.
	// The Initialize() function is immune to this because it is never run in
	// parrallel, while Reinitialize() function can get in a situation where it is
	// called in parallel.
	std::unique_lock<std::mutex> lock(ReinitializationMutex);
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
	return depotPath.substr(depotPath.size() - 4, 4) == "/..." && depotPath.substr(0, 2) == "//";
}

bool P4API::IsFileUnderDepotPath(const std::string& fileRevision, const std::string& depotPath)
{
	return fileRevision.substr(0, depotPath.size() - 4) + "/..." == depotPath;
}

bool P4API::IsFileUnderClientSpec(const std::string& fileRevision)
{
	StrBuf to;
	StrBuf from(fileRevision.c_str());
	return m_ClientMapping.Translate(from, to);
}

bool P4API::IsDeleted(const std::string& action)
{
	return action.find("delete") != std::string::npos;
}

bool P4API::IsBinary(const std::string& fileType)
{
	return fileType.find("binary") != std::string::npos;
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
	for (int i = 0; i < viewStrings.size(); i++)
	{
		const std::string& view = viewStrings.at(i);

		bool modification = view.front() != '/';

		MapType mapType = MapType::MapInclude;
		switch (view.front())
		{
		case '+':
			mapType = MapType::MapOverlay;
			break;
		case '-':
			mapType = MapType::MapExclude;
			break;
		case '&':
			mapType = MapType::MapOneToMany;
			break;
		}

		// Skip the first few characters to only match with the right half.
		size_t right = view.find("//", 3);
		if (right == std::string::npos)
		{
			WARN("Found a one-sided client mapping, ignoring...");
			continue;
		}

		std::string mapStrLeft = view.substr(0, right).c_str() + modification;
		mapStrLeft.erase(mapStrLeft.find_last_not_of(' ') + 1);
		mapStrLeft.erase(0, mapStrLeft.find_first_not_of(' '));

		std::string mapStrRight = view.substr(right).c_str();
		mapStrRight.erase(mapStrRight.find_last_not_of(' ') + 1);
		mapStrRight.erase(0, mapStrRight.find_first_not_of(' '));

		m_ClientMapping.Insert(StrBuf(mapStrLeft.c_str()), StrBuf(mapStrRight.c_str()), mapType);
	}
}

void P4API::UpdateClientSpec()
{
	Run<Result>("client", {});
}

ClientResult P4API::Client()
{
	return Run<ClientResult>("client", { "-o" });
}

ChangesResult P4API::ShortChanges(const std::string& path)
{
	return Run<ChangesResult>("changes", {
	                                         "-r", // Get CLs from earliest to latest
	                                         "-s", "submitted", // Only include submitted CLs
	                                         path // Depot path to get CLs from
	                                     });
}

ChangesResult P4API::Changes(const std::string& path)
{
	MTR_SCOPE("P4", __func__);
	return Run<ChangesResult>("changes", {
	                                         "-l", // Get full descriptions instead of sending cut-short ones
	                                         "-s", "submitted", // Only include submitted CLs
	                                         path // Depot path to get CLs from
	                                     });
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

	ChangesResult result = Run<ChangesResult>("changes", args);

	return result;
}

ChangesResult P4API::ChangesFromTo(const std::string& path, const std::string& from, const std::string& to)
{
	std::string pathArg = path + "@" + from + "," + to;
	return Run<ChangesResult>("changes", {
	                                         "-s", "submitted", // Only include submitted CLs
	                                         pathArg // Depot path to get CLs from
	                                     });
}

ChangesResult P4API::LatestChange(const std::string& path)
{
	MTR_SCOPE("P4", __func__);
	return Run<ChangesResult>("changes", {
	                                         "-s", "submitted", // Only include submitted CLs,
	                                         "-m", "1", // Get top-most change
	                                         path // Depot path to get CLs from
	                                     });
}

ChangesResult P4API::OldestChange(const std::string& path)
{
	return Run<ChangesResult>("changes", {
	                                         "-s", "submitted", // Only include submitted CLs,
	                                         "-r", // List from earliest to latest
	                                         "-m", "1", // Get top-most change
	                                         path // Depot path to get CLs from
	                                     });
}

DescribeResult P4API::Describe(const std::string& cl)
{
	MTR_SCOPE("P4", __func__);
	return Run<DescribeResult>("describe", { "-s", // Omit the diffs
	                                           cl });
}

FilesResult P4API::Files(const std::string& path)
{
	return Run<FilesResult>("files", { path });
}

SizesResult P4API::Size(const std::string& file)
{
	return Run<SizesResult>("sizes", { "-a", "-s", file });
}

Result P4API::Sync()
{
	return Run<Result>("sync", {});
}

SyncResult P4API::GetFilesToSyncAtCL(const std::string& path, const std::string& cl)
{
	std::string clCommand = "@" + cl;
	return Run<SyncResult>("sync", {
	                                   "-n", // Only preview the files to sync. Don't send file contents...yet
	                                   clCommand,
	                               });
}

PrintResult P4API::PrintFile(const std::string& filePathRevision)
{
	return Run<PrintResult>("print", {
	                                     filePathRevision,
	                                 });
}

PrintResult P4API::PrintFiles(const std::vector<std::string>& fileRevisions)
{
	MTR_SCOPE("P4", __func__);

	if (fileRevisions.empty())
	{
		return PrintResult();
	}

	return Run<PrintResult>("print", fileRevisions);
}

Result P4API::Sync(const std::string& path)
{
	return Run<Result>("sync", {
	                               path // Sync a particular depot path
	                           });
}

UsersResult P4API::Users()
{
	return Run<UsersResult>("users", {
	                                     "-a" // Include service accounts
	                                 });
}

InfoResult P4API::Info()
{
	return Run<InfoResult>("info", {});
}
