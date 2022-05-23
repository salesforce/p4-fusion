/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <iostream>

#include "test.common.h"
#include "test.utils.h"
#include "test.git.h"
#include "test.perforce.h"

int main()
{
	TEST_REPORT("Utils", TestUtils());
	TEST_REPORT("GitAPI", TestGitAPI());
	TEST_REPORT("P4API", TestP4API());

	SUCCESS("All test cases passed");
	return 0;
}
