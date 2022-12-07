/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#pragma once

#include <vector>
#include <string>

#include "common.h"

struct FileMap
{
private:
	MapApi m_map;
	MapCase m_sensitivity;

	void insertMapping(const std::string& left, const std::string& right, const MapType mapType);
	void copyMapApiInto(MapApi& map) const;

public:
	FileMap();
	FileMap(const FileMap& src);

	bool IsInLeft(const std::string fileRevision) const;
	bool IsInRight(const std::string fileRevision) const;

	void SetCaseSensitivity(const MapCase mode);
	MapCase GetCaseSensitivity() const { return m_sensitivity; };

	// TranslateLeftToRight turn a left to a right.
	// Returns an empty string if the mapping is invalid.
	std::string TranslateLeftToRight(const std::string& path) const;

	// TranslateRightToLeft turn a right to a left.
	// Returns an empty string if the mapping is invalid.
	std::string TranslateRightToLeft(const std::string& path) const;

	// "//a/... b/..." format
	void InsertTranslationMapping(const std::vector<std::string>& mapping);

	// "//a/..." format
	void InsertPaths(const std::vector<std::string>& paths);

	// "..." format
	void InsertPrefixedPaths(const std::string prefix, const std::vector<std::string>& paths);

	void InsertFileMap(const FileMap& src);
};
