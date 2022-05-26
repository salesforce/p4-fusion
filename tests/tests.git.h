/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "tests.common.h"
#include "git_api.h"

int TestGitAPI()
{
	TEST_START();

	GitAPI git(false);

	TEST(git.InitializeRepository("/tmp/test-repo"), true);
	git.CreateIndex();
	git.AddFileToIndex("//a/b/c/...", "//a/b/c/foo.txt", { 'x', 'y', 'z' }, false);
	git.Commit(
	    "//a/b/c/...",
	    "12345678",
	    "test.user",
	    "test@user",
	    0,
	    "Test description",
	    10000000);
	TEST(git.IsHEADExists(), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/..."), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/d/..."), false);
	TEST(git.IsRepositoryClonedFrom("//x/y/z/..."), false);
	TEST(git.DetectLatestCL(), "12345678");

	git.RemoveFileFromIndex("//a/b/c/...", "//a/b/c/foo.txt");
	git.Commit(
	    "//a/b/c/...",
	    "12345679",
	    "test.user.2",
	    "test2@user",
	    0,
	    "Test description",
	    20000000);
	TEST(git.IsHEADExists(), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/..."), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/d/..."), false);
	TEST(git.IsRepositoryClonedFrom("//x/y/z/..."), false);
	TEST(git.DetectLatestCL(), "12345679");

	git.CloseIndex();

	TEST_END();
	return TEST_EXIT_CODE();
}
