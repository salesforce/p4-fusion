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
	std::unique_lock<std::mutex> lock(*mu);

	MapApi argMap;
	argMap.SetCaseSensitivity(m_sensitivity);
	argMap.Insert(StrBuf(fileRevision.c_str()), MapType::MapInclude);

	// MapAPI is poorly written and doesn't declare things as const when it should.
	MapApi* joined = MapApi::Join(const_cast<MapApi*>(&m_map), &argMap);
	bool isInLeft = joined != nullptr;
	// We have to manually clean up the returned map.
	delete joined;
	return isInLeft;
}

void FileMap::insertMapping(const std::string& left, const std::string& right, const MapType mapType)
{
	std::string mapStrLeft = left;
	mapStrLeft.erase(mapStrLeft.find_last_not_of(' ') + 1);
	mapStrLeft.erase(0, mapStrLeft.find_first_not_of(' '));

	std::string mapStrRight = right;
	mapStrRight.erase(mapStrRight.find_last_not_of(' ') + 1);
	mapStrRight.erase(0, mapStrRight.find_first_not_of(' '));

	std::unique_lock<std::mutex> lock(*mu);

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

const std::vector<std::string> PATH_PREFIX_DESCRIPTIONS = {
	// Order is important.
	"share ",
	"isolate ",
	"import+ ",
	"import ",
	"exclude "
};

void FileMap::copyMapApiInto(MapApi& map) const
{
	std::unique_lock<std::mutex> lock(*mu);

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
