/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "file_data.h"

FileAction extrapolateFileAction(std::string& action);

FileDataStore::FileDataStore(std::string& _depotFile, std::string& _revision, std::string& action, std::string& type)
    : depotFile(_depotFile)
    , revision(_revision)
    , isBinary(STDHelpers::Contains(type, "binary"))
    , isExecutable(STDHelpers::Contains(type, "+x"))
    , isContentsPendingDownload(false)
    , actionCategory(extrapolateFileAction(action))
{
	SetAction(action);
}

FileData::FileData(std::string& depotFile, std::string& revision, std::string& action, std::string& type)
    : m_data(std::make_shared<FileDataStore>(depotFile, revision, action, type))
{
}

FileData& FileData::operator=(const FileData& other)
{
	if (this == &other)
	{
		return *this;
	}

	m_data = other.m_data;
	return *this;
}

void FileData::SetFromDepotFile(const std::string& fromDepotFile, const std::string& fromRevision)
{
	m_data->fromDepotFile = fromDepotFile;
	if (STDHelpers::StartsWith(fromRevision, "#"))
	{
		m_data->fromRevision = fromRevision.substr(1);
	}
	else
	{
		m_data->fromRevision = fromRevision;
	}
}

void FileData::SetBlobOID(std::string&& blobOID)
{
	std::lock_guard<std::mutex> lock(m_data->blobOIDMu);

	if (m_data->blobOID != nullptr)
	{
		// Do not set the contents.  Assume that
		// they were already set or, worst case, are currently being set.
		return;
	}

	m_data->blobOID = std::make_unique<std::string>(std::move(blobOID));
	m_data->isContentsPendingDownload = false;
}

void FileData::SetPendingDownload()
{
	std::lock_guard<std::mutex> lock(m_data->blobOIDMu);

	if (m_data->blobOID == nullptr)
	{
		m_data->isContentsPendingDownload = true;
	}
}

void FileData::SetRelativePath(std::string& relativePath)
{
	m_data->relativePath = relativePath;
}

void FileDataStore::SetAction(std::string fileAction)
{
	actionCategory = extrapolateFileAction(fileAction);
	switch (actionCategory)
	{
	case FileAction::FileBranch:
	case FileAction::FileMoveAdd:
	case FileAction::FileIntegrate:
	case FileAction::FileImport:
		isIntegrated = true;
		isDeleted = false;
		break;

	case FileAction::FileDelete:
	case FileAction::FileMoveDelete:
	case FileAction::FilePurge:
		// Note: not including FileAction::FileArchive
		isDeleted = true;
		isIntegrated = false;
		break;

	case FileAction::FileIntegrateDelete:
		// This is the source of the integration,
		//   so even though this causes a delete to happen,
		//   as a source, there isn't something merging into this
		//   change.
		isIntegrated = false;
		isDeleted = true;
		break;

	default:
		isIntegrated = false;
		isDeleted = false;
	}
}

void FileDataStore::Clear()
{
	depotFile.clear();
	revision.clear();
	fromDepotFile.clear();
	fromRevision.clear();
	blobOID = nullptr;
	relativePath.clear();
}

FileAction extrapolateFileAction(std::string& action)
{
	if ("add" == action)
	{
		return FileAction::FileAdd;
	}
	if ("edit" == action)
	{
		return FileAction::FileEdit;
	}
	if ("delete" == action)
	{
		return FileAction::FileDelete;
	}
	if ("branch" == action)
	{
		return FileAction::FileBranch;
	}
	if ("move/add" == action)
	{
		return FileAction::FileMoveAdd;
	}
	if ("move/delete" == action)
	{
		return FileAction::FileMoveDelete;
	}
	if ("integrate" == action)
	{
		return FileAction::FileIntegrate;
	}
	if ("import" == action)
	{
		return FileAction::FileImport;
	}
	if ("purge" == action)
	{
		return FileAction::FilePurge;
	}
	if ("archive" == action)
	{
		return FileAction::FileArchive;
	}
	if (FAKE_INTEGRATION_DELETE_ACTION_NAME == action)
	{
		return FileAction::FileIntegrateDelete;
	}

	// That's all the actions known at the time of writing.
	// An unknown type, probably some future Perforce version with a new kind of action.
	if (STDHelpers::Contains(action, "delete"))
	{
		// Looks like a delete.
		WARN("Found an unsupported action " << action << "; assuming delete")
		return FileAction::FileDelete;
	}
	if (STDHelpers::Contains(action, "move/"))
	{
		// Looks like a new kind of integrate.
		WARN("Found an unsupported action " << action << "; assuming move/add")
		return FileAction::FileMoveAdd;
	}

	// assume an edit, as it's the safe bet.
	WARN("Found an unsupported action " << action << "; assuming edit")
	return FileAction::FileEdit;
}
