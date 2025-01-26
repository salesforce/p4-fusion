/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <array>
#include <vector>
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

	{
		std::string depotPath = "//depot/path/";
		std::string filePath = "//depot/path/foo/bar/crash.txt";
		STDHelpers::Erase(filePath, depotPath);

		TEST(filePath == "foo/bar/crash.txt", true);
	}
	{
		std::string depotPath = "//depot/path/...";
		std::string filePath = "//depot/path/foo/bar/crash.txt";
		STDHelpers::Erase(filePath, depotPath);

		TEST(filePath == "//depot/path/foo/bar/crash.txt", true);
		TEST(filePath == "foo/bar/crash.txt", false);
	}

	TEST(Time::GetTimezoneMinutes("2022/03/15 09:56:15 -0400 EDT"), -240);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -0800 PST"), -480);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +0800 PST"), +480);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -1800 PST"), -1080);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +1800 PST"), +1080);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +0530 IST"), +330);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -1234 XXX"), -754);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +1234 XXX"), +754);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 +0000 GMT"), +0);
	TEST(Time::GetTimezoneMinutes("2022/03/09 22:59:04 -0000 GMT"), +0);

	{
		auto actual = STDHelpers::SplitAt("depot/path/some/other", '/');
		auto expected = std::array<std::string, 2> { "depot", "path/some/other" };
		TEST(actual, expected);
	}
	{
		auto actual = STDHelpers::SplitAt("//depot/path", '/', 2);
		auto expected = std::array<std::string, 2> { "depot", "path" };
		TEST(actual, expected);
	}

	{
		auto actual = STDHelpers::SplitOnDelim("Foo Bar Baz", ' ');
		auto expected = std::vector<std::string> { "Foo", "Bar", "Baz" };
		TEST(actual, expected);
	}
	{
		auto actual = STDHelpers::SplitOnDelim("somestring", ' ');
		auto expected = std::vector<std::string> { "somestring" };
		TEST(actual, expected);
	}
	{
		auto actual = STDHelpers::SplitOnDelim("test  string", ' ');
		auto expected = std::vector<std::string> { "test", "", "string" };
		TEST(actual, expected);
	}

	TEST_END();
	return TEST_EXIT_CODE();
}
