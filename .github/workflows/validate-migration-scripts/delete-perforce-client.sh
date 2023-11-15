#!/usr/bin/env bash

SCRIPT_ROOT="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
cd "${SCRIPT_ROOT}/../../.."

set -euxo pipefail

export P4USER="${P4USER:-"admin"}"                       # the name of the Perforce superuser that the script will use to create the depot
export P4PORT="${P4PORT:-"ssl:perforce.sgdev.org:1666"}" # the address of the Perforce server to connect to   # the name of the depot that the script will create on the server
export P4CLIENT="${P4CLIENT:-"integration-test-client"}" # the name of the temporary client that the script will use while it creates the depot

# delete_perforce_client deletes the client specified by "$P4CLIENT"
# if it exists on the Perforce server.
#
## P4 CLI reference(s):
##
## https://www.perforce.com/manuals/cmdref/Content/CmdRef/p4_client.html

if p4 clients | awk '{print $2}' | grep -Fxq "${P4CLIENT}"; then
  # delete the client
  p4 client -f -Fs -d "${P4CLIENT}"
fi
