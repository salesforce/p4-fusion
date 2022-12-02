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
    std::cout << "Read depot file " << m_FileData.size() << " " << entry.depotFile << "#" << entry.revision << "\n";
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
        if (STDHelpers::Contains(howStr, " from"))
        {
            // copy or integrate or branch or move or archive from a location.
            entry.sourceDepotFile = varList->GetVar(("file0," + indexString).c_str())->Text();
            // already includes the '#' on it.
            entry.sourceRevision = varList->GetVar(("erev0," + indexString).c_str())->Text();
            break;
        }
	}

    m_FileData.push_back(entry);
}
