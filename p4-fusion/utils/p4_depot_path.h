#pragma once

#include <string>
#include <vector>

// A class representing a file spec in depot syntax, without wildcards, revspec, etc.
class P4DepotPath
{
public:
	class InvalidDepotPathException : public std::exception
	{
	public:
		InvalidDepotPathException(const std::string& depotPath);

		const char* what() const noexcept override;

	private:
		std::string m_path;
		mutable std::string m_what;
	};

	class Part
	{
	public:
		Part(const std::string& part);
		Part(std::string&& part);
		Part(const char* pPartStr);

		const std::string& AsString() const;

		bool operator==(const Part& other) const;
		bool operator!=(const Part& other) const;

	private:
		std::string m_rawPart;
		std::string m_normalizedPart; // Lowercase part
	};

	using Parts = std::vector<Part>;

	P4DepotPath(const std::string& path);
	P4DepotPath(std::string&& path);
	P4DepotPath(Parts::const_iterator begin, Parts::const_iterator end);
	P4DepotPath(const Parts& parts);

	// Might throw std::out_of_range or std::invalid_argument
	Parts Splice(size_t index_begin, size_t index_end) const;

	const std::string& AsString() const;
	Part GetDepotName() const;

	Parts GetParts() const;

	// Comparison operators
	bool operator==(const P4DepotPath& other) const;
	bool operator!=(const P4DepotPath& other) const;

private:
	std::string m_rawPath;
	std::string m_normalizedPath; // Lowercase path, trailing slash removed (if present)
};

namespace std
{
template <>
struct hash<P4DepotPath>
{
	size_t operator()(const P4DepotPath& path) const
	{
		return std::hash<std::string>()(path.AsString());
	}
};
}