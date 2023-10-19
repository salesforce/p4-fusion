#!/usr/bin/env bash

set -eo pipefail

migration_log_file=""
p4_client_dir=""
bare_git_dir=""
default_workdir="/tmp/$( basename "$0" )-data"
workdir="${default_workdir}"

# scan sub-directories; exclude the '.git' directory.
diff_args=("--recursive" "--exclude=.git")
show_help=0
forced=0
debug=0

for arg in "$@" ; do
    case "${arg}" in
        --force)
            forced=1
            ;;
        --logfile=*)
            migration_log_file="${arg:10}"
            ;;
        --p4workdir=*)
            p4_client_dir="${arg:12}"
            ;;
        --gitdir=*)
            bare_git_dir="${arg:9}"
            ;;
        --datadir=*)
            workdir="${arg:10}"
            ;;
        --ignore-eol)
            diff_args+=("--ignore-trailing-space")
            ;;
        --debug)
            debug=1
            ;;
        --help)
            show_help=1
            ;;
        -h)
            show_help=1
            ;;
        *)
            echo "Unknown argument: ${arg}"
            show_help=1
            ;;
    esac
done
if [ ! -f "${migration_log_file}" ] || [ -z "${p4_client_dir}" ] || [ ! -d "${bare_git_dir}" ] || [ ${forced} = 0 ] ; then
    show_help=1
fi

if [ ${debug} = 1 ] ; then
  echo "logfile=${migration_log_file}"
  echo "p4_client_dir=${p4_client_dir}"
  echo "bare_git_dir=${bare_git_dir}"
  echo "workdir=${workdir}"
  echo "diff_args=${diff_args[*]}"
fi

if [ ${show_help} = 1 ] ; then
    echo "Usage: $( dirname "$0" ) (arguments)"
    echo "where:"
    echo "   --force"
    echo "      Force operation.  The tool will not run without this."
    echo "      Required."
    echo "   --logfile=(migration log file)"
    echo "      The location of the captured output from running the p4-fusion command."
    echo "      This must include a complete listing of all commits."
    echo "      Make sure this isn't located in the data directory."
    echo "      Required."
    echo "   --p4workdir=(p4 workspace directory)"
    echo "      Location of the local directory mapped to the Perforce workspace"
    echo "      for the migrated depot '--path' argument."
    echo "      Required."
    echo "   --gitdir=(generated git directory)"
    echo "      The generated, bare Git directory from the '--src' argument."
    echo "      This directory will not be changed."
    echo "      Required."
    echo "   --datadir=(working directory)"
    echo "      Location where data files and a cloned Git repository are created."
    echo "      Defaults to ${default_workdir}"
    echo "      The contents of this directory will be wiped!  Be careful reusing a directory."
    echo "   --ignore-eol"
    echo "      When performing the difference, ignore the whitespace at the end of lines."
    echo "   --debug"
    echo "      Report progress information."
    echo "   --help"
    echo "      This text."
    echo ""
    echo "Additionally, you must have your Perforce environment configured such that"
    echo "running 'p4' will be able to sync files to the workspace directory."
    echo ""
    echo "WARNING:"
    echo "Note that this tool will modify the contents of the workspace directory,"
    echo "including removing all files from it.  It will also run 'p4 sync' in the"
    echo "directory, causing your client's file references to change."
    echo ""
    echo "It also cleans out the contents of the data directory."
    exit 0
fi


# Ensure Perforce directory is clean, and the Perforce  before we start.
if [ ${debug} = 1 ]; then
  echo "Cleaning out Perforce client and directory under ${p4_client_dir}"
fi
test -d "${p4_client_dir}" || mkdir -p "${p4_client_dir}"
( cd "${p4_client_dir}" && p4 sync -f -q "...#0" ) || exit 1
rm -rf "${p4_client_dir}" || exit 1
mkdir -p "${p4_client_dir}"

# Set up our work directory.
test -d "${workdir}" && rm -rf "${workdir}"
mkdir -p "${workdir}"
mkdir "${workdir}/diffs"

# Save off a copy of the migration log.
cp "${migration_log_file}" "${workdir}/migration-log.txt"

# Extract out the commit/changelist history.
# Each extracted line is in the format "(commit sha):(p4 changelist):(branch name)"
grep -E '] COMMIT:' "${workdir}/migration-log.txt" \
  | cut -f 2- -d ']' \
  | cut -f 2- -d ':' \
  > "${workdir}/history.txt"

# Make a full file copy of the bare Git repository.
if [ ${debug} = 1 ]; then
  echo "Copying Git repository into ${workdir}/repo"
fi
( cd "${workdir}" && git clone "${bare_git_dir}" repo )

# Process every commit.
while read line ; do
    # The Git commit SHA.
    gitcommit="$( echo "${line}" | cut -f 1 -d ':' )"

    # Shorten up the SHA; some Perforce changelists may map to multiple
    #   commits, so we need this as a distiguisher.
    gitcommit_short="${gitcommit:0:5}"

    # Perforce changelist
    p4cl="$( echo "${line}" | cut -f 2 -d ':' )"

    # Branch the commit happened in, and the source Perforce branch.
    # Empty if there was no branch specified when running p4-fusion.
    branch="$( echo "${line}" | cut -f 3 -d ':' )"

    # The relative P4 depot path for pulling files in the changelist.
    p4DepotPath="${branch}/...@${p4cl}"

    # The output directory where the perforce files are placed.
    # Allows for clean diff between the git repo and the Perforce files.
    p4dir="${p4_client_dir}/${branch}"

    # The name of the Git branch to create.  Again, because the changelist
    # may have multiple commits, we need to make them distinctly named.
    gitbranch="${branch}-${p4cl}-${gitcommit_short}"

    # The output differences file.  The changelist is first in the bits of the
    # name, to allow later comparisons to be easier.
    diff_file="${workdir}/diffs/diff-${p4cl}-${branch}-${gitcommit_short}.txt"

    if [ -z "${branch}" ] ; then
      # No branching done for the p4-fusion execution, so strip branch
      # specific parts off of the names.
      p4DepotPath="...@${p4cl}"
      p4dir="${p4_client_dir}"
      gitbranch="br-${p4cl}-${gitcommit_short}"
      diff_file="${workdir}/diffs/diff-${p4cl}.txt"
    fi

    echo "${p4cl} - ${gitcommit}" >> "${workdir}/progress.txt"

    # Fetch all the files.
    # The Git checkout and Perforce sync are performed in parallel.

    # Make a clean checkout in Git of the commit.
    # This ensures the files are exactly what's in the commit.
    if [ ${debug} = 1 ]; then
      echo "Switching to Git Commit ${gitcommit}"
    fi
    ( cd "${workdir}/repo" && git checkout -b "${gitbranch}" "${gitcommit}" >/dev/null 2>&1 && git reset --hard >/dev/null 2>&1 && git clean -f -d >/dev/null 2>&1 ) &
    j1=$!

    # Have the Perforce depot path match that specific changelist state.
    # Because the directory was cleaned out before the start, it should be in a pristine state
    # after running.
    # Note: not sync -f, because that's not necessary.
    if [ ${debug} = 1 ]; then
      echo "Fetching ${p4DepotPath}"
    fi
    ( cd "${p4_client_dir}" && p4 sync -q "${p4DepotPath}" ) &
    j2=$!

    # Wait for the checkout and sync to complete.
    wait $j1 $j2

    # Discover differences.
    if [ ${debug} = 1 ]; then
      echo "Writing diff into ${diff_file}"
    fi
    set +e
    diff "${diff_args[@]}" "${workdir}/repo" "${p4dir}" > "${diff_file}"
    set -e
    if [ -s "${diff_file}" ] ; then
      echo "${p4cl}:${gitcommit}:${diff_file}" >> "${workdir}/commit-differences.txt"
      echo "${p4cl}" >> "${workdir}/all-changelist-differences.txt"
    fi
done < "${workdir}/history.txt"

# For the error detection, only loop through unique changelists.
if [ -f "${workdir}/all-changelist-differences.txt" ]; then
  sort < "${workdir}/all-changelist-differences.txt" | uniq > "${workdir}/changelist-differences.txt"
fi

error_count=0

if [ ${debug} = 1 ]; then
  echo "Discovering problems."
fi

if [ -f "${workdir}/changelist-differences.txt" ]; then
  while read p4cl ; do
    # This changelist had at least 1 corresponding commit with a problem.
    # If there is some commit with the same changelist with no problem,
    # then that means it eventually matched.
    # Note that, with the splat pattern, non-branch runs will always have
    # this changelist be marked as a problem.
    is_error=1
    file_list=()
    for diff in ${workdir}/diffs/diff-${p4cl}.txt ; do
      file_list+=("$( basename "${diff}" )")
      if [ ! -s "${diff}" ] ; then
        is_error=0
      fi
    done
    if [ ${is_error} = 1 ] ; then
      error_count=$(( error_count + 1 ))
      echo "${p4cl} ${file_list[*]}" >> "${workdir}/errors.txt"
      echo "ERROR: changelist ${p4cl}"
    fi
  done < "${workdir}/changelist-differences.txt"
fi

echo "${error_count} problems discovered.  Complete list in ${workdir}/errors.txt and ${workdir}/commit-differences.txt"
exit ${error_count}
