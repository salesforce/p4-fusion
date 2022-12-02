/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "stream_branch_model.h"

#include <vector>
#include <array>
#include <map>
#include <stdexcept>

#include "common.h"

std::string createStreamPrefix(const std::string& srcDepotName)
{
    return "//" + srcDepotName + "/";
}


StreamBranchModel::StreamBranchModel(const std::string srcDepotName)
    : m_depotName(srcDepotName)
    , m_streamPrefix(createStreamPrefix(srcDepotName))
{
}


// splitStreamPath split the stream path into depot name, stream name, using //(depotname)/1 stream depth pattern.
// Returns two empty lists if it's not the right format.
std::array<std::string, 2> splitStreamPath(const std::string& streamPath)
{
    if (streamPath.find_first_of("//") != 0)
    {
        return {"", ""};
    }
    size_t spos = streamPath.rfind('/');
    if (spos <= 2 || spos == std::string::npos)
    {
        // == 2 means "///", which is just wrong.
        return {"", ""};
    }
    std::string depotName = streamPath.substr(2, spos - 2);
    if (depotName.find('/') != std::string::npos)
    {
        // more than one path separator in stream path.
        return {"", ""};
    }
    std::string streamName = streamPath.substr(spos + 1);
    return {depotName, streamName};
}


StreamBranchModel::StreamBranchModel(const std::string streamPath, bool streamPathMarker)
{
    std::array<std::string, 2> parts = splitStreamPath(streamPath);
    if (parts[0].size() <= 0 || parts[1].size() <= 0)
    {
        throw std::invalid_argument("invalid stream path");
    }
    m_depotName = parts[0];
    m_streamPrefix = createStreamPrefix(parts[0]);
    // We can avoid the extra check and split.
    m_streams.push_back(parts[1]);
}


// splitStreamFilePath split the file based on the format "//(depotname)/(streamname)/(path)"
// returns {depotname, streamname, /path}; path will start with a '/'.
std::array<std::string, 3> splitStreamFilePath(const std::string& filePath)
{
    if (filePath.find_first_of("//") != 0)
    {
        return {"", "", ""};
    }
    size_t spos = filePath.find('/', 2);
    if (spos <= 2 || spos == std::string::npos)
    {
        return {"", "", ""};
    }
    std::string depotName = filePath.substr(2, spos - 2);
    size_t ppos = filePath.find('/', spos + 1);
    if (ppos <= spos + 1 || ppos == std::string::npos)
    {
        return {"", "", ""};
    }
    std::string streamName = filePath.substr(spos + 1, ppos - spos - 1);
    std::string path = filePath.substr(ppos);
    return {depotName, streamName, path};
}


struct streamIntegrationMap {
    std::vector<BranchedFiles> branches;
    std::map<std::string, int> branchIndicies;

    void addMerge(std::string& sourceBranch, std::string& targetBranch, const FileRevision& rev);
    void addTarget(std::string& targetBranch, const FileRevision& rev);
};

void streamIntegrationMap::addTarget(std::string& targetBranch, const FileRevision& rev)
{
    std::string src = "";
    addMerge(src, targetBranch, rev);
}

void streamIntegrationMap::addMerge(std::string& sourceBranch, std::string& targetBranch, const FileRevision& rev)
{
    // Need to store this in the integration map, using "src/tgt" as the
    // key.  Because stream names can't have a '/' in them, this creates a unique key.
    // source might be empty, and that's okay.
    std::string mapKey = sourceBranch + "/" + targetBranch;
    auto entry = branchIndicies.find(mapKey);
    if (entry == branchIndicies.end())
    {
        const int index = branches.size();
        BranchedFiles bf;
        bf.sourceBranch = sourceBranch;
        bf.targetBranch = targetBranch;
        bf.hasSource = !sourceBranch.empty();
        branches.push_back(bf);
        // Add the files to the copy, as a small optimization.
        branches.at(index).files.push_back(rev);
        branchIndicies.insert(std::make_pair(mapKey, index));
    }
    else
    {
        branches.at(entry->second).files.push_back(rev);
    }
}


std::vector<BranchedFiles> StreamBranchModel::GetBranchedFiles(const std::vector<FileRevision> changedFiles) const
{
    // This implementation doesn't care if the integration spreads across multiple streams.
    // Perforce is supposed to prevent that by limiting client views on submit to a single stream.

    streamIntegrationMap streamIntegrations;
    for (int i = 0; i < changedFiles.size(); i++)
    {
        const FileRevision& rev = changedFiles.at(i);
        std::array<std::string, 3> targetParts = splitStreamFilePath(rev.target);
        if (targetParts[0] == m_depotName && ContainsStreamName(targetParts[1]))
        {
            // It's a valid target within a stream.
            // Now see if it's a merge.
            bool isNormal = true;
            if (rev.hasSource)
            {
                std::array<std::string, 3> sourceParts = splitStreamFilePath(rev.source);
                if (
                        // Is the source also related to the same depot?
                        sourceParts[0] == m_depotName

                        // Does it reference a different stream?
                        && sourceParts[1] != targetParts[1]

                        // Is the source and destination relative paths the same?
                        // (e.g. not a rename)
                        && sourceParts[2] == targetParts[2]

                        // Is the stream covered by this model?
                        && ContainsStreamName(sourceParts[1])
                )
                {
                    // It all checks out.
                    streamIntegrations.addMerge(sourceParts[1], targetParts[1], rev);
                    isNormal = false;
                }
            }
            if (isNormal)
            {
                // Just use the target stream by itself.
                streamIntegrations.addTarget(targetParts[1], rev);
            }
        }
    }
    return streamIntegrations.branches;
}


bool StreamBranchModel::IsStreamPathInDepot(const std::string& streamPath) const
{
    return streamPath.find(m_streamPrefix) == 0;
}

bool StreamBranchModel::ContainsStreamName(const std::string& streamName) const
{
    for (int i = 0; i < m_streams.size(); i++)
    {
        if (streamName == m_streams.at(i))
        {
            return true;
        }
    }
    return false;
}


bool StreamBranchModel::InsertStream(const std::string streamPath)
{
    std::array<std::string, 2> parts = splitStreamPath(streamPath);
    if (m_depotName == parts[0])
    {
        m_streams.push_back(parts[1]);
        return true;
    }
    return false;
}
