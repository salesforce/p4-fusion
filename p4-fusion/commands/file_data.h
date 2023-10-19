/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <memory>
#include <atomic>
#include "common.h"
#include "utils/std_helpers.h"

#define FAKE_INTEGRATION_DELETE_ACTION_NAME "FAKE merge delete"

// See https://www.perforce.com/manuals/cmdref/Content/CmdRef/p4_fstat.html
// for a list of actions.
enum FileAction
{
	FileAdd, // add
	FileEdit, // edit
	FileDelete, // delete
	FileBranch, // branch
	FileMoveAdd, // move/add
	FileMoveDelete, // move/delete
	FileIntegrate, // integrate
	FileImport, // import
	FilePurge, // purge
	FileArchive, // archive

	FileIntegrateDelete, // artificial action to reflect an integration that happened that caused a delete
};

struct FileDataStore
{
	// describe/filelog values
	std::string depotFile;
	std::string revision;
	std::string action;
	std::string type;

	// filelog values
	//   - empty if not an integration style change
	std::string fromDepotFile;
	std::string fromRevision;

	// print values
	//   the "is*" values here are intended to put the
	//   breaks on possible multi-threaded downloads.
	std::vector<char> contents;
	std::atomic<bool> isContentsSet;
	std::atomic<bool> isContentsPendingDownload;

	// Derived Values
	std::string relativePath;
	FileAction actionCategory;
	bool isDeleted;
	bool isIntegrated; // ... or copied, or moved, or ...

	FileDataStore();

	void SetAction(std::string action);

	void Clear();
};

// For memory efficiency; the underlying data is passed around a bunch.
struct FileData
{
private:
	std::shared_ptr<FileDataStore> m_data;

public:
	FileData(std::string& depotFile, std::string& revision, std::string& action, std::string& type);
	FileData(const FileData& copy);
	FileData& operator=(FileData& other);

	void SetFromDepotFile(const std::string& fromDepotFile, const std::string& fromRevision);
	void SetRelativePath(std::string& relativePath);
	void SetFakeIntegrationDeleteAction() { m_data->SetAction(FAKE_INTEGRATION_DELETE_ACTION_NAME); };

	// moves the argument's data into this file data structure.
	void MoveContentsOnceFrom(const std::vector<char>& contents);
	void SetPendingDownload();
	bool IsDownloadNeeded() const { return !m_data->isContentsSet && !m_data->isContentsPendingDownload; };

	const std::string& GetDepotFile() const { return m_data->depotFile; };
	const std::string& GetRevision() const { return m_data->revision; };
	const std::string& GetRelativePath() const { return m_data->relativePath; };
	const std::vector<char>& GetContents() const { return m_data->contents; };
	bool IsDeleted() const { return m_data->isDeleted; };
	bool IsIntegrated() const { return m_data->isIntegrated; };
	std::string& GetFromDepotFile() const { return m_data->fromDepotFile; };

	bool IsBinary() const;
	bool IsExecutable() const;

	void Clear() { m_data->Clear(); };
};
