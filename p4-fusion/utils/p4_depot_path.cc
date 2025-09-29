#include "p4_depot_path.h"

#include "std_helpers.h"

namespace
{

bool IsValidPath(const std::string& depotPath)
{
	// Best-effort validation...

	if (depotPath.empty())
	{
		return false;
	}

	if (!STDHelpers::StartsWith(depotPath, "//"))
	{
		return false;
	}

	if (STDHelpers::Contains(depotPath, "\\"))
	{
		return false;
	}

	return true;
}

std::string NormalizePath(const std::string& path)
{
	std::string result = STDHelpers::ToLower(path);
	if (STDHelpers::EndsWith(result, "/"))
	{
		result.pop_back();
	}

	return result;
}

}

P4DepotPath::InvalidDepotPathException::InvalidDepotPathException(const std::string& path)
    : m_path(path)
{
}

const char* P4DepotPath::InvalidDepotPathException::what() const noexcept
{
	m_what = std::string("Invalid P4 depot path: " + m_path);

	return m_what.c_str();
}

P4DepotPath::Part::Part(const std::string& part)
    : Part(std::string(part))
{
}

P4DepotPath::Part::Part(std::string&& part)
{
	m_rawPart = std::move(part);
	m_normalizedPart = STDHelpers::ToLower(m_rawPart);
}

P4DepotPath::Part::Part(const char* pPartStr)
{
	m_rawPart = pPartStr;
	m_normalizedPart = STDHelpers::ToLower(pPartStr);
}

const std::string& P4DepotPath::Part::AsString() const
{
	return m_rawPart;
}

bool P4DepotPath::Part::operator==(const Part& other) const
{
	return m_normalizedPart == other.m_normalizedPart;
}

bool P4DepotPath::Part::operator!=(const Part& other) const
{
	return !(*this == other);
}

P4DepotPath::P4DepotPath(const std::string& path)
    : P4DepotPath(std::string(path))
{
}

P4DepotPath::P4DepotPath(std::string&& path)
{
	if (!IsValidPath(path))
	{
		throw InvalidDepotPathException(path);
	}

	m_rawPath = std::move(path);
	m_normalizedPath = NormalizePath(m_rawPath);
}

P4DepotPath::P4DepotPath(Parts::const_iterator begin, Parts::const_iterator end)
{
	std::string path = "//";
	for (auto it = begin; it != end; ++it)
	{
		path += it->AsString() + "/";
	}

	m_rawPath = path;
	m_normalizedPath = NormalizePath(m_rawPath);
}

P4DepotPath::P4DepotPath(const Parts& parts)
    : P4DepotPath(parts.begin(), parts.end())
{
}

P4DepotPath::Parts P4DepotPath::Splice(size_t index_begin, size_t index_end) const
{
	auto parts = GetParts();

	if (index_begin >= parts.size() || index_end >= parts.size())
	{
		throw std::out_of_range("Index out of range");
	}

	if (index_begin >= index_end)
	{
		throw std::invalid_argument("Invalid splice range");
	}

	return { parts.begin() + index_begin, parts.begin() + index_end + 1 }; // +1: half-open range blues
}

const std::string& P4DepotPath::AsString() const
{
	return m_rawPath;
}

P4DepotPath::Part P4DepotPath::GetDepotName() const
{
	return GetParts()[0];
}

std::vector<P4DepotPath::Part> P4DepotPath::GetParts() const
{
	std::vector<P4DepotPath::Part> result;
	std::string remainder = m_rawPath;
	STDHelpers::Erase(remainder, "//");

	do
	{
		std::array<std::string, 2> splitResult = STDHelpers::SplitAt(remainder, '/');
		result.push_back(splitResult[0]);

		remainder = splitResult[1];
	} while (!remainder.empty());

	return result;
}

bool P4DepotPath::operator==(const P4DepotPath& other) const
{
	return m_normalizedPath == other.m_normalizedPath;
}

bool P4DepotPath::operator!=(const P4DepotPath& other) const
{
	return !(*this == other);
}

std::string P4Unescape(std::string p4path)
{
	STDHelpers::ReplaceAll(p4path, "%40", "@");
	STDHelpers::ReplaceAll(p4path, "%23", "#");
	STDHelpers::ReplaceAll(p4path, "%2A", "*");
	// Replace %25 last to avoid interfering with other escape sequences
	STDHelpers::ReplaceAll(p4path, "%25", "%");
	return p4path;
}