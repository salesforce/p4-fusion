/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "file_map.h"

FileMap::FileMap()
    : m_sensitivity(MapCase::Sensitive)
{
	// The constructor line + this m_map set are the equivalent of calling SetCaseSensitivity locally.
	m_map.SetCaseSensitivity(m_sensitivity);
}

FileMap::FileMap(const FileMap& src)
    : m_sensitivity(src.GetCaseSensitivity())
{
	// This call sets the case sensitivity of m_map.
	src.copyMapApiInto(m_map);
}

bool FileMap::IsInLeft(const std::string fileRevision) const
{
	MapApi argMap;
	argMap.SetCaseSensitivity(m_sensitivity);
	argMap.Insert(StrBuf(fileRevision.c_str()), MapType::MapInclude);

	// MapAPI is poorly written and doesn't declare things as const when it should.
	std::unique_ptr<MapApi> joinResult(MapApi::Join(const_cast<MapApi*>(&m_map), &argMap));
	return joinResult != nullptr;
}

bool FileMap::IsInRight(const std::string fileRevision) const
{
	StrBuf to;
	StrBuf from(fileRevision.c_str());

	// MapAPI is poorly written and doesn't declare things as const when it should.
	MapApi* ref = const_cast<MapApi*>(&m_map);
	return ref->Translate(from, to);
}

void FileMap::SetCaseSensitivity(const MapCase mode)
{
	m_sensitivity = mode;
	m_map.SetCaseSensitivity(mode);
}

std::string FileMap::TranslateLeftToRight(const std::string& path) const
{
	StrBuf from(path.c_str());
	StrBuf to;

	// MapAPI is poorly written and doesn't declare things as const when it should.
	MapApi* ref = const_cast<MapApi*>(&m_map);
	if (ref->Translate(from, to, MapDir::MapLeftRight))
	{
		return to.Text();
	}
	return "";
}

std::string FileMap::TranslateRightToLeft(const std::string& path) const
{
	StrBuf from(path.c_str());
	StrBuf to;

	// MapAPI is poorly written and doesn't declare things as const when it should.
	MapApi* ref = const_cast<MapApi*>(&m_map);
	if (ref->Translate(from, to, MapDir::MapRightLeft))
	{
		return to.Text();
	}
	return "";
}

void FileMap::insertMapping(const std::string& left, const std::string& right, const MapType mapType)
{
	std::string mapStrLeft = left;
	mapStrLeft.erase(mapStrLeft.find_last_not_of(' ') + 1);
	mapStrLeft.erase(0, mapStrLeft.find_first_not_of(' '));

	std::string mapStrRight = right;
	mapStrRight.erase(mapStrRight.find_last_not_of(' ') + 1);
	mapStrRight.erase(0, mapStrRight.find_first_not_of(' '));

	m_map.Insert(StrBuf(mapStrLeft.c_str()), StrBuf(mapStrRight.c_str()), mapType);
}

void FileMap::InsertTranslationMapping(const std::vector<std::string>& mapping)
{
	for (int i = 0; i < mapping.size(); i++)
	{
		const std::string& view = mapping.at(i);

		size_t left = view.find('/');

		MapType mapType = MapType::MapInclude;
		switch (view.front())
		{
		case '+':
			mapType = MapType::MapOverlay;
			break;
		case '-':
			mapType = MapType::MapExclude;
			break;
		case '&':
			mapType = MapType::MapOneToMany;
			break;
		}

		// TODO This also needs quote handling

		// Skip the first few characters to only match with the right half.
		size_t right = view.find("//", 3);
		if (right == std::string::npos)
		{
			WARN("Found a one-sided mapping, ignoring...");
			continue;
		}

		insertMapping(view.substr(left, right), view.substr(right), mapType);
	}
}

void FileMap::InsertPaths(const std::vector<std::string>& paths)
{
	for (int i = 0; i < paths.size(); i++)
	{
		const std::string& view = paths.at(i);
		insertMapping(view, view, MapType::MapInclude);
	}
}

void FileMap::InsertFileMap(const FileMap& src)
{
	src.copyMapApiInto(m_map);
}

const std::vector<std::string> PATH_PREFIX_DESCRIPTIONS = {
	// Order is important.
	"share ",
	"isolate ",
	"import+ ",
	"import ",
	"exclude "
};
const int PATH_PREFIX_DESCRIPTION_COUNT = 5;
const int PATH_PREFIX_DESCRIPTION_EXCLUDE_INDEX_START = 4;

void FileMap::InsertPrefixedPaths(const std::string prefix, const std::vector<std::string>& paths)
{
	for (int i = 0; i < paths.size(); i++)
	{
		MapType mapType = MapType::MapInclude;
		std::string view = paths.at(i);

		// Some paths, such as the Stream spec, can include a prefix.
		for (int i = 0; i < PATH_PREFIX_DESCRIPTION_COUNT; i++)
		{
			size_t match = view.find(PATH_PREFIX_DESCRIPTIONS[i]);
			if (match == 0)
			{
				if (i >= PATH_PREFIX_DESCRIPTION_EXCLUDE_INDEX_START)
				{
					mapType = MapType::MapExclude;
				}
				view.erase(PATH_PREFIX_DESCRIPTIONS[i].size());
				break;
			}
		}

		view = prefix + view;
		insertMapping(view, view, mapType);
	}
}

void FileMap::copyMapApiInto(MapApi& map) const
{
	// MapAPI is poorly written and doesn't declare things as const when it should.
	MapApi* ref = const_cast<MapApi*>(&m_map);

	map.Clear();
	map.SetCaseSensitivity(m_sensitivity);
	for (int i = 0; i < ref->Count(); i++)
	{
		map.Insert(
		    StrBuf(ref->GetLeft(i)->Text()),
		    StrBuf(ref->GetRight(i)->Text()),
		    ref->GetType(i));
	}
}
