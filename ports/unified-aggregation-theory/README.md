# Port ledger: unified-aggregation-theory

This directory holds the mechanism-lang port of
MavenRain/unified-aggregation-theory (UAT).  The roadmap track is
Track 1 in `ROADMAP.md`.  Today the directory holds this ledger.  The
source port lands stage by stage.  The reusable framework lands in
`prelude/aggregate/` and the bridges stay here (D-UAT-3).

## Source pin

- Repository: https://github.com/MavenRain/unified-aggregation-theory,
  local checkout `~/Documents/unified-aggregation-theory`.
- Commit: f9d2bc2, "Add q-state Potts (S_q) instantiation of the
  trichotomy".  Toolchain leanprover/lean4:v4.31.0.  No Mathlib.
- Size: 19 `.lean` files, 2,916 lines, 93 theorems, 52 defs, 10
  structures, 3 inductives, 7 abbrevs, 3 instances, 0 sorries.
- Axioms (collectAxioms, 2026-09-06): propext, Classical.choice,
  Quot.sound.
- Dependencies: comp-cat-theory (Foundation/Category.lean 126 lines
  and Primitive/KanExtension.lean 145 lines), arrow-cat (7 files,
  1,297 lines, 56 theorems, 12 defs), kan-tactics and kan-saturation.

## Frozen export

- File: `corpus/lean-parity/uat/uat.export` in the kanon-m2-corpus
  checkout, lean4export v4.31.0, format 3.1.0.
- SHA-256: f4439dce6a0b488e9bc328592e53c47867c4d19fb123b31358aeb35ed5d14354.
- Roots: 570 (365 UnifiedAggregation, 113 ArrowCat, 92 CompCatTheory).
  Closure: 3,202 declarations.  Referenced external constants: 2,477,
  with 185 NEVER rows.  These are the denominators of PRELUDE-CHECKED
  and PARITY-UAT, frozen in `dev/denominators.json`.
- The export stays in that checkout, referenced by sha and path.  No
  copy lands in this directory (D-UAT-6).

## Module roster

One row per source module, in tier B stage order.  Lines are `wc -l`.
Theorem counts are top-level declarations, 90 in all.  Three nested
theorems bring the total to 93.  The home column follows D-UAT-3:
`prelude/aggregate` for the framework, `ports` for this directory
(`src/` or `arrow-cat/`), and `prelude` for the four category helpers,
whose directory the U2 design document names.

| module | lines | theorems | stage | home |
| --- | ---: | ---: | --- | --- |
| FunctorExt.lean | 89 | 2 | U2 | prelude |
| HeqTransport.lean | 302 | 8 | U2 | prelude |
| Discrete.lean | 76 | 0 | U2 | prelude |
| Indiscrete.lean | 91 | 1 | U2 | prelude |
| ConfigSpace.lean | 49 | 0 | U2 | prelude/aggregate |
| SymmetricGroup.lean | 104 | 6 | U2 | prelude/aggregate |
| Z2Group.lean | 80 | 5 | U2 | prelude/aggregate |
| ChoiceRule.lean | 42 | 0 | U2 | prelude/aggregate |
| Aggregation.lean | 230 | 1 | U3 | prelude/aggregate |
| Regimes.lean | 77 | 0 | U3 | prelude/aggregate |
| Characterization.lean | 97 | 4 | U3 | prelude/aggregate |
| Trichotomy.lean | 107 | 1 | U3 | prelude/aggregate |
| Bridge/ArrowImpossibility.lean | 340 | 7 | U4 | ports |
| Bridge/ArrowDebreu.lean | 130 | 2 | U4 | ports |
| Bridge/SchellingIsing.lean | 571 | 30 | U5 | ports |
| Bridge/Potts.lean | 429 | 21 | U5 | ports |
| TrichotomyWitnesses.lean | 77 | 2 | U5 | ports |
| Bridge.lean | 8 | 0 | U5 | ports |
| UnifiedAggregation.lean | 17 | 0 | U5 | ports |

## Feature census

Sites counted by `rg` over the 19 files at f9d2bc2, with the
mechanism-lang counterpart and its milestone.

- HEq 112 sites, cast 30 sites: Eq over a Lan at SPi and J at Univ
  (D1).  Data equality and transport shipped at Stage C.  The generic
  Prop form and the subsingleton criterion are M1.
- funext 35 sites (FunctorExt 3, SymmetricGroup 3, ArrowDebreu 6,
  ArrowImpossibility 6, SchellingIsing 12, Potts 5): a tracked axiom
  printed by `mech axioms` before M2 (D-UAT-4) and derived from
  Quot.sound at M2 (D2).
- `kan_` tactic call sites 132, 16 of them kan_saturate, all 16 in
  Bridge/SchellingIsing.lean: no tactic language (Q4).  Each site
  becomes a term or a call of the four D5 tools.  The 16 kan_saturate
  sites become explicit lemma chains over the normalized Rat carrier
  with the rewrite tool at fuel 512 (D-UAT-5).
- Rat 85 sites, Int 17 sites, Fin 51 sites: the M1 tower (D6).
- Classical 7 sites (byContradiction 4, em 2, choice 1): the tracked
  port axiom Classical.choice.
- universe 13 declarations: D3 prenex levels, Stage A.
- macro and syntax 15 lines, the HEq tactic suite of HeqTransport:
  NEVER under Q4.  The eight helpers become eight lemmas.
- structures 10, instances 3: non-recursive single-constructor
  structures land at SPi with eta, indexed and recursive ones stay
  SMu with no eta (verdict, parity ledger row 2).  Instances resolve
  by Auto at M1 (D4).

## Headline theorems

The PORT-UAT-HEADLINE gate checks these seven, with file and line at
f9d2bc2.  Their axioms are propext, Classical.choice and Quot.sound,
plus the tracked funext before M2 (D-UAT-4).

- `trichotomy`, Trichotomy.lean:92.
- `trichotomy_regimes_realized`, TrichotomyWitnesses.lean:65.
- `potts_trichotomy_regimes_realized`, Bridge/Potts.lean:418.
- `no_equivariant_constrained_swf`, Bridge/ArrowImpossibility.lean:273.
- `arrow_debreu_uniqueness`, Bridge/ArrowDebreu.lean:82.
- `schelling_ising_z2_degeneracy`, Bridge/SchellingIsing.lean:368.
- `mean_field_bifurcation`, Bridge/SchellingIsing.lean:485.

## Leg 2 fixture candidates

The verdict records no confirmed numeric fixture for UAT.  Stage U6
opens only after one candidate is confirmed computable on the kernel
evaluator.  Candidates, unverified:

- `Magnetization upConfig` and `Magnetization downConfig` at a fixed
  n, the extremal values n and minus n.
- `Hamiltonian` of a fixed SpinConfig at a fixed n.
- A decidable check of `IsMeanFieldFixedPoint β m` at rational β and
  m on each side of β = 1.
- `pottsUniform` as a Potts fixed point at fixed n and q.

## Layout

- `README.md`: this ledger.
- `src/`: tier B `.mech` bridge and witness modules, from U4.
- `arrow-cat/`: the arrow-cat sub-port, from U4, inside the 570-root
  denominator (D-UAT-2).
- `fixtures.json`: leg 2 fixtures with their sha, from U6.
- No `export/`: the frozen export stays in the kanon-m2-corpus
  checkout (D-UAT-6).
