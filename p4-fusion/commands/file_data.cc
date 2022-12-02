/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "file_data.h"

FileAction extrapolateFileAction(std::string& action);

void FileData::Clear()
{
    depotFile.clear();
    revision.clear();
    changelist.clear();
    action.clear();
    type.clear();
    contents.clear();
    sourceDepotFile.clear();
    sourceRevision.clear();
    sourceChangelist.clear();
}

void FileData::SetAction(std::string fileAction)
{
    action = fileAction;
    actionCategory = extrapolateFileAction(fileAction);
}


bool FileData::IsDeleted() const
{
    switch (actionCategory)
    {
    case FileAction::FileDelete:
    case FileAction::FileMoveDelete:
    case FileAction::FilePurge:
    case FileAction::FileIntegrateDelete:
        return true;
    default:
        return false;
    }
}


bool FileData::IsIntegrated() const
{
    switch (actionCategory)
    {
    case FileAction::FileBranch:
    case FileAction::FileMoveAdd:
    case FileAction::FileIntegrate:
    case FileAction::FileImport:
    // case FileAction::FileArchive:
    case FileAction::FileIntegrateDelete:
        return true;
    default:
        return false;
    }
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
        WARN("Found an unsupported action " << action << "; assuming delete");
        return FileAction::FileDelete;
    }
    if (STDHelpers::Contains(action, "move/"))
    {
        // Looks like a new kind of integrate.
        WARN("Found an unsupported action " << action << "; assuming move/add");
        return FileAction::FileMoveAdd;
    }

    // assume an edit, as it's the safe bet.
    WARN("Found an unsupported action " << action << "; assuming edit");
    return FileAction::FileEdit;
}
