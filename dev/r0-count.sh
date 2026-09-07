#!/bin/zsh
# Compare mechanism's built spec-count output with its own SPEC.md.
# Run dev/dunecho.sh build first.  The vendored driver's output is never
# used as evidence that the D3 overlay preserves the R0 counts.

set -u
chpwd_functions=()
unfunction chpwd 2>/dev/null

ROOT=${0:A:h}/..
ROOT=${ROOT:A}
SPEC=$ROOT/SPEC.md
DRIVER=$ROOT/_build/default/bin/mech.exe
WORK=$ROOT/.gatework/r0
rm -rf $WORK
mkdir -p $WORK

fail () {
  print -r -- "R0-COUNT FAIL: $1"
  rm -rf $WORK
  exit 1
}

head_ln=$(rg -n '^## R0 counts$' -- $SPEC | head -1 | awk -F: '{print $1}')
if [[ -z $head_ln ]]; then
  fail "SPEC.md has no '## R0 counts' heading"
fi

open_ln=$(rg -n '^```' -- $SPEC | awk -F: -v h=$head_ln '$1 > h' | head -1 | awk -F: '{print $1}')
close_ln=$(rg -n '^```' -- $SPEC | awk -F: -v h=$head_ln '$1 > h' | head -2 | tail -1 | awk -F: '{print $1}')
if [[ -z $open_ln || -z $close_ln || $close_ln -le $open_ln ]]; then
  fail "SPEC.md has no fenced block under the heading"
fi

awk -v a=$((open_ln + 1)) -v b=$((close_ln - 1)) \
  'NR >= a && NR <= b' $SPEC > $WORK/spec.txt || fail "cannot read SPEC.md"
if [[ ! -x $DRIVER ]]; then
  fail "$DRIVER is not built"
fi
if ! $DRIVER spec-count > $WORK/driver.txt; then
  fail "mechanism's spec-count command failed"
fi

if diff $WORK/spec.txt $WORK/driver.txt > $WORK/d 2>&1; then
  print -r -- "R0-COUNT OK"
  rm -rf $WORK
  exit 0
fi
cat $WORK/d
fail "SPEC.md and mechanism's spec-count differ"
