/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "git_api.h"

#include <cstring>
#include <cstdlib>

#include "git2.h"
#include "git2/sys/repository.h"
#include "minitrace.h"
#include "utils/std_helpers.h"

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

void GitAPI::SetActiveBranch(const std::string& branchName)
{
	if (branchName == m_CurrentBranch)
	{
		return;
	}

	int errorCode;
	// Look up the branch.
	git_reference *branch;
	errorCode = git_reference_lookup(&branch, m_Repo, ("refs/heads/" + branchName).c_str());
	if (errorCode != 0 && errorCode != GIT_ENOTFOUND)
	{
		GIT2(errorCode);
	}
	if (errorCode == GIT_ENOTFOUND)
	{
		// Create the branch from the first index.
		git_commit* tailCommit = nullptr;
		GIT2(git_commit_lookup(&tailCommit, m_Repo, &m_FirstCommit));
		GIT2(git_branch_create(&branch, m_Repo, branchName.c_str(), tailCommit, 0));
		git_commit_free(tailCommit);
	}

	// Make head point to the branch.
	git_reference *head;
	GIT2(git_reference_symbolic_create(&head, m_Repo, "HEAD", git_reference_name(branch), 1, branchName.c_str()));
	git_reference_free(head);
	git_reference_free(branch);

	m_CurrentBranch = branchName;
}

std::string GitAPI::CreateNoopMergeFromBranch(
		const std::string& sourceBranch,
	    const std::string& cl,
	    const std::string& user,
	    const std::string& email,
	    const int& timezone,
	    const int64_t& timestamp)
{
	MTR_SCOPE("Git", __func__);

	int errorCode;
	// Look up the head branch commit
	git_commit* branchCommit = nullptr;
	git_oid branchOid;
	errorCode = git_reference_name_to_id(&branchOid, m_Repo, ("refs/heads/" + sourceBranch).c_str());
	if (errorCode != 0 && errorCode != GIT_ENOTFOUND)
	{
		GIT2(errorCode);
	}
	if (errorCode == GIT_ENOTFOUND)
	{
		// Create the branch from the first index.
		GIT2(git_commit_lookup(&branchCommit, m_Repo, &m_FirstCommit));
		git_reference *branch;
		GIT2(git_branch_create(&branch, m_Repo, sourceBranch.c_str(), branchCommit, 0));
		git_reference_free(branch);
	}
	else
	{
		// Find the head commit.
		GIT2(git_commit_lookup(&branchCommit, m_Repo, &branchOid));
	}

	git_reference* ourRef;
	GIT2(git_repository_head(&ourRef, m_Repo));
	git_commit *ourCommit = nullptr;
	GIT2(git_reference_peel((git_object **)&ourCommit, ourRef, GIT_OBJECT_COMMIT));

	// Then make the commit with multiple parents.

	git_oid commitTreeID;
	GIT2(git_index_write_tree_to(&commitTreeID, m_Index, m_Repo));

	git_tree* commitTree = nullptr;
	GIT2(git_tree_lookup(&commitTree, m_Repo, &commitTreeID));

	git_signature* author = nullptr;
	GIT2(git_signature_new(&author, user.c_str(), email.c_str(), timestamp, timezone));

	git_oid commitOid;
	const git_commit *parents[] = {ourCommit, branchCommit};
	std::string commitMsg = cl + " - merge from " + sourceBranch;
	GIT2(git_commit_create(&commitOid, m_Repo, "HEAD", author, author, "UTF-8", commitMsg.c_str(), commitTree, 2, parents));

	// Clean up
	git_signature_free(author);
	git_tree_free(commitTree);
	git_commit_free(ourCommit);
	git_reference_free(ourRef);
	git_commit_free(branchCommit);
	return git_oid_tostr_s(&commitOid);
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
	MTR_SCOPE("Git", __func__);

	GIT2(git_repository_index(&m_Index, m_Repo));

	if (IsHEADExists())
	{
		git_oid oid_parent_commit = {};
		GIT2(git_reference_name_to_id(&oid_parent_commit, m_Repo, "HEAD"));

		git_commit* head_commit = nullptr;
		GIT2(git_commit_lookup(&head_commit, m_Repo, &oid_parent_commit));

		git_tree* head_commit_tree = nullptr;
		GIT2(git_commit_tree(&head_commit_tree, head_commit));

		GIT2(git_index_read_tree(m_Index, head_commit_tree));

		git_tree_free(head_commit_tree);
		git_commit_free(head_commit);

		// Find the first commit
		git_revwalk *walk;
		git_revwalk_new(&walk, m_Repo);
		git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL);
		git_revwalk_push_head(walk);
		git_revwalk_next(&m_FirstCommit, walk);

		WARN("Loaded index was refreshed to match the tree of the current HEAD commit");
	}
	else
	{
		// In order to have branches be mergable, even with no shared history, we perform
		// a trick by adding an empty commit as the very first commit, and use this as the base for all branches.
		// The time is set to the beginning of time.
		git_oid commitTreeID;
		GIT2(git_index_write_tree_to(&commitTreeID, m_Index, m_Repo));

		git_tree* commitTree = nullptr;
		GIT2(git_tree_lookup(&commitTree, m_Repo, &commitTreeID));

		git_signature* author = nullptr;
		GIT2(git_signature_new(&author, "No User", "no@user", 0, 0));

		git_reference* ref = nullptr;
		git_object* parent = nullptr;
		git_revparse_ext(&parent, &ref, m_Repo, "HEAD");

		GIT2(git_commit_create_v(&m_FirstCommit, m_Repo, "HEAD", author, author, "UTF-8", "Initial repository.", commitTree, parent ? 1 : 0, parent));

		git_object_free(parent);
		git_reference_free(ref);
		git_signature_free(author);
		git_tree_free(commitTree);

		WARN("No HEAD commit was found. Created a fresh index " << git_oid_tostr_s(&m_FirstCommit) << ".");
	}
}

void GitAPI::AddFileToIndex(const std::string& relativePath, const std::vector<char>& contents, const bool plusx)
{
	MTR_SCOPE("Git", __func__);

	git_index_entry entry = {};
	entry.mode = GIT_FILEMODE_BLOB;
	if (plusx)
	{
		entry.mode = GIT_FILEMODE_BLOB_EXECUTABLE; // 0100755
	}

	entry.path = relativePath.c_str();

	GIT2(git_index_add_from_buffer(m_Index, &entry, contents.data(), contents.size()));
}

void GitAPI::RemoveFileFromIndex(const std::string& relativePath)
{
	MTR_SCOPE("Git", __func__);

	GIT2(git_index_remove_bypath(m_Index, relativePath.c_str()));
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
	MTR_SCOPE("Git", __func__);

	git_oid commitTreeID;
	GIT2(git_index_write_tree_to(&commitTreeID, m_Index, m_Repo));

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
	// -3 to remove the trailing "..."
	std::string commitMsg = cl + " - " + desc + "\n[p4-fusion: depot-paths = \"" + depotPath.substr(0, depotPath.size() - 3) + "\": change = " + cl + "]";
	GIT2(git_commit_create_v(&commitID, m_Repo, "HEAD", author, author, "UTF-8", commitMsg.c_str(), commitTree, parent ? 1 : 0, parent));

	git_object_free(parent);
	git_reference_free(ref);
	git_signature_free(author);
	git_tree_free(commitTree);

	return git_oid_tostr_s(&commitID);
}

void GitAPI::CloseIndex()
{
	GIT2(git_index_write(m_Index));
	git_index_free(m_Index);
}
