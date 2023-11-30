/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <thread>

/*
 * ThreadRAII is inspired by the Effective Modern C++ book and wraps a std::thread
 * to join it in the destructor.
 */
class ThreadRAII
{
public:
	ThreadRAII() = default;
	ThreadRAII(std::thread&& t)
	    : t(std::move(t))
	{
	}
	~ThreadRAII();

	// Reinstate the move and copy constructors. They're
	// implicitly removed by having a destructor but they
	// are acceptable.
	ThreadRAII(ThreadRAII&&) = default;
	ThreadRAII& operator=(ThreadRAII&&) = default;

	std::thread& get() { return t; }

private:
	std::thread t;
};
