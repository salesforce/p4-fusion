#!/usr/bin/env bash

# Running this test:
# Provide a P4PORT, P4USER, P4PASSWD and P4CLIENT variable.
# Next, pick a path in the depot you want to test against, and pass it to
# p4-fusion.
# Create a client that points to ./verify/p4datadir with the option rmdir
# enabled.

# This is my client spec:
# Client:	erik-git-p4

# Update:	2023/10/17 02:47:03

# Access:	2023/10/17 03:02:48

# Owner:	admin

# Description:
# 	Used by Erik to ingest repos.

# Root:	/Users/erik/Code/sourcegraph/p4-fusion/verify/p4datadir
# # Note!!! The rmdir option here is very important!
# Options:	noallwrite noclobber nocompress unlocked nomodtime rmdir

# SubmitOptions:	submitunchanged

# LineEnd:	local

# View:
# 	//source/src-cli/... //erik-git-p4/...


set -euo pipefail

ROOT="$(dirname "${BASH_SOURCE[0]}")"

rm -f "${ROOT}/verify/p4fusion.log"
rm -rf "${ROOT}/verify/gitout"
rm -rf "${ROOT}/verify/p4home"
rm -rf "${ROOT}/verify/datadir"
rm -rf "${ROOT}/verify/p4datadir"

mkdir -p "${ROOT}/verify/gitout"
mkdir -p "${ROOT}/verify/p4home"
mkdir -p "${ROOT}/verify/datadir"
mkdir -p "${ROOT}/verify/p4datadir"

./generate_cache.sh Debug

./build.sh

export P4USER=<PROVIDE_ME>
export P4PORT=<PROVIDE_ME>
export P4PASSWD=<PROVIDE_ME>
export HOME=./verify/p4home

./build/p4-fusion/p4-fusion \
    --path //source/src-cli/... \
    --client '' \
    --user admin \
    --src "${ROOT}/verify/gitout" \
    --networkThreads 64 \
    --port <PROVIDE_ME> \
    --lookAhead 15000 \
    --printBatch 1000 \
    --noBaseCommit true \
    --retries 10 \
    --refresh 1000 \
    --maxChanges -1 \
    --includeBinaries true \
    --fsyncEnable true \
    --noColor true 2>&1 | tee ./verify/p4fusion.log

export P4CLIENT=<PROVIDE_ME>

./validate-migration.sh \
    --force \
    --debug \
    --logfile="${ROOT}/verify/p4fusion.log" \
    --p4workdir="${ROOT}/verify/p4datadir" \
    --gitdir="${ROOT}/verify/gitout" \
    --datadir="${ROOT}/verify/datadir"
