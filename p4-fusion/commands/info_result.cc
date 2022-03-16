/*
 * Copyright (c) 2022, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "info_result.h"

InfoResult::InfoResult()
    : m_TimezoneMinutes(0)
{
}

void InfoResult::OutputStat(StrDict* varList)
{
	std::string serverDate = varList->GetVar("serverDate")->Text();

	// ... serverDate 2021/09/06 04:49:28 -0700 PDT
	//                ^                   ^
	//                0                  20
	std::string timezone = serverDate.substr(20, 5);

	int hours = std::stoi(timezone.substr(1, 2));
	int minutes = std::stoi(timezone.substr(3, 2));
	int sign = timezone[0] == '-' ? -1 : +1;

	m_TimezoneMinutes = sign * (hours * 60 + minutes);
}
