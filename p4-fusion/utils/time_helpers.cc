#include "time_helpers.h"

int Time::GetTimezoneMinutes(const std::string& timezoneStr)
{
	// string -> ... serverDate 2021/09/06 04:49:28 -0700 PDT
	//                          ^                   ^
	// index  ->                0                  20
	std::string timezone = timezoneStr.substr(20, 5);

	int hours = std::stoi(timezone.substr(1, 2));
	int minutes = std::stoi(timezone.substr(3, 2));
	int sign = timezone[0] == '-' ? -1 : +1;

	return sign * (hours * 60 + minutes);
}
