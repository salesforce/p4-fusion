/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "thread.h"
#include "log.h"

ThreadRAII::~ThreadRAII()
{
	if (t.joinable())
	{
		SUCCESS("Joining thread on exit")
		t.join();
	}
}