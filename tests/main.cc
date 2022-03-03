#include <iostream>

#include "common.h"
#include "utils/std_helpers.h"

#define TEST_START()                        \
	static int32_t failedTestCaseCount = 0; \
	SUCCESS("Running p4-fusion tests...");

#define TEST(value, expected)                 \
	if (value == expected)                    \
	{                                         \
		PRINT(#value << " == " << #expected); \
	}                                         \
	else                                      \
	{                                         \
		failedTestCaseCount++;                \
		ERR(#value << " == " << #expected);   \
	}

#define TEST_END()                                        \
	if (failedTestCaseCount == 0)                         \
	{                                                     \
		SUCCESS("All test cases passed successfully");    \
	}                                                     \
	else                                                  \
	{                                                     \
		ERR(failedTestCaseCount << " tests are failing"); \
	}

#define TEST_EXIT_CODE() failedTestCaseCount

int main()
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

	TEST_END();

	return TEST_EXIT_CODE();
}
