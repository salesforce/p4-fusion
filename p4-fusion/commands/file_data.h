/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"
#include "utils/std_helpers.h"


// See https://www.perforce.com/manuals/cmdref/Content/CmdRef/p4_fstat.html
// for a list of actions.
enum FileAction {
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
};


struct FileData
{
	std::string depotFile;
	std::string revision;
    std::string changelist;
	std::string action;
	std::string type;
	std::vector<char> contents;

    FileAction actionCategory = FileAction::FileAdd;
	bool shouldCommit = false;

    // Empty if no source file (e.g. not an integration)
    std::string sourceDepotFile;
    // If set, the sourceRevision will start with a '#'.
    std::string sourceRevision;
    std::string sourceChangelist;

	void SetAction(std::string action);
	void Clear();

    bool IsDeleted() const;
    bool IsIntegrated() const;  // ... or copied, or moved, or ...
};
