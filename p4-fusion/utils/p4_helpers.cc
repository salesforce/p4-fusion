/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "p4_helpers.h"

#include "std_helpers.h"

std::string P4Unescape(std::string p4path)
{
	STDHelpers::ReplaceAll(p4path, "%40", "@");
	STDHelpers::ReplaceAll(p4path, "%23", "#");
	STDHelpers::ReplaceAll(p4path, "%2A", "*");
	// Replace %25 last to avoid interfering with other escape sequences
	STDHelpers::ReplaceAll(p4path, "%25", "%");
	return p4path;
}