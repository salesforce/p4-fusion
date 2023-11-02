#!/usr/bin/env bash

SCRIPT_ROOT="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
cd "${SCRIPT_ROOT}/../.."

export TEMPLATES="${SCRIPT_ROOT}/templates"

set -euxo pipefail

export P4USER="${P4USER:-"admin"}"                   # the name of the Perforce superuser that the script will use to create the depot
export P4PORT="${P4PORT:-"perforce.sgdev.org:1666"}" # the address of the Perforce server to connect to

export DEPOT_NAME="${DEPOT_NAME:-"source/src-cli"}"      # the name of the depot that the script will create on the server
export P4CLIENT="${P4CLIENT:-"integration-test-client"}" # the name of the temporary client that the script will use while it creates the depot

TMP="$(mktemp -d)"
export DEPOT_DIR="${TMP}/${DEPOT_NAME}"
export GIT_DEPOT_DIR="${TMP}/git-${DEPOT_NAME}"
export P4_FUSION_LOG="${TMP}/p4-fusion.log"

cleanup() {
  # ensure that we don't leave a client behind (using up one of our licenses)
  delete_perforce_client

  # delete temp folders
  rm -rf "${TMP}" || true
}
trap cleanup EXIT

# delete_perforce_client deletes the client specified by "$P4CLIENT"
# if it exists on the Perforce server.
#
## P4 CLI reference(s):
##
## https://www.perforce.com/manuals/cmdref/Content/CmdRef/p4_client.html
delete_perforce_client() {
  if p4 clients | awk '{print $2}' | grep -Fxq "${P4CLIENT}"; then
    # delete the client

    p4 client -f -Fs -d "${P4CLIENT}"
  fi
}

# ensure that user is logged into the Perforce server
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

{
  printf "(re)creating temporary client '%s'..." "$P4CLIENT"

  # delete older copy of client (if it exists)
  delete_perforce_client
  envsubst <"${TEMPLATES}/client.tmpl"
  # create new client
  P4_CLIENT_HOST="$(hostname)" envsubst <"${TEMPLATES}/client.tmpl" | p4 client -i

  printf "done\n"
}

# build p4-fusion
./generate_cache.sh Debug
./build.sh

# run p4-fusion against the downloaded depot
./build/p4-fusion/p4-fusion \
  --path "//${DEPOT_NAME}/..." \
  --client "${P4CLIENT}" \
  --user "$P4USER" \
  --src "${GIT_DEPOT_DIR}" \
  --networkThreads 64 \
  --port "${P4PORT}" \
  --lookAhead 15000 \
  --printBatch 1000 \
  --noBaseCommit true \
  --retries 10 \
  --refresh 1000 \
  --maxChanges -1 \
  --includeBinaries true \
  --fsyncEnable true \
  --noColor true 2>&1 | tee "${P4_FUSION_LOG}"

# run validation on migrated data
./validate-migration.sh \
  --force \
  --debug \
  --logfile="${P4_FUSION_LOG}" \
  --p4workdir="${DEPOT_DIR}" \
  --gitdir="${GIT_DEPOT_DIR}" \
  --datadir="${TMP}/datadir"
