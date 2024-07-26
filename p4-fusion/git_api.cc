/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "git_api.h"

#include <sstream>

#include "git2.h"
#include "minitrace.h"
#include "labels_conversion.h"
#include "utils/std_helpers.h"

void checkGit2Error(int errcode)
{
	if (errcode < 0)
	{
		const git_error* e = git_error_last();
		std::ostringstream oss;
		oss << "GitAPI failed: " << errcode << ":" << e->klass << ": " << e->message;
		throw std::runtime_error(oss.str());
	}
}

Libgit2RAII::Libgit2RAII(int fsyncEnable)
{
	checkGit2Error(git_libgit2_init());
	checkGit2Error(git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR, (int)fsyncEnable));

	SUCCESS("Initialized libgit2 successfully")
}

Libgit2RAII::~Libgit2RAII()
{
	auto errcode = git_libgit2_shutdown();
	if (errcode < 0)
	{
		const git_error* e = git_error_last();
		ERR("Failed to shut down libgit2: " << errcode << ":" << e->klass << ": " << e->message)

		return;
	}
	SUCCESS("Shut down libgit2 successfully")
}

GitAPI::GitAPI(std::string _repoPath, const int tz)
    : timezoneMinutes(tz)
    , repoPath(std::move(_repoPath))
    , m_Repo(nullptr)
{
}

GitAPI::~GitAPI()
{
	std::unordered_map<std::string, git_index*>::iterator it;
	for (it = lastBranchTree.begin(); it != lastBranchTree.end(); it++)
	{
		git_index_free(it->second);
	}
	lastBranchTree.clear();

	if (m_Repo)
	{
		git_repository_free(m_Repo);
		m_Repo = nullptr;
	}
}

bool GitAPI::IsRepositoryClonedFrom(const std::string& depotPath) const
{
	// First, resolve the HEAD symbolic ref to a ref.
	git_oid oid;
	checkGit2Error(git_reference_name_to_id(&oid, m_Repo, "HEAD"));

	// Next, find the commit the ref is pointing at.
	git_commit* headCommit = nullptr;
	checkGit2Error(git_commit_lookup(&headCommit, m_Repo, &oid));

	std::string message = git_commit_message(headCommit);
	size_t depotPathStart = message.rfind("depot-paths = \"") + 15;
	size_t depotPathEnd = message.find("\": change", depotPathStart) - 1;
	std::string repoDepotPath = message.substr(depotPathStart, depotPathEnd - depotPathStart + 1) + "...";

	git_commit_free(headCommit);

	return repoDepotPath == depotPath;
}

// Concurrent calls to git_repository_open_bare cause a data race that helgrind complains
// about so we guard against that.
std::mutex GitAPI::repoMutex;

void GitAPI::OpenRepository()
{
	std::lock_guard<std::mutex> lock(repoMutex);
	checkGit2Error(git_repository_open_bare(&m_Repo, repoPath.c_str()));
}

void GitAPI::InitializeRepository(const bool noCreateBaseCommit)
{
	std::lock_guard<std::mutex> lock(repoMutex);

	if (git_repository_open_bare(&m_Repo, repoPath.c_str()) < 0)
	{
		git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;
		opts.flags = GIT_REPOSITORY_INIT_MKPATH | GIT_REPOSITORY_INIT_BARE;
		opts.initial_head = "main";

		checkGit2Error(git_repository_init_ext(&m_Repo, repoPath.c_str(), &opts));
		SUCCESS("Initialized Git repository at " << repoPath)
	}
	else
	{
		SUCCESS("Opened existing Git repository at " << repoPath)
	}

	// Now that we have a bare repo (potentially empty), we can go ahead and determine
	// our common merge commit.
	// If HEAD exists, we use the root commit of the branch HEAD is pointing to.
	if (IsHEADExists())
	{
		PRINT("Finding root commit for HEAD")

		// Walk the graph to find the root commit of the HEAD branch.
		git_revwalk* walk;
		checkGit2Error(git_revwalk_new(&walk, m_Repo));
		checkGit2Error(git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL));
		checkGit2Error(git_revwalk_push_head(walk));
		while (git_revwalk_next(&m_FirstCommitOid, walk) != GIT_ITEROVER)
		{
		}
		git_revwalk_free(walk);

		SUCCESS("Starting from commit " << git_oid_tostr_s(&m_FirstCommitOid))
	}
	else if (!noCreateBaseCommit)
	{
		// In order to have branches be mergable, even with no shared history, we perform
		// a trick by adding an empty commit as the very first commit, and use this as the base for all branches.
		// The time is set to the beginning of time.
		git_index* idx;
		checkGit2Error(git_index_new(&idx));
		// Write an empty tree, so we can create an empty commit.
		git_oid commitTreeID;
		checkGit2Error(git_index_write_tree_to(&commitTreeID, idx, m_Repo));

		git_tree* commitTree = nullptr;
		checkGit2Error(git_tree_lookup(&commitTree, m_Repo, &commitTreeID));

		git_signature* author = nullptr;
		checkGit2Error(git_signature_new(&author, "No User", "no@user", 0, 0));

		checkGit2Error(git_commit_create_v(&m_FirstCommitOid, m_Repo, "HEAD", author, author, "UTF-8", "Initial repository.", commitTree, 0, nullptr));

		git_signature_free(author);
		git_tree_free(commitTree);
		git_index_free(idx);

		WARN("No HEAD commit was found. Created fresh index " << git_oid_tostr_s(&m_FirstCommitOid) << ".")
	}
	else
	{
		// Otherwise, the first commit is empty. This means that we have to create
		// a root commit for the first commit we create on a branch.
		// This empty oid will pass the git_oid_is_zero check.
		m_FirstCommitOid = git_oid {};
	}
}

bool GitAPI::IsHEADExists() const
{
	MTR_SCOPE("Git", __func__);

	git_oid oid;
	int errorCode = git_reference_name_to_id(&oid, m_Repo, "HEAD");
	if (errorCode && errorCode != GIT_ENOTFOUND)
	{
		checkGit2Error(errorCode);
	}
	return errorCode != GIT_ENOTFOUND;
}

std::string GitAPI::DetectLatestCL() const
{
	MTR_SCOPE("Git", __func__);

	// Resolve HEAD to a reference.
	git_oid oid;
	checkGit2Error(git_reference_name_to_id(&oid, m_Repo, "HEAD"));

	// Next, get the commit that the HEAD reference is pointing at.
	git_commit* headCommit = nullptr;
	checkGit2Error(git_commit_lookup(&headCommit, m_Repo, &oid));

	// Look for the specific change message generated from the Commit method.
	// Note that extra branching information can be added after it.
	// ": change = " is 11 characters long.
	std::string message = git_commit_message(headCommit);
	size_t pos = message.find(": change = ");
	if (pos == std::string::npos)
	{
		throw std::invalid_argument("failed to parse commit message, well known section : change =  not found");
	}
	// The changelist number starts after 11 characters, the length of ": change = ".
	size_t clStart = pos + 11;
	size_t clEnd = message.find(']', clStart);
	if (clEnd == std::string::npos)
	{
		throw std::invalid_argument("failed to parse commit message, well closing ] not found");
	}

	std::string cl(message, clStart, clEnd - clStart);

	git_commit_free(headCommit);

	return cl;
}

std::string GitAPI::WriteChangelistBranch(
    const std::string& depotPath,
    const ChangeList& cl,
    std::vector<FileData>& files,
    const std::string& targetBranch,
    const std::string& authorName,
    const std::string& authorEmail,
    const std::string& mergeFrom)
{
	MTR_SCOPE("Git", __func__);

	std::string targetBranchRef = "HEAD";
	git_index* idx;

	if (lastBranchTree.find(targetBranch) != lastBranchTree.end())
	{
		idx = lastBranchTree[targetBranch];
	}
	else
	{
		// Branch not seen yet, we need to load the index into memory.
		if (!targetBranch.empty())
		{
			// targetBranchRef = "refs/heads/" + targetBranch;
			// // Look up the branch.
			// git_reference* branch;
			// int errorCode = git_reference_lookup(&branch, m_Repo, targetBranchRef.c_str());
			// if (errorCode != 0 && errorCode != GIT_ENOTFOUND)
			// {
			// 	// Unexpected error.
			// 	checkGit2Error(errorCode);
			// }
			// if (errorCode == GIT_ENOTFOUND)
			// {
			// 	// Ref doesn't exist yet, so we need to create a new branch. If the
			// 	// no branch option is set, we create a new root commit, otherwise
			// 	// we branch off of the known last commit.
			// 	// If this is a new export, the last known commit is the empty commit
			// 	// we create to have a common merge-base.
			// 	// Otherwise, it is the commit HEAD was pointing at when the process
			// 	// started.

			// 	// Create the branch from the first index.
			// 	git_commit* firstCommit = nullptr;
			// 	// TODO: m_FirstCommitOid can be empty if we didn't create a base commit.
			// 	// In that case, we should probably create a root commit?
			// 	checkGit2Error(git_commit_lookup(&firstCommit, m_Repo, &m_FirstCommitOid));
			// 	checkGit2Error(git_branch_create(&branch, m_Repo, branchName.c_str(), firstCommit, 0));
			// 	git_commit_free(firstCommit);
			// }

			// targetBranchRef = git_reference_name(branch);

			// // Make head point to the branch.
			// git_reference* head;
			// checkGit2Error(git_reference_symbolic_create(&head, m_Repo, "HEAD", git_reference_name(branch), 1, branchName.c_str()));
			// git_reference_free(head);
			// git_reference_free(branch);

			// // Now update the files in the index to match the content of the commit pointed at by HEAD.
			// git_oid oidParentCommit;
			// checkGit2Error(git_reference_name_to_id(&oidParentCommit, m_Repo, "HEAD"));

			// git_commit* headCommit = nullptr;
			// checkGit2Error(git_commit_lookup(&headCommit, m_Repo, &oidParentCommit));

			// git_tree* headCommitTree = nullptr;
			// checkGit2Error(git_commit_tree(&headCommitTree, headCommit));

			// checkGit2Error(git_index_read_tree(m_Index, headCommitTree));

			// git_tree_free(headCommitTree);
			// git_commit_free(headCommit);
		}
		else
		{
			// Create a new in-memory index for current HEAD.
			checkGit2Error(git_index_new(&idx));
			git_oid headCommitSHA;
			int exitCode = git_reference_name_to_id(&headCommitSHA, m_Repo, "HEAD");
			if (exitCode != 0 && exitCode != GIT_ENOTFOUND)
			{
				checkGit2Error(exitCode);
			}
			// If the HEAD doesn't exist yet, nothing we need to do. Otherwise, we load
			// it's current tree into our index.
			if (exitCode != GIT_ENOTFOUND)
			{
				git_commit* headCommit;
				checkGit2Error(git_commit_lookup(&headCommit, m_Repo, &headCommitSHA));
				git_tree* headTree;
				checkGit2Error(git_commit_tree(&headTree, headCommit));
				// Load the current tree into the index.
				checkGit2Error(git_index_read_tree(idx, headTree));
				git_tree_free(headTree);
				git_commit_free(headCommit);
			}
			// Now we have an in-memory index with the current contents of HEAD.
		}
		lastBranchTree[targetBranch] = idx;
	}

	for (auto& file : files)
	{
		if (file.IsDeleted())
		{
			checkGit2Error(git_index_remove_bypath(idx, file.GetRelativePath().c_str()));
		}
		else
		{
			git_index_entry entry = {
				.mode = GIT_FILEMODE_BLOB,
				.path = file.GetRelativePath().c_str(),
			};

			auto& blobOID = file.GetBlobOID();
			checkGit2Error(git_oid_fromstr(&entry.id, blobOID.c_str()));

			if (file.IsExecutable())
			{
				entry.mode = GIT_FILEMODE_BLOB_EXECUTABLE; // 0100755
			}

			checkGit2Error(git_index_add(idx, &entry));
		}
	}

	// Commit the index and move the ref of target branch forward to point to our
	// new commit.
	std::string commitSHA;
	{
		git_oid commitTreeID;
		checkGit2Error(git_index_write_tree_to(&commitTreeID, idx, m_Repo));

		git_tree* commitTree = nullptr;
		checkGit2Error(git_tree_lookup(&commitTree, m_Repo, &commitTreeID));

		git_signature* author = nullptr;
		checkGit2Error(git_signature_new(&author, authorName.c_str(), authorEmail.c_str(), cl.timestamp, timezoneMinutes));

		// -3 to remove the trailing "..."
		std::string commitMsg = std::to_string(cl.number) + " - " + cl.description + "\n[p4-fusion: depot-paths = \"" + depotPath.substr(0, depotPath.size() - 3) + "\": change = " + std::to_string(cl.number) + "]";

		// Find the parent commits.
		// Order is very important.
		std::vector<std::string> parentRefs = { "HEAD" };
		if (!mergeFrom.empty())
		{
			parentRefs.push_back("refs/heads/" + mergeFrom);
		}

		git_commit* parents[2];
		int parentCount = 0;
		for (std::string& parentRef : parentRefs)
		{
			git_oid refOid;
			int errorCode = git_reference_name_to_id(&refOid, m_Repo, parentRef.c_str());
			if (errorCode != 0 && errorCode != GIT_ENOTFOUND)
			{
				checkGit2Error(errorCode);
			}
			else if (errorCode != GIT_ENOTFOUND)
			{
				git_commit* refCommit = nullptr;
				checkGit2Error(git_commit_lookup(&refCommit, m_Repo, &refOid));
				if (parentCount > 0)
				{
					commitMsg += "; merged from " + parentRef;
				}
				parents[parentCount++] = refCommit;
			}
			// Skip the reference if it wasn't found.  That means it doesn't
			// exist yet, which means there wasn't a previous commit for it.
			// Practically speaking, this should only happen in non-branching
			// mode on the first commit.
		}

		// Next, write the commit object and point targetBranchRef to it.
		git_oid commitID;
		checkGit2Error(git_commit_create(&commitID, m_Repo, targetBranchRef.c_str(), author, author, "UTF-8", commitMsg.c_str(), commitTree, parentCount, (const git_commit**)parents));

		for (int i = 0; i < parentCount; i++)
		{
			git_commit_free(parents[i]);
			parents[i] = nullptr;
		}
		git_signature_free(author);
		git_tree_free(commitTree);

		commitSHA = git_oid_tostr_s(&commitID);
	}

	return commitSHA;
}

void GitAPI::CreateTagsFromLabels(LabelMap revToLabel)
{
	git_reference_iterator* refIter;
	checkGit2Error(git_reference_iterator_glob_new(&refIter, m_Repo, "refs/tags/*"));
	git_reference* ref;
	while (git_reference_next(&ref, refIter) >= 0)
	{
		std::string labelName = trimPrefix(git_reference_name(ref), "refs/tags/");

		git_commit* commit;
		checkGit2Error(git_commit_lookup(&commit, m_Repo, git_reference_target(ref)));
		if (std::string cl = getChangelistFromCommit(commit); revToLabel.contains(cl) && revToLabel.at(cl)->contains(labelName))
		{
			revToLabel.at(cl)->erase(labelName);
			if (revToLabel.at(cl)->empty())
			{
				delete revToLabel.at(cl);
				revToLabel.erase(cl);
			}
		}
		else
		{
			PRINT("Tag has moved or no longer exists, deleting: " << labelName)
			checkGit2Error(git_reference_delete(ref));
		}
		git_commit_free(commit);
		git_reference_free(ref);
	}
	git_reference_iterator_free(refIter);

	PRINT("Creating new tags, if any...")

	git_reference* head;
	checkGit2Error(git_repository_head(&head, m_Repo));

	git_commit* current_commit;
	checkGit2Error(git_commit_lookup(&current_commit, m_Repo, git_reference_target(head)));
	git_reference_free(head);

	git_commit* parent_commit;

	while (true)
	{
		std::string clID = getChangelistFromCommit(current_commit);
		if (revToLabel.contains(clID))
		{
			for (auto& [_, v] : *revToLabel.at(clID))
			{

				PRINT("TAG:" << convertLabelToTag(v.label) << ":" << v.label)
				git_reference* tmpref;
				checkGit2Error(git_reference_create(&tmpref, m_Repo, ("refs/tags/" + convertLabelToTag(v.label)).c_str(), git_commit_id(current_commit), false, v.description.c_str()));
				git_reference_free(tmpref);
			}
			delete revToLabel.at(clID);
		}
		if (git_commit_parentcount(current_commit) == 0)
		{
			git_commit_free(current_commit);
			break;
		}
		checkGit2Error(git_commit_parent(&parent_commit, current_commit, 0));
		git_commit_free(current_commit);
		current_commit = parent_commit;
	}
}

BlobWriter::BlobWriter(git_repository* gitRepo)
    : repo(gitRepo)
    , writer(nullptr)
    , state(State::Uninitialized)
{
}

BlobWriter GitAPI::WriteBlob() const
{
	if (m_Repo == nullptr)
	{
		throw std::runtime_error("created blob writer before opening repository");
	}

	return BlobWriter(m_Repo);
}

void BlobWriter::Write(const char* contents, int length)
{
	MTR_SCOPE("BlobWriter", __func__);

	if (state == State::Closed)
	{
		throw std::runtime_error("Called BlobWriter::Write after Close");
	}

	if (state == State::Uninitialized)
	{
		checkGit2Error(git_blob_create_from_stream(&writer, repo, nullptr));
		state = State::ReadyToWrite;
	}
	checkGit2Error(writer->write(writer, contents, length));
}

std::string BlobWriter::Close()
{
	MTR_SCOPE("BlobWriter", __func__);

	if (state == State::Uninitialized)
	{
		// If nothing was written yet, it's most likely an empty file is written, so
		// create a new stream and commit it right away.
		Write("", 0);
	}

	if (state == State::Closed)
	{
		throw std::runtime_error("Called BlobWriter::Close again");
	}

	git_oid objId;
	checkGit2Error(git_blob_create_from_stream_commit(&objId, writer));
	auto oid = git_oid_tostr_s(&objId);
	std::string strOID(oid);
	return strOID;
}
