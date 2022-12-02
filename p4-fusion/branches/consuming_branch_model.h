/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "branch_model.h"
#include "file_map.h"


// Captures all values into a single commit.
// If the client view >= the full list of files requested to move,
// then this avoids the extra view check that's unnecessary
// that ClientViewBranchModel introduces.
class ConsumingBranchModel : public BranchModel
{
private:
    const std::string m_branchName;

public:
    ConsumingBranchModel(const std::string& branchName);
    ~ConsumingBranchModel() override {};

    std::vector<BranchedFiles> GetBranchedFiles(const std::vector<FileRevision> changedFiles) const override;
};
