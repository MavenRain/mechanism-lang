#!/bin/zsh
# dev/gates.sh
# The M0 gate battery: the Stage 0, A, B and C legs of plan section 9, in the order
# the plan writes them (M0-PLAN.md:143 and :173), plus the LEVELS,
# SUITE-SURFACE and SUITE-WASM suites of mechanism-lang itself, which run
# beside SUITE-KERNEL and belong to no row of section 9.  Example:
#   zsh /Users/oobi/Documents/mechanism-lang/dev/gates.sh
#
# Each leg prints one PASS or FAIL line.  A FAIL adds the leg's captured
# output under its line.  BUILD is the one leg that ends the run,
# because every later leg reads the build.  Every other leg runs even
# when an earlier one failed, so one run names every failing leg.  After
# the last leg the script prints the MEASURE block, one line per leg in
# the order above, then GATES-OK and exit 0, or GATES-FAIL and exit 1.
#
# The root comes from this script's own path, so a copy of the
# repository under a scratch directory gates itself.
#
# The script also runs one leg alone, which is how the watchdog wraps a
# leg whose body is a shell function:
#   zsh dev/gates.sh --leg pin

set -u

# The user shell startup files add a chpwd hook that reads an unset
# parameter.  Under set -u that hook fails and cd inherits its non-zero
# status, so the hooks are cleared before any cd.
chpwd_functions=()
unfunction chpwd 2>/dev/null

# EPOCHREALTIME carries microseconds, which is the resolution
# gate_timed reports in milliseconds.
zmodload zsh/datetime

SELF=${0:A}
ROOT=${0:A:h}/..
ROOT=${ROOT:A}
WORK=$ROOT/.gatework/gates
MEASURE_FILE=$WORK/measure.txt
mkdir -p $WORK

# The kanon pin.  The PIN leg reads this literal against the three
# places that record the sha.  No agent moves this number.
PIN_SHA=936a43a92dd59a04698648f24fa5ae94cdb532df

# The watchdog.  GNU coreutils ships timeout as gtimeout on stock macOS.
watchdog=""
if command -v timeout > /dev/null 2>&1; then
  watchdog=timeout
elif command -v gtimeout > /dev/null 2>&1; then
  watchdog=gtimeout
fi

if [[ -z $watchdog ]]; then
  print -r -- "FAIL-WATCHDOG (no timeout or gtimeout on PATH)"
  print -r -- "GATES-FAIL"
  exit 1
fi

# The named tiers, in seconds.  A tier is a hang ceiling, not a budget:
# a leg that grows from one second to nine stays green at FAST and shows
# the growth in the MEASURE block.  These four lines hold every numeric
# watchdog literal in this file.
FAST=10
MED=30
SLOW=120
SUITE=300

# gate_timed TIER NAME CMD...
# Runs one leg under the named tier, records the elapsed wall time in
# milliseconds, and forwards the leg's output and exit code unchanged.
# It adds no policy:  a green leg stays green and a red leg stays red.
gate_timed () {
  local tier=$1
  local name=$2
  shift 2
  local seconds=${(P)tier}
  local t0=$EPOCHREALTIME
  local out
  out=$("$watchdog" "$seconds" "$@" 2>&1)
  local code=$?
  local t1=$EPOCHREALTIME
  printf 'MEASURE %s tier=%s elapsed_ms=%.3f exit=%d\n' \
    "$name" "$tier" "$(( (t1 - t0) * 1000 ))" "$code" >> $MEASURE_FILE
  print -r -- "$out"
  return $code
}

# --- the leg bodies that need more than one command -------------------
#
# Each one prints its own PASS or FAIL line, because its verdict line
# carries a value.  The battery below runs them through the watchdog as
# "zsh dev/gates.sh --leg NAME".

# PIN.  The PIN file, the index gitlink for vendor/kanon and the
# submodule HEAD all read the pin (M0-PLAN.md:146, tally
# dev/gates-tally-m1.sh:127-133).
leg_pin () {
  local pin gitlink head line
  pin=$(cat $ROOT/PIN)
  gitlink=$(git -C $ROOT ls-files -s vendor/kanon | awk '{ print $2 }')
  head=$(git -C $ROOT/vendor/kanon rev-parse HEAD)
  line="PIN pin=$pin gitlink=$gitlink head=$head want=$PIN_SHA"
  if [[ $pin == $PIN_SHA && $gitlink == $PIN_SHA && $head == $PIN_SHA ]]; then
    print -r -- "PASS $line"
    return 0
  fi
  print -r -- "FAIL $line"
  return 1
}

# DENOMINATORS.  The digest of dev/denominators.json is the one the
# frozen numbers were hashed under (M0-PLAN.md:165).  The cd runs inside
# the quoted command, so the entry holds the bare file name.
leg_denominators () {
  local out code
  out=$(zsh -c "cd $ROOT/dev && shasum -c DENOMINATORS.sha256" 2>&1)
  code=$?
  if [[ $code -eq 0 ]] && print -r -- "$out" | rg -q -- '^denominators\.json: OK$'; then
    print -r -- "PASS DENOMINATORS $out"
    return 0
  fi
  print -r -- "FAIL DENOMINATORS $out"
  return 1
}

if [[ ${1:-} == --leg ]]; then
  case ${2:-} in
    pin) leg_pin; exit $? ;;
    denominators) leg_denominators; exit $? ;;
    *) print -r -- "gates: unknown leg ${2:-}"; exit 64 ;;
  esac
fi

if [[ $# -ne 0 ]]; then
  print -r -- "usage: zsh dev/gates.sh [--leg NAME]"
  exit 64
fi

: > $MEASURE_FILE || exit 9
fail=0

# leg TIER NAME ORACLE CMD...
#   ORACLE is a ripgrep pattern that the leg's output must hold when the
#   leg exits 0.  The word SELF means the leg prints its own verdict
#   line, because that line carries a value, and its whole output
#   belongs on stdout.
leg () {
  local tier=$1
  local name=$2
  local oracle=$3
  shift 3
  local out code
  out=$(gate_timed $tier $name "$@")
  code=$?
  if [[ $oracle == "SELF" ]]; then
    print -r -- "$out"
    if [[ $code -eq 0 ]]; then
      return 0
    fi
    if ! print -r -- "$out" | rg -q -- "^FAIL $name"; then
      print -r -- "FAIL $name"
    fi
    fail=1
    return 1
  fi
  if [[ $code -eq 0 ]] && print -r -- "$out" | rg -q -- "$oracle"; then
    print -r -- "PASS $name"
    return 0
  fi
  print -r -- "FAIL $name"
  print -r -- "$out"
  fail=1
  return 1
}

# The legs, in the order of plan section 9.  BUILD ends the run when it
# fails, because every later leg reads the build it makes.
if ! leg SLOW BUILD '0 errors, 0 warnings' zsh $ROOT/dev/dunecho.sh build; then
  print -r -- ""
  cat $MEASURE_FILE
  print -r -- ""
  print -r -- "GATES-FAIL"
  exit 1
fi

leg FAST PIN SELF zsh $SELF --leg pin
leg MED PIN-DELTA '^PIN-DELTA OK$' zsh $ROOT/dev/pin-delta.sh
leg FAST R0-COUNT '^R0-COUNT OK$' zsh $ROOT/dev/r0-count.sh
leg FAST R0-AUDIT '^R0-AUDIT OK$' zsh $ROOT/dev/r0-audit.sh $ROOT

# Each executable below links mechanism's rebuilt kernel, surface and
# backend.  The pinned fixture files remain the input corpus, while the
# WASM suite writes its artifacts outside the vendor checkout.
leg SUITE SUITE-KERNEL '^SUITE-KERNEL OK$' \
  $ROOT/_build/default/test/main.exe $ROOT/vendor/kanon/test
leg FAST LEVELS '^LEVELS-OK$' $ROOT/_build/default/test/levels.exe
leg FAST SUITE-SURFACE '^SL-SURFACE OK$' $ROOT/_build/default/test/sl_surface.exe
leg SUITE SUITE-WASM '^SUITE-WASM OK$' \
  $ROOT/_build/default/test/wasm.exe $ROOT/vendor/kanon/test $ROOT/.gatework/wasm-suite
leg MED IMPORT-GRAMMAR '^IMPORT-OK$' zsh $ROOT/dev/import-gates.sh grammar
leg FAST IMPORT-CLI '^IMPORT-CLI OK cases=12$' \
  python3 -P $ROOT/test/import_cli.py $ROOT/_build/default/bin/mech.exe
leg MED CORPUS-UAT '^CORPUS-OK$' zsh $ROOT/dev/import-gates.sh corpus
leg MED PARITY-COUNTS '^PARITY-COUNTS OK$' zsh $ROOT/dev/import-gates.sh counts
leg FAST PRELUDE '^PRELUDE-OK families=' $ROOT/_build/default/test/prelude.exe $ROOT
leg FAST AXIOMS '^AXIOMS OK prelude=0 fixture=1 hidden_builtins=0$' \
  python3 -P $ROOT/dev/prelude-gates.py axioms
leg MED MAP-INVENTORY '^MAP-INVENTORY OK$' \
  python3 -P $ROOT/dev/prelude-gates.py mapping
leg FAST TRUSTED-LINES '^TRUSTED-LINES kernel=[0-9]+/[0-9]+ encoder=[0-9]+/[0-9]+ OK$' \
  zsh $ROOT/dev/trusted-lines.sh $ROOT
leg MED DENOMINATORS SELF zsh $SELF --leg denominators

# The legs of plan section 9 that later stages add.  Each one lands with
# the stage named beside it, so this file grows by edit and never by
# rewrite.
#   Stage D: PRELUDE-CHECKED, M0-TIME.
#   Stage E: AUCTION-EXPORT, HOUSE.

print -r -- ""
cat $MEASURE_FILE
print -r -- ""

if [[ $fail -eq 0 ]]; then
  print -r -- "GATES-OK"
  exit 0
fi

print -r -- "GATES-FAIL"
exit 1
