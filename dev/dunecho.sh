#!/bin/zsh
# dev/dunecho.sh ARGS...
# Runs dunecho from the zxcaml-p1 opam switch with the root at this
# repository.  Every dune verb at every stage goes through this runner
# (M0-PLAN.md section 11).  Example:
#   zsh /Users/oobi/Documents/mechanism-lang/dev/dunecho.sh build
#
# The root comes from this script's own path, never from a literal, so a
# copy of the repository under a scratch directory builds itself.

set -u

# The user shell startup files add a chpwd hook that reads an unset
# parameter.  Under set -u that hook fails and cd inherits its non-zero
# status, so the hooks are cleared before the cd.
chpwd_functions=()
unfunction chpwd 2>/dev/null

export PATH=/Users/oobi/.opam/zxcaml-p1/bin:$PATH
cd ${0:A:h}/.. || exit 3
exec /Users/oobi/.local/bin/dunecho "$@"
