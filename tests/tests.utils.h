#pragma once

#include "tests.common.h"
#include "utils/std_helpers.h"
#include "utils/time_helpers.h"

int TestUtils()
{
	TEST_START();

	TEST(STDHelpers::Contains("abc", "a"), true);
	TEST(STDHelpers::Contains("abc", "abcd"), false);
	TEST(STDHelpers::Contains("abc", ""), true);
	TEST(STDHelpers::Contains("", "abcd"), false);
	TEST(STDHelpers::Contains("", ""), true);
	TEST(STDHelpers::Contains("//depot/path/.git/config", "/.git/"), true);
	TEST(STDHelpers::Contains("//depot/path/.git/config", ".git"), true);
	TEST(STDHelpers::Contains("//depot/path/.git", "/.git"), true);
	TEST(STDHelpers::Contains("//depot/path/.git", "/.git/"), false);
	TEST(STDHelpers::Contains("//depot/path/.git", ".git"), true);
	TEST(STDHelpers::Contains("//.git", ".git"), true);
	TEST(STDHelpers::Contains("//", "/.git/"), false);
	TEST(STDHelpers::Contains("//", ".git"), false);

	TEST(STDHelpers::StartsWith("abc", "a"), true);
	TEST(STDHelpers::StartsWith("abc", "b"), false);
	TEST(STDHelpers::StartsWith("ab", "abc"), false);
	TEST(STDHelpers::StartsWith("ab", "abc"), false);
	TEST(STDHelpers::StartsWith("//depot/path/", "//"), true);
	TEST(STDHelpers::StartsWith("//depot/path/", "//depot"), true);
	TEST(STDHelpers::StartsWith("//depot/path/", "//_depot"), false);

	TEST(STDHelpers::EndsWith("abc", "abc"), true);
	TEST(STDHelpers::EndsWith("abc", "ab"), false);
	TEST(STDHelpers::EndsWith("abc", "a"), false);
	TEST(STDHelpers::EndsWith("abc", "abcd"), false);
	TEST(STDHelpers::EndsWith("abc", "z"), false);
	TEST(STDHelpers::EndsWith("//...", "/..."), true);
	TEST(STDHelpers::EndsWith("//", "/..."), false);
	TEST(STDHelpers::EndsWith("", "/..."), false);
	TEST(STDHelpers::EndsWith("", ""), true);
	TEST(STDHelpers::EndsWith("", "/..."), false);
	TEST(STDHelpers::EndsWith("//depot/path/", "/..."), false);
	TEST(STDHelpers::EndsWith("//depot/path", "/..."), false);
	TEST(STDHelpers::EndsWith("//depot/path/...", "/..."), true);
	TEST(STDHelpers::EndsWith("//depot/path", ".git"), false);
	TEST(STDHelpers::EndsWith("//depot/path/.git/config", ".git"), false);
	TEST(STDHelpers::EndsWith("//depot/path/.git", "/.git"), true);
	TEST(STDHelpers::EndsWith("//depot/path/.git", "/.git/"), false);
	TEST(STDHelpers::EndsWith("//depot/path/.git", ".git"), true);

	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -0800 PST"), -480);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +0800 PST"), +480);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -1800 PST"), -1080);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +1800 PST"), +1080);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +0530 IST"), +330);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -1234 XXX"), -754);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +1234 XXX"), +754);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +0000 GMT"), +0);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -0000 GMT"), +0);

	TEST_END();
	return TEST_EXIT_CODE();
}
