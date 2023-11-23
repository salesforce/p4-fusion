/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "info_result.h"
#include "utils/time_helpers.h"

InfoResult::InfoResult()
    : m_TimezoneMinutes(0)
{
}

void InfoResult::OutputStat(StrDict* varList)
{
	std::string serverDate(varList->GetVar("serverDate")->Text());
	m_TimezoneMinutes = Time::GetTimezoneMinutes(serverDate);
}
