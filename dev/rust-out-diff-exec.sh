#!/bin/zsh
# DIFF-EXEC gate of `mech rust-out`.
#
# The gate builds the driver bend2/tests/rust_emit.bend to JavaScript, writes
# the crate of prelude/init.mech + prelude/mechanism/second-price.mech, and
# compares it with the goldens in test/rust/emit. Then it adds the probes (the
# generated probes and test/rust/emit/probes.mech) as a third module, and
# compares the value of each probe from the kernel evaluator with the value
# that the generated src/bin/diff_exec.rs prints. Each crate is built below
# $TMPDIR, outside the tracked tree.
#
# Output: one PASS or FAIL line for each check and each value, then
# DIFF-EXEC-OK or DIFF-EXEC-FAIL. The mutation control swaps two match arms
# in a copy of the crate. The gate is green only if the copy is RED.
#
# Use: dev/rust-out-diff-exec.sh [emit]. `emit` stops before the cargo steps.
# BEND gives the Bend compiler (default: $HOME/.bend/bin/bend). NODE gives
# node. The driver runs with the stack of dev/BEND2-BASELINE.json.
set -u
ulimit -s "$(ulimit -Hs)"
root=${0:A:h:h}
bend=${BEND:-$HOME/.bend/bin/bend}
node=(${NODE:-node} --stack-size=16384)
stage=${1:-all}
G=$root/test/rust/emit
O=${TMPDIR:-/tmp}/mech-diff-exec
mkdir -p $O
R=$(mktemp -d $O/run.XXXXXX)
rc=0
pass() { print -r -- "PASS $1" }
fail() { print -r -- "FAIL $1"; rc=1 }
stop() { print -r -- "FAIL $1"; [[ -n ${2:-} ]] && head -n 12 $2 | cut -c1-200; print -r -- "DIFF-EXEC-FAIL"; exit 1 }
golden() { if diff $G/$1 $2 > /dev/null 2>&1; then pass "golden $1"; else fail "golden $1"; fi }

$bend $root/bend2/tests/rust_emit.bend -o $O/re.js > $O/build.log 2>&1 || stop "driver build" $O/build.log
cp $root/prelude/init.mech $root/prelude/mechanism/second-price.mech $R/
cd -q $R
$node $O/re.js crate init.mech second-price.mech plain > plain.out 2> plain.err || stop "crate write" plain.err
for f in Cargo.toml mech-carrier.mech src/lib.rs src/init.rs src/nat.rs src/second_price.rs; do
  golden crate/$f plain/$f
done
$node $O/re.js probes init.mech second-price.mech > generated.mech 2> generated.err || stop "probe generation" generated.err
golden generated-probes.mech generated.mech
cat generated.mech $G/probes.mech > probes.mech
$node $O/re.js values init.mech second-price.mech probes.mech > values.txt 2> values.err || stop "kernel values" values.err
golden values.txt values.txt
$node $O/re.js crate init.mech second-price.mech probes.mech out > out.out 2> out.err || stop "probe crate write" out.err
mkdir out/src/bin
$node $O/re.js bin init.mech second-price.mech probes.mech > out/src/bin/diff_exec.rs 2> bin.err || stop "diff_exec.rs" bin.err
if [[ $stage == emit ]]; then
  print -r -- "run directory: $R"
  exit $rc
fi

# run_bin <crate> <target dir> <tag>: builds the crate and runs diff_exec.
run_bin() {
  cargo build --quiet --manifest-path $1/Cargo.toml --target-dir $2 -j 2 > $3.build.log 2>&1 || return 1
  $2/debug/diff_exec > $3.txt 2> $3.err
}

# compare <rust values> <show>: sets `diffs` to the number of values that differ.
compare() {
  local -a want got
  local i name
  want=("${(@f)$(< values.txt)}")
  got=("${(@f)$(< $1)}")
  diffs=0
  if (( ${#want} != ${#got} )); then
    diffs=1
    [[ $2 == show ]] && fail "value count: kernel ${#want}, rust ${#got}"
  fi
  for i in {1..${#want}}; do
    name=${want[i]%% *}
    if [[ "${want[i]}" == "$name = ${got[i]:-}" ]]; then
      [[ $2 == show ]] && pass "${want[i]}"
    else
      diffs=$((diffs + 1))
      [[ $2 == show ]] && fail "${want[i]} (rust: ${got[i]:-no value})"
    fi
  done
}

run_bin out $O/target rust || stop "crate build or run" rust.build.log
compare rust.txt show

# Mutation control: each match on AuctionChoice gets its two arms swapped.
cp -R out mutant
python3 - mutant/src/second_price.rs <<'EOF' || stop "mutation site"
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
lose, win = "AuctionChoice::AuctionLose =>", "AuctionChoice::AuctionWin =>"
if lose not in text or win not in text:
    sys.exit(1)
path.write_text(text.replace(lose, "\0").replace(win, lose).replace("\0", win))
EOF
run_bin mutant $O/target-mutant mutant || stop "mutant build or run" mutant.build.log
kept=$rc
compare mutant.txt quiet
rc=$kept
if (( diffs > 0 )); then pass "mutation control is RED: $diffs values differ"; else fail "mutation control is GREEN"; fi

if (( rc == 0 )); then
  print -r -- "DIFF-EXEC-OK ${#${(@f)$(< values.txt)}} values"
else
  print -r -- "DIFF-EXEC-FAIL"
fi
exit $rc
