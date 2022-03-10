#include "common.h"

#define TEST_START() \
	static int32_t failedTestCaseCount = 0;

#define TEST(value, expected)                       \
	if (value == expected)                          \
	{                                               \
		PRINT(#value << " == " << #expected);       \
	}                                               \
	else                                            \
	{                                               \
		failedTestCaseCount++;                      \
		ERR(#value << " == " << value << " but != " \
		           << #expected << " (expected)");  \
	}

#define TEST_END()                                          \
	if (failedTestCaseCount != 0)                           \
	{                                                       \
		ERR(failedTestCaseCount << " tests failed so far"); \
	}

#define TEST_EXIT_CODE() failedTestCaseCount

#define TEST_REPORT(testName, functionCall)                       \
	do                                                            \
	{                                                             \
		auto result = functionCall;                               \
		if (result == 0)                                          \
		{                                                         \
			SUCCESS(testName << " tests succeeded");              \
		}                                                         \
		else                                                      \
		{                                                         \
			ERR(testName << " failed with exit code " << result); \
			return result;                                        \
		}                                                         \
	} while (false)
