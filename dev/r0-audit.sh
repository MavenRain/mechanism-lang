#!/bin/zsh
# Audit the effective source compiled into mechanism's kernel and WASM
# libraries.  Run dev/dunecho.sh build first.  The source files under
# _build/default combine the physical overlays and explicit Dune copies;
# scanning vendor alone would miss an overlay that leaks a shape name.
# Generated wrapper files and binaries are not source audit inputs.
#
# Shape names belong only to lib/shape.ml, lib/rules.ml, lib/pp.ml,
# lib/erase.ml and wasm/emit.ml, retaining the pinned audit's scope.
# A stale build copy is an error, so rebuilding is part of mutation
# verification too.

set -u
chpwd_functions=()
unfunction chpwd 2>/dev/null
setopt null_glob

root=${1:-${0:A:h}/..}
root=${root:A}
build=$root/_build/default
pattern='SColl|SMu|SNu|SPar|SPi'

if [[ ! -x $build/bin/mech.exe || ! -f $build/lib/shape.ml \
      || ! -f $build/wasm/gc_encode.ml ]]; then
  print -r -- "R0-AUDIT FAIL: build mechanism with dev/dunecho.sh build first"
  exit 1
fi

files=($build/lib/*.ml $build/lib/*.mli $build/wasm/*.ml $build/wasm/*.mli)
inputs=()
fail=0
for file in $files; do
  rel=${file#$build/}
  source=$root/$rel
  if [[ ! -f $source ]]; then
    source=$root/vendor/kanon/$rel
  fi
  if [[ ! -f $source ]] || ! cmp -s $source $file; then
    print -r -- "R0-AUDIT STALE $rel: rebuild the effective sources"
    fail=1
  fi
  case $rel in
    lib/shape.ml|lib/rules.ml|lib/pp.ml|lib/erase.ml|wasm/emit.ml) ;;
    *) inputs+=($file) ;;
  esac
done

if [[ ${#inputs} -eq 0 ]]; then
  print -r -- "R0-AUDIT FAIL: no effective source files to audit"
  exit 1
fi

hits=$(rg -n -e $pattern -- $inputs)
code=$?
if [[ $code -eq 0 ]]; then
  print -r -- "$hits"
  fail=1
elif [[ $code -ne 1 ]]; then
  print -r -- "R0-AUDIT cannot read the effective source files"
  fail=1
fi

if [[ $fail -eq 0 ]]; then
  print -r -- "R0-AUDIT OK"
  exit 0
fi
print -r -- "R0-AUDIT FAIL"
exit 1
