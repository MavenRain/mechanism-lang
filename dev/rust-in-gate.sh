#!/bin/zsh
# RUST-IN gate for the Rust importer (`mech rust-in`: lift, resolve, infer,
# lower, kernel check).  One PASS or FAIL line for each check, then
# RUST-IN-OK or RUST-IN-FAIL.
#   LIFT        The lift of each module file of the golden crate prints back
#               byte for byte.
#   GOLDEN      The import of the golden crate is equal to
#               test/rust/import/expected.
#   MECH-CHECK  The imported program passes the kernel check with no axiom.
#   RT-RUST     The emitter gives each file of the golden crate back from the
#               imported program.  The carrier is not compared.
#   REFUSE      Each fixture of test/rust/import/refuse gives the exit code
#               and the first error line of EXPECTED.tsv (name, code, line;
#               tabs), and writes no output directory.
#   RED         Two mutation controls.  Each one must fail its check.
# DIFF-EXEC on the imported program is not a part of this gate.  RT-RUST gives
# the golden crate back, and dev/rust-out-diff-exec.sh runs that crate.
set -u

ROOT=${0:A:h}/..
ROOT=${ROOT:A}
BEND=${BEND:-$HOME/.bend/bin/bend}
NODE=${NODE:-node}
CRATE=$ROOT/test/rust/emit/crate
IMPORT=$ROOT/test/rust/import
EXPECTED=$IMPORT/refuse/EXPECTED.tsv
MIN_REFUSED=8

WORK=$(mktemp -d ${TMPDIR:-/tmp}/rust-in-gate.XXXXXX)
trap 'rm -rf $WORK' EXIT
JS=$WORK/rust_import.js
RE=$WORK/rust_emit.js
ulimit -s "$(ulimit -Hs)" 2> /dev/null

pass=0
fail=0

ok() {
  print -r -- "PASS $1"
  pass=$((pass + 1))
}

bad() {
  print -r -- "FAIL $1"
  fail=$((fail + 1))
}

finish() {
  print -r -- "pass=$pass fail=$fail"
  if (( fail == 0 )); then
    print RUST-IN-OK
    exit 0
  fi
  print RUST-IN-FAIL
  exit 1
}

# build <driver name> <out js>
build() {
  if ! $BEND $ROOT/bend2/tests/$1.bend -o $2 > $WORK/build.log 2>&1; then
    tail -n 20 $WORK/build.log
    bad "build $1"
    finish
  fi
}

# text <file>: the bytes of the file go to $REPLY.  The `x` keeps the last
# newlines.
text() {
  REPLY="$(cat $1; print -rn -- x)"
  REPLY=${REPLY%x}
}

# imp <mode> <arguments>: run the importer driver.  The exit code goes to $rc,
# stdout to $WORK/o, stderr to $WORK/e.
rc=0
imp() {
  $NODE --stack-size=16384 $JS "$@" > $WORK/o 2> $WORK/e < /dev/null
  rc=$?
}

# emit <folder of .mech files> <out dir> <files>: run `rust-out`.  The emitter
# reads the files by name, so it runs in their folder.
emit() {
  local dir=$1 out=$2
  shift 2
  (cd -q $dir && $NODE --stack-size=16384 $RE crate "$@" $out > $WORK/emit.out 2> $WORK/emit.err < /dev/null)
}

# edit <file> <old> <new>: replace each <old>; fail if the file has no <old>.
edit() {
  python3 - "$@" <<'EOF'
import sys
path, old, new = sys.argv[1:4]
text = open(path).read()
open(path, "w").write(text.replace(old, new))
sys.exit(0 if old in text else 1)
EOF
}

build rust_import $JS
build rust_emit $RE
ok "build (importer driver, emitter driver)"

# The modules of the golden crate, in the order of src/lib.rs.
mods=()
for l in ${(f)"$(< $CRATE/src/lib.rs)"}; do
  if [[ $l == 'pub mod '*';' ]]; then
    mods+=(${${l#pub mod }%;})
  fi
done

# LIFT.  The `nat` module has a fixed text and no lift.
for m in ${mods:#nat}; do
  text $CRATE/src/$m.rs
  imp lift "$REPLY"
  if (( rc == 0 )) && cmp -s $WORK/o $CRATE/src/$m.rs; then
    ok "LIFT src/$m.rs"
  else
    bad "LIFT src/$m.rs: exit $rc: $(head -c 300 $WORK/e)"
  fi
done

# GOLDEN.
O=$WORK/out
imp run $CRATE $O
if (( rc != 0 )); then
  bad "import of the golden crate: exit $rc: $(head -c 300 $WORK/e)"
  finish
fi
ok "import of the golden crate (exit 0)"
files=(${(f)"$(< $O/MANIFEST)"})
for f in MANIFEST $files; do
  if cmp -s $O/$f $IMPORT/expected/$f; then
    ok "GOLDEN $f"
  else
    bad "GOLDEN $f"
  fi
done
have=($IMPORT/expected/*(N:t))
if (( ${#have} == ${#files} + 1 )); then
  ok "GOLDEN file count (${#have})"
else
  bad "GOLDEN file count: ${#have} files, want $(( ${#files} + 1 ))"
fi

# Check the complete published directory, including the optional carrier.
want=(MANIFEST $files)
if [[ -f $CRATE/mech-carrier.mech ]]; then
  want+=(mech-carrier.mech)
  if cmp -s $CRATE/mech-carrier.mech $O/mech-carrier.mech; then
    ok "GOLDEN carrier copy"
  else
    bad "GOLDEN carrier copy"
  fi
fi
have=($O/*(DN:t))
if [[ ${(j: :)${(o)have}} == ${(j: :)${(o)want}} ]]; then
  ok "GOLDEN output inventory"
else
  bad "GOLDEN output inventory: ${(j: :)have}"
fi
for f in $have; do
  if [[ ! -f $O/$f || -L $O/$f ]]; then
    bad "GOLDEN output is not a regular file: $f"
  fi
done

# MECH-CHECK.
args=()
for m in $mods; do
  text $CRATE/src/$m.rs
  args+=($m "$REPLY")
done
text $CRATE/src/lib.rs
imp check "$REPLY" $args
line="$(head -n 1 $WORK/o)"
if (( rc == 0 )) && [[ $line == MECH-CHECK-OK\ <->\ rows\ axioms=0 ]]; then
  ok "MECH-CHECK ($line)"
else
  bad "MECH-CHECK: exit $rc: $(head -c 300 $WORK/o) $(head -c 300 $WORK/e)"
fi

# RT-RUST.
if emit $O $WORK/rt $files; then
  for f in Cargo.toml src/lib.rs src/${^mods}.rs; do
    if cmp -s $CRATE/$f $WORK/rt/$f; then
      ok "RT-RUST $f"
    else
      bad "RT-RUST $f"
    fi
  done
  have=($WORK/rt/*(DN:t))
  want=($CRATE/*(DN:t))
  if [[ ${(j: :)${(o)have}} == ${(j: :)${(o)want}} ]] \
    && diff -r $CRATE/src $WORK/rt/src > $WORK/rt.diff; then
    ok "RT-RUST complete file inventory"
  else
    bad "RT-RUST complete file inventory"
  fi
else
  bad "RT-RUST: the emitter failed: $(head -c 300 $WORK/emit.err)"
fi

# REFUSE.
rows=0
seen=()
while IFS=$'\t' read -r name code want; do
  if [[ -z $name ]]; then
    continue
  fi
  rows=$((rows + 1))
  seen+=($name)
  if [[ ! -f $IMPORT/refuse/$name/src/lib.rs ]]; then
    bad "REFUSE $name: no fixture"
    continue
  fi
  imp run $IMPORT/refuse/$name $WORK/refuse-$name
  got="$(head -n 1 $WORK/e)"
  if [[ $rc == "$code" && $got == "$want" && ! -e $WORK/refuse-$name && ! -L $WORK/refuse-$name ]]; then
    ok "REFUSE $name ($want)"
  else
    bad "REFUSE $name: exit $rc, want $code: $got"
  fi
done < $EXPECTED
for d in $IMPORT/refuse/*(N/:t); do
  if (( ! ${seen[(Ie)$d]} )); then
    bad "REFUSE $d: no row in EXPECTED.tsv"
  fi
done
if (( rows >= MIN_REFUSED )); then
  ok "REFUSE count ($rows rows)"
else
  bad "REFUSE count: $rows rows, want $MIN_REFUSED or more"
fi

# RED 1: swap the names of the two constructors of AuctionChoice in the
# imported second-price.mech.  RT-RUST must fail on src/second_price.rs.
M=$WORK/swap
mkdir -p $M
cp $O/* $M/
if edit $M/second-price.mech auctionLose auctionSwap \
  && edit $M/second-price.mech auctionWin auctionLose \
  && edit $M/second-price.mech auctionSwap auctionWin; then
  if emit $M $WORK/rt-swap $files && ! cmp -s $CRATE/src/second_price.rs $WORK/rt-swap/src/second_price.rs; then
    ok "RED constructor swap (RT-RUST fails on src/second_price.rs)"
  else
    bad "RED constructor swap: RT-RUST is green or the emitter failed: $(head -c 200 $WORK/emit.err)"
  fi
else
  bad "RED constructor swap: the edit did not apply"
fi

# RED 2: auction_compare calls itself on its full arguments in a copy of the
# golden crate.  The kernel check must fail with the totality error.
B=$WORK/total
cp -R $CRATE $B
if edit $B/src/second_price.rs 'auction_compare(tie_wins, previous, other)' 'auction_compare(tie_wins, bid, price)'; then
  imp run $B $WORK/out-total
  got="$(head -n 1 $WORK/e)"
  if [[ $rc == 65 && $got == MECH-CHECK-FAIL*'structural termination guard'* && ! -e $WORK/out-total && ! -L $WORK/out-total ]]; then
    ok "RED recursion that is not structural ($got)"
  else
    bad "RED recursion that is not structural: exit $rc: $got"
  fi
else
  bad "RED recursion that is not structural: the edit did not apply"
fi

finish
