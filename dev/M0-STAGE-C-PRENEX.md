# M0 Stage C: textual prenex definitions

Date: 2026-09-10.  Base: c9869ef, the committed congruence slice.
This increment exposes universally checked definition templates through
the source language.  Stage C and PRELUDE-CHECKED remain open.

## Grammar

```text
level       ::= NAT | NAME | '(' level ')'
              | '(succ' level ')' | '(max' level level ')'
              | '(imax' level level ')'
term        ::= ... | 'Sort' level
declaration ::= ...
              | 'poly' '(' NAME (',' NAME)* ')' 'def' NAME ':' term
                ':=' term
              | 'specialize' NAME '(' level (',' level)* ')' 'as' NAME
```

Sort uses raw universe levels: zero denotes Prop and successor n denotes
Type n.  Existing Prop and Type syntax retains its meaning.  Numeric level
atoms retain the inherited eighteen-digit limit and checked machine-int
narrowing.  Successor expressions keep their syntax when printed, so a
composed level above that digit limit still round-trips.

A binder list is nonempty and its names are distinct.  Universe names have
their own scope for one template's type and body, independent of term
binders.  The parser resolves them to indices in binder order.  Its printer
uses canonical names u0, u1 and so on.  A fresh immutable parser scope
handles each template.  An ordinary declaration or specialization has no
universe binders and rejects an open level argument.

The new reserved words are Sort, poly and specialize.  Existing source
that used those words as identifiers must rename them.

## Checking and lifetime

The elaborator checks a template under its declared universe arity and
passes the resulting declaration to Poly.declare.  That API checks it for
all universe assignments and stores it outside Global.t.  A body that only
works at one universe is rejected before any specialization occurs.

Specialization requires exactly the declared number of closed arguments
and a fresh output name.  Poly.instantiate substitutes all universe
occurrences and checks the closed type and body again.  Only that checked
instance enters globals and output rows.  Instances retain source order
and work through check, axioms, emit and run.

The program's catalog lasts for one Elab.check_in or elab_program_in call.
Templates cannot be used as bare terms or reference other templates.
Later ordinary declarations, families and recursive groups cannot reuse a
template name.  A specialization cannot reuse a global, family, template
or constructor name, and a template cannot reuse a constructor name of a
family that the same program declares before it.  Every constructor of a
mu group joins the reservation, because a constructor name occupies the
flat global namespace as its family name does.  A plain `def` that
repeats a constructor name stays accepted: that allowance comes from the
kernel at PIN, it applies to a `def` before or after the family, and
this increment does not change it.  A failed source check returns no
partial environment.

This initial slice supported nonrecursive definition templates.
The follow-up in M0-STAGE-C-PRENEX-FAMILIES.md adds single recursive
family templates. Textual polymorphic axioms, recursive definitions,
family members and ordered family groups remain unsupported.
The programmatic Poly and Family_poly APIs remain available.
There is no universe inference or implicit specialization.

## Validation

PRENEX checks the source fixture from Global.empty, checks parse/print
round-trips, counts the installed definitions, checks row order and
ensures that symbolic templates escape into no global entry.  It audits
the entries for postulates and primitives.  Four independently specified
normal forms check data and type computation.  The fixture covers Prop,
Type 0, Type 1, max and imax with independent domain and codomain levels.
Twenty-five refusal checks cover paired source misuse, budget exhaustion and
the catalog's lifetime.

PRENEX-RUNTIME checks a numeric value and a closure on the kernel, Node and
Wasmtime.  It checks the source and its axiom output before emission.  A
payload mutation changes one expected answer from 37 to 41 and leaves the
closure answer at 12.

`python3 -I dev/prenex-mutations.py NEW_WORK_DIRECTORY` builds isolated
source copies and replays seven controls.  Each mutant must build without
warnings and fail the designated suite with its diagnostic.  Source
hashes, replacements, output and the restored-suite result remain in the
work directory.

The gate battery retains every prior leg and adds PRENEX and
PRENEX-RUNTIME.  The existing trusted-line limits remain 3,000 kernel lines
and 900 encoder lines. Textual family groups and members, category targets,
checked source-type parity and the D-A-1 bound ruling remain open.

Build copy: `/Users/oobi/Documents/gpt11/mechanism-lang`.
Validated files are staged in `/Users/oobi/Documents/mechanism-lang`
after checking the original HEAD and file contents.  No commit is created.
