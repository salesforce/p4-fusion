/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>
#include <memory>

#include "branch_model.h"


// Branch Model for streams under a stream depot.
// Currently only supports streams in the form "//depotname/branchname"
// (stream depot depth "//(depotname)/1").
class StreamBranchModel : public BranchModel
{
private:
    std::string m_depotName;
    std::string m_streamPrefix;
    std::vector<std::string> m_streams;
    StreamBranchModel(StreamBranchModel&& copy) {};

public:
    StreamBranchModel(const std::string depotName);
    StreamBranchModel(const std::string streamPath, bool streamPathMarker);
    ~StreamBranchModel() override {};

    bool IsStreamPathInDepot(const std::string& streamPath) const;

    bool ContainsStreamName(const std::string& streamName) const;

    // InsertStream insert the argument, in //(depotname)/(streamname) form, into the stream list.
    // If the path is not a stream path in this depot, then it is ignored and returns false.
    bool InsertStream(const std::string streamPath);

    // createForStreamPath create a new model for a stream path as returned by p4 stream -o (name), in the "Stream" field.
    static std::unique_ptr<StreamBranchModel> createForStreamPath(const std::string streamPath);

    std::vector<BranchedFiles> GetBranchedFiles(const std::vector<FileRevision> changedFiles) const override;
};
