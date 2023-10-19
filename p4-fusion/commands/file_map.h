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
	std::shared_ptr<std::mutex> mu = std::make_shared<std::mutex>();

	void insertMapping(const std::string& left, const std::string& right, const MapType mapType);
	void copyMapApiInto(MapApi& map) const;

public:
	FileMap();
	FileMap(const FileMap& src);

	bool IsInLeft(const std::string fileRevision) const;

	MapCase GetCaseSensitivity() const { return m_sensitivity; };

	// "//a/... b/..." format
	void InsertTranslationMapping(const std::vector<std::string>& mapping);
};
