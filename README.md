# P4 Fusion

A fast Perforce depot to Git repository converter using the Helix Core C/C++ API as an attempt to mitigate the performance bottlenecks in [git-p4.py](https://github.com/git/git/blob/master/git-p4.py).

This project was started as a proof of concept for an internal project which required converting P4 depots to Git repositories. A similar solution exists within Git, known as [git-p4.py](https://github.com/git/git/blob/master/git-p4.py), however, it has (as of the time of writing) performance issues with any depot greater than 1 GB in size, and it runs in a single thread using Python2 which adds another set of limitations to the use of git-p4.py for larger use-cases.

This tool solves some of the most impactful scaling and performance limitations in git-p4.py by:

* Using the [Helix Core C++ API](https://www.perforce.com/downloads/helix-core-c/c-api) to handle downloading CLs with more control over the memory and how it is committed to the Git repo without unnecessary memory copies and file I/O.
* Using [libgit2](https://libgit2.org/) to forward the file contents received from the Perforce server as-is to a Git repository, while avoiding memory copies as much as possible. This library allows creating commits from file contents existing plainly in memory.
* Using a custom wakeup-based threadpool implemented in C++11 that runs thread-local library contexts of the Helix Core C++ API to heavily multithread the changelist downloading process.

## Performance

Please be aware that this tool is fast enough to instantaneously generate a tremendous amount of load (more than 150K requests in a few minutes if running with a moderate number of threads) on your Perforce server, thus, it needs careful monitoring to ensure that the Perforce server does not get impacted. This tool will continue generating load without any rate limitations (apart from the runtime options that this tool provides) till the conversion process is complete. However, having no rate limits and running this tool with several hundred network threads (or more if possible) is the ideal case for achieving maximum speed in the conversion process.

The number of network threads shall be set to a number generally be more than the number of logical CPUs because the most time-taking step is downloading the CL data, which is mostly network I/O bounded.

In our study, this tool is running upwards of 100 times faster than git-p4.py. We have observed an average time of 26 seconds for the conversion of the history inside a depot path containing around 3393 moderately sized changelists using 200 parallel connections, while git-p4.py was taking close to 42 minutes to convert the same depot path. If the Perforce server has the files cached completely then these conversion times might be reproducible, else if the file cache is empty then the first couple of runs are expected to take much more time.

These execution times are expected to scale as expected with larger depots (millions of CLs or more). The tool provides options to control the memory utilization during the conversion process so these options shall help in larger use-cases.

## Usage

```shell
❯ ./build/p4-fusion/p4-fusion
[ PRINT @ Main:24 ] Running p4-fusion from: ./build/p4-fusion/p4-fusion
[ PRINT @ Main:43 ] Usage:
[Required] --port
        Specify which P4PORT to use.

[Required] --path
        P4 depot path to convert to a Git repo

[Required] --lookAhead
        How many CLs in the future, at most, shall we keep downloaded by the time it is to commit them?

[Required] --src
        Local relative source path with P4 code. Git repo will be created at this path. This path should be empty before running p4-fusion.

[Required] --client
        Name/path of the client workspace specification.

[Required] --user
        Specify which P4USER to use. Please ensure that the user is logged in.

[Optional, Default is false] --includeBinaries
        Don't discard binary files while downloading changelists.

[Optional, Default is false] --fsyncEnable
        Enable fsync() while writing objects to disk to ensure they get written to permanent storage immediately instead of being cached. This is to mitigate data loss in events of hardware failure.

[Optional, Default is 10] --retries
        Specify how many times a command should be retried before the process exits in a failure.

[Optional, Default is 16] --networkThreads
        Specify the number of threads in the threadpool for running network calls. Defaults to the number of logical CPUs.

[Optional, Default is -1] --maxChanges
        Specify the max number of changelists which should be processed in a single run. -1 signifies unlimited range.

[Optional, Default is 1] --printBatch
        Specify the p4 print batch size.

[Optional, Default is 100] --refresh
        Specify how many times a connection should be reused before it is refreshed.
```

## Build

0. Pre-requisites
  * Install openssl@1.0.2t at `/usr/local/ssl` by following the steps [here](https://askubuntu.com/a/1094690).
  * Install CMake 3.16+.
  * Install g++ 11.2.0 (older versions compatible with C++11 are also supported).
  * Clone this repository or [get a release distribution](https://github.com/salesforce/p4-fusion/releases).
  * Get Helix Core C++ API from the [official Perforce distribution](https://www.perforce.com/downloads/helix-core-c/c-api). Version 2021.1/2179737 shall have the best compatibility. Extract the contents in `./helix-core-api/linux/` or `./helix-core-api/mac/` based on your OS.

> For CentOS, you can try `yum install git make cmake gcc-c++ libarchive` to set up the compilation toolchain. Installing `libarchive` is only required to fix a bug that stops CMake from starting properly.

This tool uses C++11 and thus it should work with much older GCC versions. We have tested compiling with both GCC 11.2.0 and GCC 4.8.

1. Generate a CMake cache

```shell
./generate_cache.sh Debug
```

> Replace `Debug` with `Release` for a speed-optimized binary. Debug builds will run marginally slower (considering the tool is mostly bottlenecked by network I/O) but will contain debug symbols and allow a better debugging experience while working with a debugger like gdb.

2. Build

```shell
./build.sh
```

3. Run!

```shell
./build/p4-fusion/p4-fusion --path //depot/path/... --user $P4USER --port $P4PORT --client $P4CLIENT --src clones/.git --networkThreads 200 --printBatch 100 --lookAhead 2000 --retries 10 --refresh 100
```

There should be a Git repo being created in the `clones/` directory with commits being created as the tool runs.

> Note: The Git repository is created as bare and running the same command again shall detect the last committed CL and only continue from that CL onwards. We also don't handle binaries by default and `.git` directories in the Perforce history.

## Contributing

Please refer to [CONTRIBUTING.md](CONTRIBUTING.md)

---

Licensed under the BSD 3-Clause License. Third-party license attributions are present in [THIRDPARTY.md](THIRDPARTY.md).
