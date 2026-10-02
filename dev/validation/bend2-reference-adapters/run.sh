#!/bin/zsh
# Post-fix live check: suite groups run separately so a stale corpus in one group does not hide the others.
R=/Users/oobi/Documents/mechanism-lang
W=/Users/oobi/Documents/mechanism-lang-bend2-reference-review
REF=/Users/oobi/Documents/gpt6/mechanism-lang-ocaml-reference-7a2f9f2
cd $R || exit 9
rm -rf $W/postfix; mkdir -p $W/postfix
: > $W/postfix-status.txt
for group in "json export translate" surface erase parser pipeline; do
  tag=${group// /-}
  args=(); for s in ${=group}; do args+=(--suite $s); done
  start=$(date +%s)
  python3 -P dev/bend2-reference-check.py --reference $REF $args --output $W/postfix/$tag > $W/postfix/$tag.stdout 2> $W/postfix/$tag.stderr
  echo "$tag exit=$? secs=$(( $(date +%s) - start ))" >> $W/postfix-status.txt
done
start=$(date +%s)
python3 -P dev/bend2-reference-test.py --reference $REF > $W/postfix/reference-test.stdout 2> $W/postfix/reference-test.stderr
echo "reference-test exit=$? secs=$(( $(date +%s) - start ))" >> $W/postfix-status.txt
echo DONE >> $W/postfix-status.txt
