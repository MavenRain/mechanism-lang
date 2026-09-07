#!/bin/zsh
# dev/trusted-lines.sh [ROOT]
# The TRUSTED-LINES leg of the gate battery (M0-PLAN.md:164, Stage 0
# brief 3.9).  Example:
#   zsh /Users/oobi/Documents/mechanism-lang/dev/trusted-lines.sh
#
# The trust base of a checked file is the kernel and the encoder.  The
# kernel is ten vendored files: shape.ml, term.ml, rules.ml, check.ml,
# value.ml, eval.ml, conv.ml, totality.ml, positivity.ml and order.ml,
# each read under vendor/kanon/lib, plus every lib/ overlay file of
# mechanism-lang.  The encoder is one file, vendor/kanon/wasm/gc_encode.ml,
# which writes the bytes of the module.  M0 holds the kernel at 3,000
# lines and the encoder at 900, from the design verdict's Trusted base
# (M0-PLAN.md:164, note N2).  No agent moves either number.
#
# The line prints the two counts against their bounds:
#   TRUSTED-LINES kernel=2305/3000 encoder=216/900 OK
#
# A missing overlay file is not an error, because lib/ is empty until
# Stage A.  A missing vendored file is an error.
#
# The root comes from this script's own path when no argument is given,
# so a copy of the repository under a scratch directory measures itself.
# wc and awk do the reading;  grep, sed and find are never called.

set -u

# The user shell startup files add a chpwd hook that reads an unset
# parameter.  Under set -u that hook fails, so the hooks are cleared.
chpwd_functions=()
unfunction chpwd 2>/dev/null

setopt null_glob

root=${1:-${0:A:h}/..}

kernel_bound=3000
encoder_bound=900

kernel_files=(
  $root/vendor/kanon/lib/shape.ml
  $root/vendor/kanon/lib/term.ml
  $root/vendor/kanon/lib/rules.ml
  $root/vendor/kanon/lib/check.ml
  $root/vendor/kanon/lib/value.ml
  $root/vendor/kanon/lib/eval.ml
  $root/vendor/kanon/lib/conv.ml
  $root/vendor/kanon/lib/totality.ml
  $root/vendor/kanon/lib/positivity.ml
  $root/vendor/kanon/lib/order.ml
  # The D3 overlay of Stage A.  The glob yields nothing while lib/ is
  # empty, so the fold is over the vendored files alone.
  $root/lib/*.ml
  $root/lib/*.mli
)
encoder_file=$root/vendor/kanon/wasm/gc_encode.ml

# wc -l over more than one file ends with a total row, which awk reads.
kernel_out=$(wc -l $kernel_files)
kernel_code=$?

encoder_out=$(wc -l < $encoder_file)
encoder_code=$?

if [[ $kernel_code -ne 0 || $encoder_code -ne 0 ]]; then
  print -r -- "trusted-lines: a trusted file is missing under $root"
  print -r -- "TRUSTED-LINES FAIL"
  exit 1
fi

kernel=$(print -r -- "$kernel_out" | awk 'END { print $1 }')
encoder=$(print -r -- "$encoder_out" | awk '{ print $1 }')

line="TRUSTED-LINES kernel=$kernel/$kernel_bound encoder=$encoder/$encoder_bound"

if [[ $kernel -le $kernel_bound && $encoder -le $encoder_bound ]]; then
  print -r -- "$line OK"
  exit 0
fi

print -r -- "$line FAIL"
exit 1
