/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <memory>

#include "branch_model.h"
#include "stream_branch_model.h"

// All stream depots under consideration.
class AllStreamsBranchModel : public BranchModel
{
private:
    std::vector<std::unique_ptr<StreamBranchModel>> m_streamDepots;
    AllStreamsBranchModel(AllStreamsBranchModel& copy) {};

public:
    AllStreamsBranchModel() {};
    ~AllStreamsBranchModel() override {};
    void InsertStreamPath(const std::string streamPath);

    bool IsEmpty() const { return m_streamDepots.empty(); };

    std::vector<BranchedFiles> GetBranchedFiles(const std::vector<FileRevision> changedFiles) const override;
};
