#ifndef LABELS_CONVERSION_H
#define LABELS_CONVERSION_H
#include "commands/labels_result.h"
#include "p4_api.h"

std::string convertLabelToTag(std::string label);
std::string trimPrefix(const std::string& str, const std::string& prefix);
std::string getChangelistFromCommit(const git_commit* commit);
LabelMap getLabelsDetails(P4API* p4, std::string depotPath, LabelNameToDetails labels);
LabelNameToDetails getLabelsDetails2(P4API* p4, std::list<LabelsResult::LabelData> labels);

#endif // LABELS_CONVERSION_H
