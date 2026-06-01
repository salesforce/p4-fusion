/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "time_helpers.h"

#include <cctype>
#include <iostream>

#include "log.h"

int Time::GetTimezoneMinutes(const std::string& timezoneStr)
{
	// Expected format: "YYYY/MM/DD HH:MM:SS [+-]HHMM TZ"
	// The timezone sign is at index 20, digits at 21-24.
	if (timezoneStr.size() < 25)
	{
		WARN("Could not parse timezone from serverDate: '" << timezoneStr << "'. Defaulting to UTC.");
		return 0;
	}

	char c = timezoneStr[20];
	if ((c != '+' && c != '-') || !std::isdigit(static_cast<unsigned char>(timezoneStr[21])) || !std::isdigit(static_cast<unsigned char>(timezoneStr[22])) || !std::isdigit(static_cast<unsigned char>(timezoneStr[23])) || !std::isdigit(static_cast<unsigned char>(timezoneStr[24])))
	{
		WARN("Could not parse timezone from serverDate: '" << timezoneStr << "'. Defaulting to UTC.");
		return 0;
	}

	int sign = (c == '-') ? -1 : 1;
	int hours = (timezoneStr[21] - '0') * 10 + (timezoneStr[22] - '0');
	int minutes = (timezoneStr[23] - '0') * 10 + (timezoneStr[24] - '0');
	return sign * (hours * 60 + minutes);
}
