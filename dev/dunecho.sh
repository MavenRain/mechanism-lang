#!/bin/zsh
# dev/dunecho.sh ARGS...
# Runs dunecho when installed, otherwise dune, from this repository.
# Every dune verb at every stage goes through this runner
# (M0-PLAN.md section 11).  Example:
#   zsh dev/dunecho.sh build
#
# The root comes from this script's own path, never from a literal, so a
# copy of the repository under a scratch directory builds itself.

set -u

# The user shell startup files add a chpwd hook that reads an unset
# parameter.  Under set -u that hook fails and cd inherits its non-zero
# status, so the hooks are cleared before the cd.
chpwd_functions=()
unfunction chpwd 2>/dev/null

# Honor an explicitly configured switch or the established local switch.
# CI can use `opam exec -- zsh dev/dunecho.sh build` with its selected compiler.
if [[ -n ${MECH_OPAM_SWITCH:-} ]]; then
  switch_bin=$(opam var bin --switch "$MECH_OPAM_SWITCH") || exit 3
  export PATH="$switch_bin:$PATH"
elif [[ -d "$HOME/.opam/zxcaml-p1/bin" && -z ${OPAM_SWITCH_PREFIX:-} ]]; then
  export PATH="$HOME/.opam/zxcaml-p1/bin:$PATH"
fi
cd ${0:A:h}/.. || exit 3
if command -v dunecho >/dev/null 2>&1; then
  exec dunecho "$@"
fi
exec dune "$@"
