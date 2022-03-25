/*
 * Copyright (c) 2022, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <iostream>

#include "tests.common.h"
#include "tests.utils.h"
#include "tests.git.h"

int main()
{
	TEST_REPORT("Utils", TestUtils());
	TEST_REPORT("GitAPI", TestGitAPI());

	SUCCESS("All test cases passed");
	return 0;
}
