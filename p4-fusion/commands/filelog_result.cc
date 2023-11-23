/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "filelog_result.h"

FileLogResult& FileLogResult::operator=(const FileLogResult& other)
{
	if (this == &other)
	{
		// guard...
		return *this;
	}

	m_FileData = other.m_FileData;
	return *this;
}

// Should be called once per varlist.  Each filelog file
//   is its own entry.
void FileLogResult::OutputStat(StrDict* varList)
{
	StrPtr* depotFile = varList->GetVar("depotFile");
	if (!depotFile)
	{
		// Quick exit if the object returned is not a file
		return;
	}
	std::string depotFileStr = depotFile->Text();
	// Only get the first record...
	// std::string changelist = varList->GetVar(("change0").c_str())->Text();
	std::string type = varList->GetVar("type0")->Text();
	std::string revision = varList->GetVar("rev0")->Text();
	std::string action = varList->GetVar("action0")->Text();

	m_FileData.emplace_back(depotFileStr, revision, action, type);
	FileData& fileData = m_FileData.back();

	// Could optimize here by only performing this loop if the action type is
	//   an integration style action (entry->isIntegration == true).
	//   That needs testing, though.
	int i = 0;
	StrPtr* how;
	while (true)
	{
		std::string indexString = std::to_string(i++);
		how = varList->GetVar(("how0," + indexString).c_str());

		if (!how)
		{
			break;
		}

		std::string howStr = how->Text();

		// How text values listed at:
		// https://www.perforce.com/manuals/cmdref/Content/CmdRef/p4_integrated.html

		// "* into" - integrated to another location from the current depot file.
		//	   This tool doesn't care about this action.  These are ignored.
		// "* from" - integrated into this depot file from another location.  Definitely care about these.
		// (* here is "add", "merge", "branch", "moved", "copy", "delete", "edit")
		// "Add w/ Edit", "Merge w/ Edit" - an integrate + an edit on top of the merge.
		//			(Add - it hasn't existed before in the target; Merge - it already existed there and is being edited)
		//			This one is seen often in Java move operations between trees when the "package" line needs to
		//			change with the move.  This rarely happens cross-branch, and when it does, it's not
		//			really a merge operation.
		// "undid" - a "revert changelist" action from a previous revision of the same file (target of p4 undo)
		// "undone by" - the "* into" concept for "undid" (source of p4 undo)
		// "Undone w/Edit"

		if (howStr == "delete from")
		{
			// The action needs to be marked as something very clearly a delete.
			// See file_data.h and file_data.cc for this special replaced action.
			fileData.SetFakeIntegrationDeleteAction();
		}

		if (STDHelpers::EndsWith(howStr, " from"))
		{
			// copy or integrate or branch or move or archive from a location.
			std::string fromDepotFile = varList->GetVar(("file0," + indexString).c_str())->Text();
			std::string fromRev = varList->GetVar(("erev0," + indexString).c_str())->Text();
			fileData.SetFromDepotFile(fromDepotFile, fromRev);

			// Don't look for any other integration history; there can (should?) be at most one.
			break;
		}
	}
}
