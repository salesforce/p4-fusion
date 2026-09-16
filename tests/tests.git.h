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
	git.AddFileToIndex("foo.txt", { 'x', 'y', 'z' }, false);
	std::string commitSHA1 = git.Commit(
	    "//a/b/c/...",
	    "12345678",
	    "test.user",
	    "test@user",
	    0,
	    "Test description",
	    10000000,
	    "");
	TEST(git.IsHEADExists(), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/..."), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/d/..."), false);
	TEST(git.IsRepositoryClonedFrom("//x/y/z/..."), false);
	TEST(git.DetectLatestCL(), "12345678");

	git.RemoveFileFromIndex("foo.txt");
	std::string commitSHA2 = git.Commit(
	    "//a/b/c/...",
	    "12345679",
	    "test.user.2",
	    "test2@user",
	    0,
	    "Test description",
	    20000000,
	    "");
	TEST(git.IsHEADExists(), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/..."), true);
	TEST(git.IsRepositoryClonedFrom("//a/b/c/d/..."), false);
	TEST(git.IsRepositoryClonedFrom("//x/y/z/..."), false);
	TEST(git.DetectLatestCL(), "12345679");

	// CL refs are enabled by default, so both commits should be resolvable by CL number.
	TEST(git.ResolveCL("12345678"), commitSHA1);
	TEST(git.ResolveCL("12345679"), commitSHA2);
	TEST(git.ResolveCL("99999999"), std::string(""));

	git.CloseIndex();

	// With CL refs disabled, no "refs/cl/<cl>" reference should be created.
	GitAPI gitNoRefs(false, false);
	TEST(gitNoRefs.InitializeRepository("/tmp/test-repo-no-cl-refs"), true);
	gitNoRefs.CreateIndex();
	gitNoRefs.AddFileToIndex("foo.txt", { 'x', 'y', 'z' }, false);
	gitNoRefs.Commit(
	    "//a/b/c/...",
	    "22345678",
	    "test.user",
	    "test@user",
	    0,
	    "Test description",
	    10000000,
	    "");
	TEST(gitNoRefs.ResolveCL("22345678"), std::string(""));
	gitNoRefs.CloseIndex();

	TEST_END();
	return TEST_EXIT_CODE();
}
