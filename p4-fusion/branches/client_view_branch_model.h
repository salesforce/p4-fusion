/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "branch_model.h"
#include "file_map.h"


// Represents a non-branching client view.
// All changes under the view go into one branch.
class ClientViewBranchModel : public BranchModel
{
private:
    const std::string m_branchName;
    FileMap m_view;

public:
    ClientViewBranchModel(const std::string& branchName, const std::vector<std::string>& mapping);
    ~ClientViewBranchModel() override {};

    std::vector<BranchedFiles> GetBranchedFiles(const std::vector<FileRevision> changedFiles) const override;
};
