#!/bin/zsh
# Count the active C trust base, including its kernel header.
# Preserve the existing 9000/900 ceilings. A language migration does not approve a larger bound.
set -u
chpwd_functions=()
unfunction chpwd 2>/dev/null
setopt null_glob
root=${1:-${0:A:h}/..}
kernel_bound=9000
encoder_bound=900
kernel_files=($root/c/kernel/**/*.(c|h))
encoder_file=$root/c/wasm/gc_encode.c
if [[ ${#kernel_files} -eq 0 || ! -f $root/c/kernel/check.c || ! -f $root/c/kernel/kernel.h || ! -f $encoder_file ]]; then
  print -r -- "trusted-lines: active C trusted source missing under $root"
  print -r -- "TRUSTED-LINES FAIL"
  exit 1
fi
kernel_out=$(wc -l $kernel_files) || exit 1
encoder_out=$(wc -l < $encoder_file) || exit 1
kernel=$(print -r -- "$kernel_out" | awk 'END { print $1 }')
encoder=$(print -r -- "$encoder_out" | awk '{ print $1 }')
print -r -- "TRUSTED-LINES language=c kernel_files=${#kernel_files}"
line="TRUSTED-LINES kernel=$kernel/$kernel_bound encoder=$encoder/$encoder_bound"
if [[ $kernel -le $kernel_bound && $encoder -le $encoder_bound ]]; then
  print -r -- "$line OK"
  exit 0
fi
print -r -- "$line FAIL"
exit 1
