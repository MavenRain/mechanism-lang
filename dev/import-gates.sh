#!/bin/zsh
# Stage B checks share a frozen corpus, independent of the input parser.
set -eu

ROOT=${0:A:h}/..
ROOT=${ROOT:A}
CORPUS=${MECHANISM_UAT_CORPUS:-/Users/oobi/Documents/kanon-m2-corpus}
EXPORT=$CORPUS/corpus/lean-parity/uat/uat.export
SHA=f4439dce6a0b488e9bc328592e53c47867c4d19fb123b31358aeb35ed5d14354

if [[ ! -f $EXPORT ]]; then
  print -r -- "IMPORT-CORPUS missing $EXPORT"
  exit 1
fi
actual=$(shasum -a 256 "$EXPORT")
actual=${actual%% *}
if [[ $actual != $SHA ]]; then
  print -r -- "IMPORT-CORPUS wrong digest $actual"
  exit 1
fi

case ${1:-} in
  grammar)
    "$ROOT/_build/default/test/import.exe" --corpus "$EXPORT"
    types=$("$ROOT/_build/default/test/import_types.exe")
    print -r -- "$types"
    print -r -- "$types" | rg -q '^IMPORT-TYPES-OK$'
    ;;
  corpus)
    bash "$CORPUS/dev/lean-parity/check-corpus.sh"
    ;;
  counts)
    out=$("$ROOT/_build/default/bin/mech.exe" diff-parity --export "$EXPORT")
    print -r -- "$out"
    print -r -- "$out" | rg -q '^IMPORT-COUNTS declarations=3202 external_referenced=2477 external_declared=2543 const_names=3017$'
    for row in 'axiom 3' 'def 1176' 'thm 1649' 'opaque 1' 'quot 4' 'inductive 112' 'constructor 143' 'recursor 114'; do
      kind=${row%% *}
      count=${row##* }
      print -r -- "$out" | rg -q "^PARITY-KIND $kind declared=$count scoped_types=$count .*unsupported=0 kernel_errors=0 NEVER=0$"
    done
    print -r -- "PARITY-COUNTS OK"
    ;;
  *)
    print -r -- 'usage: zsh dev/import-gates.sh grammar|corpus|counts'
    exit 64
    ;;
esac
