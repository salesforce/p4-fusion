#include <string>
#include <regex>

#include "git_api.h"
#include "p4_api.h"
#include "git2/commit.h"
#include "git2/types.h"

// sanitizeLabelName removes characters from a label name that
// aren't valid in git tags as specified by
// https://git-scm.com/docs/git-check-ref-format
//
// All invalid characters are replaced with underscores.
//
// This function was LLM generated and the regex checked with
// https://regex101.com/ . Seems to make sense, and tested
// with a few label names.
std::string convertLabelToTag(std::string input)
{
	std::string result = input;

	// Rule: Replace invalid characters with underscores
	std::regex invalidChars(R"([[:cntrl:]\x7f ~^:?\*\[\]\\])");
	result = std::regex_replace(result, invalidChars, "_");

	// Rule: Remove sequences "@{"
	std::regex atBrace(R"(@\{)");
	result = std::regex_replace(result, atBrace, "_");

	// Rule: No consecutive dots
	std::regex consecutiveDots(R"(\.\.)");
	while (std::regex_search(result, consecutiveDots))
	{
		result = std::regex_replace(result, consecutiveDots, "_");
	}

	// Rule: No slash-separated component can begin with a dot
	std::regex dotComponent(R"(/\.|^\.|(\.\.))");
	if (std::regex_search(result, dotComponent))
	{
		result = std::regex_replace(result, dotComponent, "_");
	}

	// Rule: No component can end with ".lock"
	std::regex dotLock(R"(\.lock(?=/|$))");
	result = std::regex_replace(result, dotLock, "_lock");

	// Rule: Cannot end with a dot
	if (result.back() == '.')
	{
		result.back() = '_';
	}

	// Rule: Cannot end with a slash
	if (result.back() == '/')
	{
		result.back() = '_';
	}

	// Rule: Cannot begin with a slash
	if (result.front() == '/')
	{
		result.front() = '_';
	}

	// Rule: Cannot have multiple consecutive slashes
	std::regex consecutiveSlashes(R"(//+)");
	result = std::regex_replace(result, consecutiveSlashes, "/");

	// Rule: Cannot be the single character '@'
	if (result == "@")
	{
		result = "_";
	}

	return result;
}

// Trim the specified suffix from the string
std::string trimSuffix(const std::string& str, const std::string& suffix)
{
	if (str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0)
	{
		return str.substr(0, str.size() - suffix.size());
	}
	return str;
}

// Trim the specified prefix from the string
std::string trimPrefix(const std::string& str, const std::string& prefix)
{
	if (str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0)
	{
		return str.substr(prefix.size());
	}
	return str;
}

std::string getChangelistFromCommit(const git_commit* commit)
{
	// Look for the specific change message generated from the Commit method.
	// Note that extra branching information can be added after it.
	// ": change = " is 11 characters long.
	std::string message = git_commit_message(commit);
	const size_t pos = message.rfind(": change = ");
	if (pos == std::string::npos)
	{
		throw std::invalid_argument("failed to parse commit message, well known section : change =  not found");
	}
	// The changelist number starts after 11 characters, the length of ": change = ".
	const size_t clStart = pos + 11;
	const size_t clEnd = message.find(']', clStart);
	if (clEnd == std::string::npos)
	{
		throw std::invalid_argument("failed to parse commit message, well closing ] not found");
	}

	std::string cl(message, clStart, clEnd - clStart);

	return cl;
}

// Fetches additional label details for each label using the provided p4 client
// and returns a map of revision to label to label details.
LabelMap getLabelsDetails(P4API* p4, std::string depotPath, std::list<std::string> labels)
{
	LabelMap revToLabel;

	for (auto& label : labels)
	{
		LabelResult labelRes = p4->Label(label);
		if (labelRes.HasError())
		{
			ERR("Failed to retrieve label details: " << labelRes.PrintError());
			continue;
		}
		if (!labelRes.revision.starts_with("@"))
		{
			continue;
		}
		labelRes.revision.erase(labelRes.revision.begin());
		if (labelRes.views.empty())
		{
			if (!revToLabel.contains(labelRes.revision))
			{
				auto* newMap = new std::unordered_map<std::string, LabelResult>;
				revToLabel.insert({ labelRes.revision, newMap });
			}
			auto res = revToLabel.at(labelRes.revision);
			res->insert({ convertLabelToTag(labelRes.label), labelRes });
		}
		else
		{
			for (auto& view : labelRes.views)
			{
				if (depotPath.starts_with(trimSuffix(view, "...")))
				{
					if (!revToLabel.contains(labelRes.revision))
					{
						auto* newMap = new std::unordered_map<std::string, LabelResult>;
						revToLabel.insert({ labelRes.revision, newMap });
					}
					auto res = revToLabel.at(labelRes.revision);
					res->insert({ convertLabelToTag(labelRes.label), labelRes });
				}
			}
		}
	}

	return revToLabel;
}
