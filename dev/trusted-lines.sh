#!/bin/zsh
# Count the active native Bend trust base, without counting retired sources.
# The Bend kernel ceiling is 9000 lines; the encoder ceiling remains 900.
# Every kernel .bend module counts, including shared data and erasure helpers.
set -u
chpwd_functions=()
unfunction chpwd 2>/dev/null
setopt null_glob
root=${1:-${0:A:h}/..}
kernel_bound=9000
encoder_bound=900
kernel_files=($root/bend2/kernel/**/*.bend)
encoder_file=$root/bend2/wasm/gc_encode.bend
if [[ ${#kernel_files} -eq 0 || ! -f $root/bend2/kernel/check.bend || ! -f $encoder_file ]]; then
  print -r -- "trusted-lines: active Bend trusted source missing under $root"
  print -r -- "TRUSTED-LINES FAIL"
  exit 1
fi
kernel_out=$(wc -l $kernel_files) || exit 1
encoder_out=$(wc -l < $encoder_file) || exit 1
kernel=$(print -r -- "$kernel_out" | awk 'END { print $1 }')
encoder=$(print -r -- "$encoder_out" | awk '{ print $1 }')
print -r -- "TRUSTED-LINES language=bend kernel_files=${#kernel_files}"
line="TRUSTED-LINES kernel=$kernel/$kernel_bound encoder=$encoder/$encoder_bound"
if [[ $kernel -le $kernel_bound && $encoder -le $encoder_bound ]]; then
  print -r -- "$line OK"
  exit 0
fi
print -r -- "$line FAIL"
exit 1
