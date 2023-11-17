#!/usr/bin/env bash

SCRIPT_ROOT="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
cd "${SCRIPT_ROOT}/../../.."

export TEMPLATES="${SCRIPT_ROOT}/templates"

set -euxo pipefail

export P4USER="${P4USER:-"admin"}"                       # the name of the Perforce superuser that the script will use to create the depot
export P4PORT="${P4PORT:-"ssl:perforce.sgdev.org:1666"}" # the address of the Perforce server to connect to
export P4CLIENT="${P4CLIENT:-"integration-test-client"}" # the name of the temporary client that the script will use while it creates the depot

export NUM_NETWORK_THREADS="${NUM_NETWORK_THREADS:-"64"}" # the number of network threads to use when running p4-fusion
export DEPOT_NAME="${DEPOT_NAME:-"source/src-cli"}"      # the name of the depot that the script will create on the server

TMP="$(mktemp -d)"
export DEPOT_DIR="${TMP}/${DEPOT_NAME}"
export GIT_DEPOT_DIR="${TMP}/git-${DEPOT_NAME}"
export P4_FUSION_LOG="${TMP}/p4-fusion.log"

# Trust our perforce server.
p4 trust -f -y

cleanup() {
  # ensure that we don't leave a client behind (using up one of our licenses)
  "${SCRIPT_ROOT}/delete-perforce-client.sh"

  # delete temp folders
  rm -rf "${TMP}" || true
}
trap cleanup EXIT

echo "::group::{Ensure that Perforce credentials are valid}"
{
  if ! p4 login -s &>/dev/null; then
    handbook_link="https://handbook.sourcegraph.com/departments/ce-support/support/process/p4-enablement/#generate-a-session-ticket"
    address="${P4USER}:${P4PORT}"

    cat <<END
'p4 login -s' command failed. This indicates that you might not be logged into '$address'.
Try using 'p4 -u ${P4USER} login -a' to generate a session ticket.
See '${handbook_link}' for more information.
END

    exit 1
  fi
}

echo "::endgroup::"

echo "::group::{Create temporary Perforce client}"
{
  printf "(re)creating temporary client '%s'..." "$P4CLIENT"

  # delete older copy of client (if it exists)
  "${SCRIPT_ROOT}/delete-perforce-client.sh"

  # create new client
  P4_CLIENT_HOST="$(hostname)" envsubst <"${TEMPLATES}/client.tmpl" | p4 client -i

  printf "done\n"
}
echo "::endgroup::"

echo "::group::{Build P4 Fusion}"
{
  time (./generate_cache.sh Debug && ./build.sh)
}
echo "::endgroup::"

echo "::group::{Run p4-fusion against the downloaded depot}"
{
  P4_FUSION_ARGS=(
    --path "//${DEPOT_NAME}/..."
    --client "${P4CLIENT}"
    --user "$P4USER"
    --src "${GIT_DEPOT_DIR}"
    --networkThreads "${NUM_NETWORK_THREADS}"
    --port "${P4PORT}"
    --lookAhead 15000
    --printBatch 1000
    --noBaseCommit true
    --retries 10
    --refresh 1000
    --maxChanges -1
    --includeBinaries true
    --fsyncEnable true
    --noColor true
  )

  if [[ "${USE_VALGRIND:-"false"}" == "true" ]]; then
    # run p4-fusion under valgrind

    VALGRIND_ARGS=(
      --fair-sched=yes    # see https://valgrind.org/docs/manual/manual-core.html#manual-core.pthreads: unsure if this makes much of a difference in pratice
      --error-exitcode=99 # tell valgrind to exit if it finds an issue
      --verbose
    )

    # fill in extra flags provided by action runner
    read -r -a extra_valgrind_args <<<"${VALGRIND_FLAGS:-}"
    for flag in "${extra_valgrind_args[@]}"; do
      if [[ -n "${flag}" ]]; then
        VALGRIND_ARGS+=("$flag")
      fi
    done

    time valgrind "${VALGRIND_ARGS[@]}" ./build/p4-fusion/p4-fusion "${P4_FUSION_ARGS[@]}" | tee "${P4_FUSION_LOG}"
  else
    # run p4-fusion normally
    time ./build/p4-fusion/p4-fusion "${P4_FUSION_ARGS[@]}" | tee "${P4_FUSION_LOG}"
  fi
}

echo "::endgroup::"

echo "::group::{Run validation on migrated data}"
{
  time ./validate-migration.sh \
    --force \
    --debug \
    --logfile="${P4_FUSION_LOG}" \
    --p4workdir="${DEPOT_DIR}" \
    --gitdir="${GIT_DEPOT_DIR}" \
    --datadir="${TMP}/datadir"
}
echo "::endgroup::"
