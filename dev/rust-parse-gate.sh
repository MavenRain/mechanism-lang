#!/bin/zsh
# RUST-PARSE gate for the Rust frontend (lex, fragment pass, parse, print).
# Accepted fixture: rustfmt-clean, and `check` prints the file byte for byte.
# Refused fixture: rustfmt-clean, and `check` prints the one refusal that
# test/rust/refuse/EXPECTED.tsv gives (name, line:col, construct; tabs).
set -u

ROOT=${0:A:h}/..
ROOT=${ROOT:A}
BEND=${BEND:-$HOME/.bend/bin/bend}
NODE=${NODE:-node}
DRIVER=$ROOT/bend2/tests/rust_frontend.bend
PARSE=$ROOT/test/rust/parse
REFUSE=$ROOT/test/rust/refuse
EXPECTED=$REFUSE/EXPECTED.tsv
MIN_ACCEPTED=14
MIN_REFUSED=20

WORK=$(mktemp -d ${TMPDIR:-/tmp}/rust-parse-gate.XXXXXX)
trap 'rm -rf $WORK' EXIT
BIN=$WORK/rust_frontend.js

pass=0
fail=0

ok() {
  print -r -- "PASS $1"
  pass=$((pass + 1))
}

bad() {
  print -r -- "FAIL $1: $2"
  fail=$((fail + 1))
}

finish() {
  print -r -- "pass=$pass fail=$fail"
  if (( fail == 0 )); then
    print -r -- "RUST-PARSE-OK"
    exit 0
  else
    print -r -- "RUST-PARSE-FAIL"
    exit 1
  fi
}

# Run `check` on one file. The `x` sentinel keeps the trailing newlines that
# a command substitution drops, so the driver gets the exact file text.
run_check() {
  local src
  src="$(cat $1; print -rn -- x)"
  $NODE $BIN check "${src%x}" > $WORK/out 2> $WORK/err < /dev/null
}

check_accepted() {
  local f=$1 name=parse/${1:t}
  rustfmt --check --edition 2021 $f > /dev/null 2>&1 || { bad $name "not rustfmt-clean"; return }
  run_check $f || { bad $name "driver exit status"; return }
  cmp -s $f $WORK/out || { bad $name "print(parse(f)) differs from f"; diff $f $WORK/out | head -10; return }
  ok $name
}

check_refused() {
  local name=refuse/$1 f=$REFUSE/$1
  [[ -f $f ]] || { bad $name "no fixture file for this EXPECTED.tsv row"; return }
  rustfmt --check --edition 2021 $f > /dev/null 2>&1 || { bad $name "not rustfmt-clean"; return }
  run_check $f || { bad $name "driver exit status"; return }
  print -r -- "REFUSED $2 $3" > $WORK/want
  cmp -s $WORK/want $WORK/out || { bad $name "want '$(<$WORK/want)', got '$(head -3 $WORK/out)'"; return }
  ok $name
}

[[ -f $EXPECTED ]] || { bad expected "missing $EXPECTED"; finish }

# One JavaScript build of the driver (about one minute); each run with node
# needs about one second. The interpreter needs about one minute for each run,
# and a native build needs 8 to 20 minutes.
whence -p $NODE > /dev/null || { bad build "no $NODE on the PATH"; finish }
if ! $BEND $DRIVER -o $BIN > $WORK/build.log 2>&1; then
  bad build "the driver did not build"
  head -20 $WORK/build.log
  finish
fi

accepted=($PARSE/*.rs(N))
refused=($REFUSE/*.rs(N))

for f in $accepted; do
  check_accepted $f
done

rows=0
while IFS=$'\t' read -r name loc what; do
  rows=$((rows + 1))
  check_refused $name $loc $what
done < $EXPECTED
names=$(cut -f1 $EXPECTED | sort -u | wc -l)

(( ${#accepted} >= MIN_ACCEPTED )) || bad accepted-count "${#accepted} fixtures, floor $MIN_ACCEPTED"
(( ${#refused} >= MIN_REFUSED )) || bad refused-count "${#refused} fixtures, floor $MIN_REFUSED"
(( rows == ${#refused} )) || bad expected-rows "$rows rows for ${#refused} refused fixtures"
(( names == rows )) || bad expected-names "$rows rows, $((names)) different names"
finish
