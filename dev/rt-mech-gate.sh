#!/bin/zsh
# ROUND-TRIP gate (mech-rust unit D, D1). E = `rust-out`, I = `rust-in`.
# For each seed program S of test/rust/seed (MANIFEST and .mech files):
#   SEED-CHECK  S passes the kernel check with no axiom.
#   RT-RUST     E(I(E(S))) is equal to E(S), file for file.
#   FIXPOINT    I(E(I(E(S)))) is equal to I(E(S)), file for file.
#   RT-MECH     S and I(E(S)) have the same checked rows up to bound names
#               (mode `rt-mech` of bend2/tests/rust_import.bend).
# Controls, on the seed 01_enum_match:
#   GREEN  a copy of the seed with each binder renamed passes RT-MECH.
#   RED    two branch bodies swapped in the imported text: RT-MECH fails.
#   RED    two constructors swapped in the imported text: RT-RUST fails.
# Controls of the carrier merge (D2), on the seed 05_proofs:
#   KEYS   the carrier of E(S) has one key line for each carried chunk.
#   COPY   the import has a byte-equal copy of the carrier.
#   RED    the last carried proof dropped from the carrier: RT-MECH fails.
#   RED    each Boolean of a fn body changed in the Rust text: a carried
#          proof is false, and the import fails with MECH-CHECK-FAIL.
#   REFUSE a key with no place in the import: exit code 65, no output.
#   REFUSE a carried chunk with no key line: exit code 65, no output.
# One PASS or FAIL line for each check, then ROUND-TRIP-OK (exit 0) or
# ROUND-TRIP-FAIL (exit 1). All outputs stay in $RT_WORK.
set -u
ROOT=${0:A:h:h}
BEND=${BEND:-$HOME/.bend/bin/bend}
SEED=$ROOT/test/rust/seed
CTRL=$ROOT/test/rust/seed-control
WORK=${RT_WORK:-$TMPDIR/rt-mech-gate}
mkdir -p $WORK
JS=$WORK/rust_import.js
RE=$WORK/rust_emit.js
ulimit -s "$(ulimit -Hs)" 2> /dev/null
$BEND $ROOT/bend2/tests/rust_import.bend -o $JS > $WORK/build-i.log 2>&1 || { print "FAIL build rust_import"; tail -n 5 $WORK/build-i.log; print ROUND-TRIP-FAIL; exit 1 }
$BEND $ROOT/bend2/tests/rust_emit.bend -o $RE > $WORK/build-e.log 2>&1 || { print "FAIL build rust_emit"; tail -n 5 $WORK/build-e.log; print ROUND-TRIP-FAIL; exit 1 }

pass=0
fail=0
step() {
  if [[ $1 == 0 ]]; then
    print -r -- "PASS $2"
    pass=$((pass + 1))
  else
    print -r -- "FAIL $2 ${3:-}"
    fail=$((fail + 1))
  fi
}
emit() {
  local dir=$1 out=$2
  shift 2
  (cd -q $dir && node --stack-size=16384 $RE crate "$@" $out > $WORK/emit.out 2> $WORK/emit.err < /dev/null)
}
imp() {
  node --stack-size=16384 $JS run $1 $2 > $WORK/imp.out 2> $WORK/imp.err < /dev/null
}
# The text of a program: its files in manifest order, a newline after each.
joined() {
  local f
  for f in ${(f)"$(< $1/MANIFEST)"}; do
    cat $1/$f
    print
  done
}
mode() {
  node --stack-size=16384 $JS "$@" 2> $WORK/mode.err < /dev/null
}

for p in $SEED/*(/); do
  n=${p:t}
  files=(${(f)"$(< $p/MANIFEST)"})
  rm -rf $WORK/$n
  mkdir -p $WORK/$n
  src=$(joined $p)
  out=$(mode seed-check $src)
  rc=$?
  [[ $rc == 0 && $out == "MECH-CHECK-OK "<1->" rows axioms=0" ]]
  step $? "SEED-CHECK $n" $out
  emit $p $WORK/$n/e1 $files
  step $? "EMIT $n" "$(head -c 300 $WORK/emit.err $WORK/emit.out)"
  imp $WORK/$n/e1 $WORK/$n/i1
  step $? "IMPORT $n" "$(head -c 300 $WORK/imp.err)"
  if [[ ! -s $WORK/$n/i1/MANIFEST ]]; then
    step 1 "MANIFEST $n" "import produced no nonempty manifest"
    continue
  fi
  f2=(${(f)"$(< $WORK/$n/i1/MANIFEST)"})
  emit $WORK/$n/i1 $WORK/$n/e2 $f2
  step $? "EMIT2 $n" "$(head -c 300 $WORK/emit.err)"
  diff -r $WORK/$n/e1 $WORK/$n/e2 > $WORK/$n/rt-rust.diff 2>&1
  step $? "RT-RUST $n" "$(head -n 6 $WORK/$n/rt-rust.diff)"
  imp $WORK/$n/e2 $WORK/$n/i2
  step $? "IMPORT2 $n" "$(head -c 300 $WORK/imp.err)"
  diff -r $WORK/$n/i1 $WORK/$n/i2 > $WORK/$n/fix.diff 2>&1
  step $? "FIXPOINT $n" "$(head -n 6 $WORK/$n/fix.diff)"
  out=$(mode rt-mech $src "$(joined $WORK/$n/i1)")
  rc=$?
  [[ $rc == 0 && $out == "RT-MECH-OK "<1-> ]]
  step $? "RT-MECH $n" $out
done

# Controls.
c=01_enum_match
src=$(joined $SEED/$c)
out=$(mode rt-mech $src "$(< $CTRL/${c}_renamed.mech)")
rc=$?
[[ $rc == 0 && $out == "RT-MECH-OK 3" && "$(< $CTRL/${c}_renamed.mech)" != "$(< $SEED/$c/light.mech)" ]]
step $? "GREEN renamed binders" $out

imported=$(joined $WORK/$c/i1)
from='| lightAmber => mechFalse | lightGreen => mechTrue'
to='| lightAmber => mechTrue | lightGreen => mechFalse'
mutant=${imported/$from/$to}
out=$(mode rt-mech $src $mutant)
rc=$?
[[ $rc == 0 && $mutant != $imported && $out == "RT-MECH-FAIL lightGo" ]]
step $? "RED branch bodies swapped (RT-MECH)" $out

rm -rf $WORK/red-ctor $WORK/red-ctor-e
mkdir -p $WORK/red-ctor
cp $WORK/$c/i1/* $WORK/red-ctor/
text=$(< $WORK/$c/i1/light.mech)
from=$'| lightRed : Light\n| lightAmber : Light'
to=$'| lightAmber : Light\n| lightRed : Light'
print -r -- ${text/$from/$to} > $WORK/red-ctor/light.mech
emit $WORK/red-ctor $WORK/red-ctor-e light.mech
rc=$?
diff -r $WORK/$c/e1 $WORK/red-ctor-e > $WORK/red-ctor.diff 2>&1
[[ $rc == 0 && $? == 1 && "$(< $WORK/red-ctor/light.mech)" != $text ]]
step $? "RED constructors swapped (RT-RUST)" "emit rc=$rc"

# Controls of the carrier merge (D2).
c=05_proofs
src=$(joined $SEED/$c)
car=$(< $WORK/$c/e1/mech-carrier.mech)
lines=(${(f)car})
keyl=(${(M)lines:#-- at *})
keys=${#keyl}
[[ $keys == 3 && $car == *$'\n-- at light.mech start\n'* && $car == *$'\n-- at light.mech after '* ]]
step $? "KEYS $c" "keys=$keys"
cmp -s $WORK/$c/e1/mech-carrier.mech $WORK/$c/i1/mech-carrier.mech
step $? "COPY $c carrier copy is byte-equal"

# File order must name the exact imported inventory once. The seed
# 07_carrier_order exercises proof-only edges across three files.
order=$'\n-- files light.mech\n'
for kind in unknown duplicate missing; do
  case $kind in
    unknown) replacement=$'\n-- files missing.mech\n' ;;
    duplicate) replacement=$'\n-- files light.mech light.mech\n' ;;
    missing) replacement=$'\n-- files\n' ;;
  esac
  rm -rf $WORK/ref-order-$kind $WORK/ref-order-$kind-i
  cp -R $WORK/$c/e1 $WORK/ref-order-$kind
  bad=${car/$order/$replacement}
  print -r -- $bad > $WORK/ref-order-$kind/mech-carrier.mech
  imp $WORK/ref-order-$kind $WORK/ref-order-$kind-i
  rc=$?
  got="$(head -n 1 $WORK/imp.err)"
  [[ $rc == 65 && $bad != $car && $got == "REFUSED carrier: "*"file"* && ! -e $WORK/ref-order-$kind-i && ! -L $WORK/ref-order-$kind-i ]]
  step $? "REFUSE $kind file in carrier source order" "import rc=$rc $got"
done

rm -rf $WORK/red-drop $WORK/red-drop-i
cp -R $WORK/$c/e1 $WORK/red-drop
print -r -- ${car%$'\n-- at '*} > $WORK/red-drop/mech-carrier.mech
imp $WORK/red-drop $WORK/red-drop-i
rc=$?
out=$(mode rt-mech $src "$(joined $WORK/red-drop-i)")
rc2=$?
[[ $rc == 0 && $rc2 == 0 && "$(< $WORK/red-drop/mech-carrier.mech)" != $car && $out == "RT-MECH-FAIL "* ]]
step $? "RED carried proof dropped (RT-MECH)" "import rc=$rc $out"

rm -rf $WORK/red-body $WORK/red-body-i
cp -R $WORK/$c/e1 $WORK/red-body
rs=$(< $WORK/$c/e1/src/light.rs)
flip=${${${rs//true/@@}//false/true}//@@/false}
print -r -- $flip > $WORK/red-body/src/light.rs
imp $WORK/red-body $WORK/red-body-i
rc=$?
[[ $rc == 65 && $flip != $rs && "$(cat $WORK/imp.err $WORK/imp.out | head -c 15)" == "MECH-CHECK-FAIL" && ! -e $WORK/red-body-i ]]
step $? "RED fn body changed (MECH-CHECK-FAIL)" "import rc=$rc $(head -c 200 $WORK/imp.err)"

# Refusals of the merge. Each one has exit code 65 and writes no output.
key=$'\n-- at light.mech start\n'
rm -rf $WORK/ref-place $WORK/ref-place-i
cp -R $WORK/$c/e1 $WORK/ref-place
bad=${car/$key/$'\n-- at light.mech after noSuchName\n'}
print -r -- $bad > $WORK/ref-place/mech-carrier.mech
imp $WORK/ref-place $WORK/ref-place-i
rc=$?
got="$(head -n 1 $WORK/imp.err)"
[[ $rc == 65 && $bad != $car && $got == "REFUSED carrier: the import has no place for the key line: "*noSuchName* && ! -e $WORK/ref-place-i ]]
step $? "REFUSE key with no place in the import" "import rc=$rc $got"

rm -rf $WORK/ref-key $WORK/ref-key-i
cp -R $WORK/$c/e1 $WORK/ref-key
bad=${car/$key/$'\n'}
print -r -- $bad > $WORK/ref-key/mech-carrier.mech
imp $WORK/ref-key $WORK/ref-key-i
rc=$?
got="$(head -n 1 $WORK/imp.err)"
[[ $rc == 65 && $bad != $car && $got == "REFUSED carrier: the first line of a chunk is not a key line: "* && ! -e $WORK/ref-key-i ]]
step $? "REFUSE carried chunk with no key line" "import rc=$rc $got"

print -r -- "work=$WORK pass=$pass fail=$fail"
if (( fail == 0 )); then
  print ROUND-TRIP-OK
  exit 0
fi
print ROUND-TRIP-FAIL
exit 1
