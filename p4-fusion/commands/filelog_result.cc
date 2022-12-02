/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "filelog_result.h"


// Should be called once per varlist.  Each filelog file
//   is its own entry.
void FileLogResult::OutputStat(StrDict* varList)
{
    FileData entry;
    entry.depotFile = varList->GetVar("depotFile")->Text();
    // Only get the first record...
    entry.revision = varList->GetVar("rev0")->Text();
    PRINT("Read depot file " << m_FileData.size() << " " << entry.depotFile << "#" << entry.revision);
    entry.changelist = varList->GetVar("change0")->Text();
    entry.type = varList->GetVar("type0")->Text();
    entry.SetAction(varList->GetVar("action0")->Text());

	int i = 0;
	StrPtr* how = nullptr;

    // Optimization: only perform this loop if the action type is
    //   an integration style action.
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
        //       This tool doesn't care about this action.  These are ignored.
        // "* from" - integrated into this depot file from another location.  Definitely care about these.
        // (* here is "add", "merge", "branch", "moved", "copy", "delete", "edit")
        // "Add w/ Edit", "Merge w/ Edit" - an integrate + an edit on top of the merge.
        //            (Add - it hasn't existed before in the target; Merge - it already existed there and is being edited)
        //            This one is seen often in Java move operations between trees when the "package" line needs to
        //            change with the move.  This rarely happens cross-branch, and when it does, it's not
        //            really a merge operation.
        // "undid" - a "revert changelist" action from a previous revision of the same file (target of p4 undo)
        // "undone by" - the "* into" concept for "undid" (source of p4 undo)
        // "Undone w/Edit"

        if (howStr == "delete from")
        {
            // The action needs to be marked as something very clearly a delete.
            // See file_data.h and file_data.cc for this special replaced action.
            entry.SetAction(FAKE_INTEGRATION_DELETE_ACTION_NAME);
        }

        if (STDHelpers::EndsWith(howStr, " from"))
        {
            // copy or integrate or branch or move or archive from a location.
            entry.sourceDepotFile = varList->GetVar(("file0," + indexString).c_str())->Text();
            // already includes the '#' on it.  Strip it off for consistency with .revision value
            std::string sourceRev = varList->GetVar(("erev0," + indexString).c_str())->Text();
            if (STDHelpers::StartsWith(sourceRev, "#"))
            {
                sourceRev.erase(0, 1);
            }
            entry.sourceRevision = sourceRev;
            PRINT("   " << entry.action << " <- " << entry.sourceDepotFile << "#" << entry.sourceRevision);
            break;
        }
	}

    m_FileData.push_back(entry);
}
