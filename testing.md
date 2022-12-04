# How To Test

In order to test the validity of the logic, we need to run the program over a Perforce depot and compare each changelist against the corresponding Git commit SHA, to ensure the files match up.

Test setup:
    * Perforce source depot path: `${depotPath}` (e.g. "//mydepot"); passed to p4-fusion with `--path ${depotPath}/...`
    * Git bare repository path: `${gitSrc}` (e.g. "/tmp/git-path"); passed to p4-fusion with `--src ${gitSrc}`
    * Output of p4-fusion piped to the file `${outputFile}` (e.g. "migrage-log.txt").  With this, it's helpful to add the `--noColor true` arguments to the command.
    * The `p4` command has a client configured that maps the `${depotPath}` to the local directory `${p4ClientDir}`.

In addition, we must first prepare the non-bare Git repository into the local directory `${gitRepo}`:

```bash
( rm -rf "${gitRepo}" ; git clone "${gitSrc}" "${gitRepo}" )
```

## Testing With Branches

If the `--branch` argument is given at least once, then we can test with the script:

```bash
mkdir -p test-files
rm test-files/diff*.txt 2>/dev/null || echo "No existing test files."
cat "${outputFile}" | grep -E '] COMMIT:' | cut -f 2- -d ']' | cut -f 2- -d ':' > test-files/history.txt
prefix=a
for line in $( cat test-files/history.txt ) ; do
    gitcommit="$( echo "${line}" | cut -f 1 -d ':' )"
    p4cl="$( echo "${line}" | cut -f 2 -d ':' )"
    branch="$( echo "${line}" | cut -f 3 -d ':' )"
    ( cd "${gitRepo}" && git checkout -b "${prefix}-${branch}-${p4cl}" "${gitcommit}" && git reset --hard && git clean -f -d )
    ( cd "${p4ClientDir}" && p4 sync -f "${branch}/...@${p4cl}" )
    diff -r -Z "${gitRepo}" "${p4ClientDir}/${branch}" | grep -v -E "^Only in ${gitRepo}: .git" > test-files/diff-${branch}-${p4cl}.txt
done
```

Any `test-files/diff-*.txt` files that are not empty contain differences, and thus are a failure.


## Testing Without Branches

If no `--branch` argument is given, then the test script is very similar to the above, just without branches.

```bash
mkdir -p test-files
rm test-files/diff*.txt 2>/dev/null || echo "No existing test files."
cat "${outputFile}" | grep -E '] COMMIT:' | cut -f 2- -d ']' | cut -f 2- -d ':' > test-files/history.txt
prefix=a
for line in $( cat test-files/history.txt ) ; do
    gitcommit="$( echo "${line}" | cut -f 1 -d ':' )"
    p4cl="$( echo "${line}" | cut -f 2 -d ':' )"
    ( cd "${gitRepo}" && git checkout -b "${prefix}-${p4cl}" "${gitcommit}" && git reset --hard && git clean -f -d )
    ( cd "${p4ClientDir}" && p4 sync -f "...@${p4cl}" )
    diff -r -Z "${gitRepo}" "${p4ClientDir}" | grep -v -E "^Only in ${gitRepo}: .git" > test-files/diff-${p4cl}.txt
done
```
