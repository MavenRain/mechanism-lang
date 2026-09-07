#!/bin/zsh
# dev/pin-delta.sh
# The PIN-DELTA leg of the gate battery (M0-PLAN.md:147, Stage 0 brief
# 3.5).  Example:
#   zsh /Users/oobi/Documents/mechanism-lang/dev/pin-delta.sh
#
# An overlay row is a mechanism-lang source file whose path under the
# repository root also exists in the vendored kanon tree at PIN.  The
# script diffs each one against `git -C vendor/kanon show PIN:PATH`, and
# it prints one row per file with the changed line count.  It fails when
# an overlay file has no row in dev/PIN-DELTA.md, when a listed row has
# no file in the tree, or when a count differs.
#
# S0-D6.  The overlay scope is the OCaml source trees lib/, wasm/,
# surface/ and bin/, because only those can shadow a vendored kanon
# source file.  dev/, test/ and the repository prose are mechanism-lang's
# own files, not overlays.  At Stage 0 the scope holds no file, so the
# script folds over zero rows.  The empty case is not a special case in
# the code.
#
# The root comes from this script's own path, so a copy of the
# repository under a scratch directory checks itself.

set -u

# The user shell startup files add a chpwd hook that reads an unset
# parameter.  Under set -u that hook fails, so the hooks are cleared.
chpwd_functions=()
unfunction chpwd 2>/dev/null

setopt null_glob

ROOT=${0:A:h}/..
ROOT=${ROOT:A}
TABLE=$ROOT/dev/PIN-DELTA.md
VENDOR=$ROOT/vendor/kanon
PIN_SHA=$(cat $ROOT/PIN)
# The work directory sits under the repository root, not under the
# system temp directory, so the script needs no writable path outside the
# tree it checks.  .gitignore holds it.
WORK=$ROOT/.gatework/pin-delta
rm -rf $WORK
mkdir -p $WORK
fail=0
seen=""

print -r -- "PIN-DELTA file diff expected"

overlay=()
for f in $ROOT/lib/**/*.ml $ROOT/lib/**/*.mli \
         $ROOT/wasm/**/*.ml $ROOT/wasm/**/*.mli \
         $ROOT/surface/**/*.ml $ROOT/surface/**/*.mli \
         $ROOT/bin/**/*.ml $ROOT/bin/**/*.mli; do
  rel=${f#$ROOT/}
  if git -C $VENDOR cat-file -e $PIN_SHA:$rel 2> /dev/null; then
    overlay+=$rel
  fi
done

rg -N '^\| (lib|wasm|surface|bin)/' -- $TABLE > $WORK/rows

while IFS= read -r row; do
  file=$(print -r -- "$row" | awk -F'|' '{gsub(/^ +| +$/, "", $2); print $2}')
  want=$(print -r -- "$row" | awk -F'|' '{gsub(/^ +| +$/, "", $3); print $3}')

  if [[ ! -f $ROOT/$file ]]; then
    print -r -- "PIN-DELTA $file MISSING FAIL"
    fail=1
    continue
  fi

  if ! git -C $VENDOR show $PIN_SHA:$file > $WORK/orig 2> /dev/null; then
    print -r -- "PIN-DELTA $file ORIGIN-MISSING FAIL"
    fail=1
    continue
  fi

  got=$(diff $WORK/orig $ROOT/$file | /usr/bin/wc -l | tr -d ' ')

  if [[ $got == $want ]]; then
    print -r -- "PIN-DELTA $file diff=$got expected=$want OK"
  else
    print -r -- "PIN-DELTA $file diff=$got expected=$want FAIL"
    fail=1
  fi
  seen="$seen $file"
done < $WORK/rows

# An overlay file with no row in the table is a FAIL too, so an overlay
# cannot hide by leaving the table alone.
for rel in $overlay; do
  if [[ " $seen " != *" $rel "* ]]; then
    print -r -- "PIN-DELTA $rel NO-ROW FAIL"
    fail=1
  fi
done

rm -rf $WORK

if [[ $fail -eq 0 ]]; then
  print -r -- "PIN-DELTA OK"
  exit 0
fi
print -r -- "PIN-DELTA FAIL"
exit 1
