# p4-fusion

[![build-check](https://github.com/salesforce/p4-fusion/actions/workflows/build.yaml/badge.svg)](https://github.com/salesforce/p4-fusion/actions/workflows/build.yaml)
[![format-check](https://github.com/salesforce/p4-fusion/actions/workflows/format.yaml/badge.svg)](https://github.com/salesforce/p4-fusion/actions/workflows/format.yaml)

A fast Perforce depot to Git repository converter using the Helix Core C/C++ API as an attempt to mitigate the performance bottlenecks in [git-p4.py](https://github.com/git/git/blob/master/git-p4.py).

This project was started as a proof of concept for an internal project which required converting P4 depots to Git repositories. A similar solution exists within Git, known as [git-p4.py](https://github.com/git/git/blob/master/git-p4.py), however, it has (as of the time of writing) performance issues with any depot greater than 1 GB in size, and it runs in a single thread using Python2 which adds another set of limitations to the use of git-p4.py for larger use-cases.

This tool solves some of the most impactful scaling and performance limitations in git-p4.py by:

* Using the [Helix Core C++ API](https://www.perforce.com/downloads/helix-core-c/c-api) to handle downloading CLs with more control over the memory and how it is committed to the Git repo without unnecessary memory copies and file I/O.
* Using [libgit2](https://libgit2.org/) to forward the file contents received from the Perforce server as-is to a Git repository, while avoiding memory copies as much as possible. This library allows creating commits from file contents existing plainly in memory.
* Using a custom wakeup-based threadpool implemented in C++11 that runs thread-local library contexts of the Helix Core C++ API to heavily multithread the changelist downloading process.

## Performance

Please be aware that this tool is fast enough to instantaneously generate a tremendous amount of load on your Perforce server (more than 150K requests in a few seconds if running with a couple hundred network threads). Since p4-fusion will continue generating load within the limits set using the runtime arguments, it needs careful monitoring to ensure that your Perforce server does not get impacted.

However, having no rate limits and running this tool with several hundred network threads (or more if possible) is the ideal case for achieving maximum speed in the conversion process.

The number of network threads should be set to a number that generally is much more than the number of logical CPUs because the most time-taking step is a low CPU intensive task i.e. downloading the CL data from the Perforce server.

In our study, this tool is running upwards of 100 times faster than git-p4.py. We have observed an average time of 26 seconds for the conversion of the history inside a depot path containing around 3393 moderately sized changelists using 200 parallel connections, while git-p4.py was taking close to 42 minutes to convert the same depot path. If the Perforce server has the files cached completely then these conversion times might be reproducible, else if the file cache is empty then the first couple of runs are expected to take much more time.

These execution times are expected to scale as expected with larger depots (millions of CLs or more). The tool provides options to control the memory utilization during the conversion process so these options shall help in larger use-cases.

## Usage

```shell
[ PRINT @ Main:56 ] Usage:
--client [Required]
        Name/path of the client workspace specification.

--flushRate [Optional, Default is 1000]
        Rate at which profiling data is flushed on the disk.

--fsyncEnable [Optional, Default is false]
        Enable fsync() while writing objects to disk to ensure they get written to permanent storage immediately instead of being cached. This is to mitigate data loss in events of hardware failure.

--includeBinaries [Optional, Default is false]
        Do not discard binary files while downloading changelists.

--lookAhead [Required]
        How many CLs in the future, at most, shall we keep downloaded by the time it is to commit them?

--branch [Optional]
        A branch to migrate under the depot path.  May be specified more than once.  If at least one is given and the noMerge option is false, then the Git repository will include merges between branches in the history.  You may use the formatting 'depot/path:git-alias', separating the Perforce branch sub-path from the git alias name by a ':'; if the depot path contains a ':', then you must provide the git branch alias.

--noMerge [Optional, Default is false]
        When false and at least one branch is given, then .  If this is true, then the Git history will not contain any merges, except for an artificial empty commit added at the root, which acts as a common source to make later merges easier.

--maxChanges [Optional, Default is -1]
        Specify the max number of changelists which should be processed in a single run. -1 signifies unlimited range.

--networkThreads [Optional, Default is 16]
        Specify the number of threads in the threadpool for running network calls. Defaults to the number of logical CPUs.

--noColor [Optional, Default is false]
        Disable colored output.

--path [Required]
        P4 depot path to convert to a Git repo

--port [Required]
        Specify which P4PORT to use.

--printBatch [Optional, Default is 1]
        Specify the p4 print batch size.

--refresh [Optional, Default is 100]
        Specify how many times a connection should be reused before it is refreshed.

--retries [Optional, Default is 10]
        Specify how many times a command should be retried before the process exits in a failure.

--src [Required]
        Relative path where the git repository should be created. This path should be empty before running p4-fusion for the first time in a directory.

--user [Required]
        Specify which P4USER to use. Please ensure that the user is logged in.
```

## Notes On Branches

When at least one branch argument exists, the tool will enable branching mode.

Branching mode currently only supports very simple branch layouts.  The format must be `//common/depot/path/branch-name`.  The common depot path is given as the `--path` argument, and each `--branch` argument specifies one branch name to inspect.  Branch names must be a directory name immediately after the path (it replaces the `...`).

In branching mode, the generated Git repository will be initially populated with a zero-content commit.  This allows branches to later be merged without needing the `--allow-unrelated-histories` flag in Git.  All branches will have this in their history.

If a Perforce changelist contains an integration like action (move, integrate, copy, etc.) from another branch listed in a `--branch` argument, then the tool will mark the Git commit with the integration as having two parents - the current branch and the source branch.  If a changelist contains integrations into one branch from multiple other branches, they are put into separate commits, each with just one source branch.  If a changelist contains integrations into multiple branches, then each one of those is also its own commit.

Because Perforce integration isn't a 1-to-1 mapping onto Git merge, there can be situations where having the tool mark a commit as a merge, but not bringing over all the changes, leads to later merge logic not picking up every changed file correctly.  To avoid this situation, the `--noMerge true` will ensure they only have the single zero-content root commit shared, so any merge done after the migration will force full file tree inspection.

If the Perforce tree contains sub-branches, such as `//base/tree/sub` being a sub-branch of `//base/tree`, then you can use the arguments `--path //base/... --branch tree/sub:tree-sub --branch tree`.  The ordering is important here - provide the deeper paths first to have them take priority over the others.  Because Git creates branches with '/' characters as implicit directories, you must provide the Git branch alias to prevent Git reporting an error where the branch "tree" can't be created because is already a directory, or "tree/sub" can't be created because "tree" isn't a directory.

## Checking Results

In order to test the validity of the logic, we need to run the program over a Perforce depot and compare each changelist against the corresponding Git commit SHA, to ensure the files match up.

The provided script [validate-migration.sh](validate-migration.sh) runs through every generated Git commit, and ensures the file state exactly matches the state of the Perforce depot.

Because of the extra effort the script performs, expect it to take orders of magnitude longer than the original p4-fusion execution.

## Build

0. Pre-requisites
  * Install openssl@1.0.2t at `/usr/local/ssl` by following the steps [here](https://askubuntu.com/a/1094690).
  * Install CMake 3.16+.
  * Install g++ 11.2.0 (older versions compatible with C++11 are also supported).
  * Clone this repository or [get a release distribution](https://github.com/salesforce/p4-fusion/releases).
  * Get the Helix Core C++ API binaries from the [official Perforce website](https://www.perforce.com/downloads/helix-core-c/c-api).
    * Tested versions: 2021.1, 2021.2, 2022.1
    * We recommend always picking the newest API versions that compile with p4-fusion.
  * Extract the contents in `./vendor/helix-core-api/linux/` or `./vendor/helix-core-api/mac/` based on your OS.

> For CentOS, you can try `yum install git make cmake gcc-c++ libarchive` to set up the compilation toolchain. Installing `libarchive` is only required to fix a bug that stops CMake from starting properly.

This tool uses C++11 and thus it should work with much older GCC versions. We have tested compiling with both GCC 11.2.0 and GCC 4.8.

1. Generate a CMake cache

```shell
./generate_cache.sh Debug
```

Replace `Debug` with `Release` or `RelWithDebInfo` or `MinSizeRel` for a differently optimized binary. Debug will run marginally slower (considering the tool is mostly bottlenecked by network I/O) but will contain debug symbols and allows a better debugging experience while working with a debugger.

By default tracing is disabled in p4-fusion. It can be enabled by including `p` in the second argument while generating the CMake cache. If tracing is enabled, p4-fusion generates trace JSON files in the cloning directory. These files can be opened in the `about:tracing` window in Chromium web browsers to view the tracing data.

Tests can be enabled by including `t` in the second command argument.

E.g. You can build tests and at the same time enable profiling by running `./generate_cache.sh Debug pt`.

2. Build

```shell
./build.sh
```

3. Run!

```shell
./build/p4-fusion/p4-fusion \
        --path //depot/path/... \
        --user $P4USER \
        --port $P4PORT \
        --client $P4CLIENT \
        --src clones/.git \
        --networkThreads 200 \
        --printBatch 100 \
        --lookAhead 2000 \
        --retries 10 \
        --refresh 100
```

There should be a Git repo being created in the `clones/.git` directory with commits being created as the tool runs.

> Note: The Git repository is created bare i.e. without a working directory and running the same command again shall detect the last committed CL and only continue from that CL onwards. Binaries files are ignored by default and this behaviour can be changed by using the `--includeBinaries` option. We do not handle `.git` directories in the Perforce history.

## Contributing

Please refer to [CONTRIBUTING.md](CONTRIBUTING.md)

---

Licensed under the BSD 3-Clause License. Third-party license attributions are present in [THIRDPARTY.md](THIRDPARTY.md).
