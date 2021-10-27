/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "git_api.h"

#include <cstring>
#include <cstdlib>

#include "git2.h"
#include "git2/sys/repository.h"

#define GIT2(x)                                                                \
	do                                                                         \
	{                                                                          \
		int error = x;                                                         \
		if (error < 0)                                                         \
		{                                                                      \
			const git_error* e = git_error_last();                             \
			ERR("GitAPI: " << error << ":" << e->klass << ": " << e->message); \
			exit(error);                                                       \
		}                                                                      \
	} while (false)

GitAPI::GitAPI(bool fsyncEnable)
{
	git_libgit2_init();
	GIT2(git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR, (int)fsyncEnable));
}

GitAPI::~GitAPI()
{
	if (m_Repo)
	{
		git_repository_free(m_Repo);
		m_Repo = nullptr;
	}
	git_libgit2_shutdown();
}

bool GitAPI::IsRepositoryClonedFrom(const std::string& depotPath)
{
	git_oid oid;
	GIT2(git_reference_name_to_id(&oid, m_Repo, "HEAD"));

	git_commit* headCommit = nullptr;
	GIT2(git_commit_lookup(&headCommit, m_Repo, &oid));

	std::string message = git_commit_message(headCommit);
	size_t depotPathStart = message.find("depot-paths = \"") + 15;
	size_t depotPathEnd = message.find("\": change") - 1;
	std::string repoDepotPath = message.substr(depotPathStart, depotPathEnd - depotPathStart + 1) + "...";

	git_commit_free(headCommit);

	return repoDepotPath == depotPath;
}

void GitAPI::OpenRepository(const std::string& repoPath)
{
	GIT2(git_repository_open(&m_Repo, repoPath.c_str()));
}

bool GitAPI::InitializeRepository(const std::string& srcPath)
{
	GIT2(git_repository_init(&m_Repo, srcPath.c_str(), true));
	SUCCESS("Initialized Git repository at " << srcPath);

	return true;
}

bool GitAPI::IsHEADExists()
{
	git_oid oid;
	int errorCode = git_reference_name_to_id(&oid, m_Repo, "HEAD");
	if (errorCode && errorCode != GIT_ENOTFOUND)
	{
		GIT2(errorCode);
	}
	return errorCode == 0;
}

git_oid GitAPI::CreateBlob(const std::vector<char>& data)
{
	git_oid oid;
	GIT2(git_blob_create_from_buffer(&oid, m_Repo, data.data(), data.size()));
	return oid;
}

std::string GitAPI::DetectLatestCL()
{
	git_oid oid;
	GIT2(git_reference_name_to_id(&oid, m_Repo, "HEAD"));

	git_commit* headCommit = nullptr;
	GIT2(git_commit_lookup(&headCommit, m_Repo, &oid));

	std::string message = git_commit_message(headCommit);
	size_t clStart = message.find_last_of("change = ") + 1;
	std::string cl(message.begin() + clStart, message.end() - 1);

	git_commit_free(headCommit);

	return cl;
}

void GitAPI::CreateIndex()
{
	GIT2(git_repository_index(&m_Index, m_Repo));
}

void GitAPI::AddFileToIndex(const std::string& depotFile, const std::vector<char>& contents)
{
	git_index_entry entry;
	memset(&entry, 0, sizeof(entry));
	entry.mode = GIT_FILEMODE_BLOB;
	entry.path = depotFile.c_str() + 2; // +2 to skip the "//" in depot path

	GIT2(git_index_add_from_buffer(m_Index, &entry, contents.data(), contents.size()));
}

void GitAPI::RemoveFileFromIndex(const std::string& depotFile)
{
	GIT2(git_index_remove_bypath(m_Index, depotFile.c_str() + 2)); // +2 to skip the "//" in depot path
}

std::string GitAPI::Commit(
    const std::string& depotPath,
    const std::string& cl,
    const std::string& user,
    const std::string& email,
    const int& timezone,
    const std::string& desc,
    const int64_t& timestamp)
{
	git_oid commitTreeID;
	GIT2(git_index_write_tree_to(&commitTreeID, m_Index, m_Repo));
	GIT2(git_index_write(m_Index));

	git_tree* commitTree = nullptr;
	GIT2(git_tree_lookup(&commitTree, m_Repo, &commitTreeID));

	git_signature* author = nullptr;
	GIT2(git_signature_new(&author, user.c_str(), email.c_str(), timestamp, timezone));

	git_reference* ref = nullptr;
	git_object* parent = nullptr;
	{
		int error = git_revparse_ext(&parent, &ref, m_Repo, "HEAD");
		if (error == GIT_ENOTFOUND)
		{
			WARN("GitAPI: HEAD not found. Creating first commit");
		}
	}

	git_oid commitID;
	std::string commitMsg = desc + "\n[p4-fusion: depot-paths = \"" + depotPath.substr(0, depotPath.size() - 3) + "\": change = " + cl + "]";
	GIT2(git_commit_create_v(&commitID, m_Repo, "HEAD", author, author, "UTF-8", commitMsg.c_str(), commitTree, parent ? 1 : 0, parent));

	git_object_free(parent);
	git_reference_free(ref);
	git_signature_free(author);
	git_tree_free(commitTree);

	return git_oid_tostr_s(&commitID);
}

void GitAPI::CloseIndex()
{
	git_index_free(m_Index);
}
