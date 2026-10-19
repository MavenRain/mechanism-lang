/* Port of tests/import_protocol.bend with the cases of import_protocol_data.bend folded in as
 * static data (the CASES table is generated row for row from the Bend data file; oracle label
 * import-protocol). Runs every case through Export.read_string, prints "PASS import NAME" or
 * "FAIL import NAME: MESSAGE" per case, then "IMPORT-OK" (exit 0) or dies with 1 and
 * "IMPORT-FAIL N of 120 cases failed". With `--corpus FILE` it also reads the frozen UAT export
 * through Files.read_file, checks its table counts (case frozen-corpus, 121 cases in all) and prints
 * the CORPUS-IMPORT line on success. Any other argument list dies with 1 and the usage text. */
#include <stdlib.h>

#include "cli.h"
#include "driver.h"
#include "import.h"

/* ---- import_protocol_data.bend ---- */

/* Probe: Accept{} | Reject{line} | Numeric{} | Group{} | Dotted{} */
typedef enum { PROBE_ACCEPT, PROBE_REJECT, PROBE_NUMERIC, PROBE_GROUP, PROBE_DOTTED } ProbeKind;
typedef struct {
  ProbeKind kind;
  uint32_t line; /* PROBE_REJECT */
} Probe;

/* Case{name, source, probe}: the source is base() ++ text when base is true, else text */
typedef struct {
  const char *name;
  bool base;
  const char *text;
  Probe probe;
} Case;

/* base() */
static const char *const BASE = "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"fixture\",\"version\":\"4.31.0\"}}}\n{\"in\":1,\"str\":{\"pre\":0,\"str\":\"D\"}}\n{\"in\":2,\"str\":{\"pre\":0,\"str\":\"u\"}}\n{\"in\":3,\"str\":{\"pre\":0,\"str\":\"C\"}}\n{\"in\":4,\"str\":{\"pre\":0,\"str\":\"R\"}}\n{\"in\":5,\"str\":{\"pre\":0,\"str\":\"T\"}}\n{\"il\":1,\"succ\":0}\n{\"ie\":0,\"sort\":0}\n{\"ie\":1,\"bvar\":0}\n{\"ie\":2,\"sort\":1}\n";

/* cases(): generated from tests/import_protocol_data.bend, one row per Case in order */
static const Case CASES[] = {
  {"empty-input", false, "", {PROBE_REJECT, 1}},
  {"newline-only-input", false, "\n", {PROBE_REJECT, 1}},
  {"blank-first-line", false, "\n{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"fixture\",\"version\":\"4.31.0\"}}}\n", {PROBE_REJECT, 1}},
  {"version-3.2.0", false, "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.2.0\"},\"lean\":{\"githash\":\"fixture\",\"version\":\"4.31.0\"}}}\n", {PROBE_REJECT, 1}},
  {"version-empty", false, "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"\"},\"lean\":{\"githash\":\"fixture\",\"version\":\"4.31.0\"}}}\n", {PROBE_REJECT, 1}},
  {"missing-metadata", false, "{\"in\":1,\"str\":{\"pre\":0,\"str\":\"D\"}}\n", {PROBE_REJECT, 1}},
  {"metadata-only", false, "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"fixture\",\"version\":\"4.31.0\"}}}\n", {PROBE_ACCEPT, 0}},
  {"duplicate-metadata", false, "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"fixture\",\"version\":\"4.31.0\"}}}\n{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"fixture\",\"version\":\"4.31.0\"}}}\n", {PROBE_REJECT, 2}},
  {"blank-record", true, "\n", {PROBE_REJECT, 11}},
  {"non-object", true, "[]\n", {PROBE_REJECT, 11}},
  {"unknown-record", true, "{\"alien\":1}\n", {PROBE_REJECT, 11}},
  {"duplicate-json-key", true, "{\"in\":6,\"in\":7,\"str\":{\"pre\":0,\"str\":\"x\"}}\n", {PROBE_REJECT, 11}},
  {"duplicate-nested-key", true, "{\"in\":6,\"str\":{\"pre\":0,\"pre\":1,\"str\":\"x\"}}\n", {PROBE_REJECT, 11}},
  {"trailing-garbage", true, "{\"ie\":3,\"bvar\":0}x\n", {PROBE_REJECT, 11}},
  {"trailing-comma", true, "{\"ie\":3,\"bvar\":0,}\n", {PROBE_REJECT, 11}},
  {"invalid-escape", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\q\"}}\n", {PROBE_REJECT, 11}},
  {"lone-high-surrogate", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uD800\"}}\n", {PROBE_REJECT, 11}},
  {"lone-low-surrogate", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uDC00\"}}\n", {PROBE_REJECT, 11}},
  {"bad-surrogate-pair", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uD800\\u0041\"}}\n", {PROBE_REJECT, 11}},
  {"raw-control-string", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\001\"}}\n", {PROBE_REJECT, 11}},
  {"valid-surrogate-pair", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"\\uD83D\\uDE00\"}}\n", {PROBE_ACCEPT, 0}},
  {"valid-json-escapes", true, "{\"ie\":3,\"strVal\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0041\"}\n", {PROBE_ACCEPT, 0}},
  {"fractional-index", true, "{\"ie\":3.5,\"bvar\":0}\n", {PROBE_REJECT, 11}},
  {"boolean-index", true, "{\"ie\":true,\"bvar\":0}\n", {PROBE_REJECT, 11}},
  {"oversized-index", true, "{\"ie\":999999999999999999999999999999,\"bvar\":0}\n", {PROBE_REJECT, 11}},
  {"leading-zero-number", true, "{\"ie\":03,\"bvar\":0}\n", {PROBE_REJECT, 11}},
  {"negative-index", true, "{\"ie\":-1,\"bvar\":0}\n", {PROBE_REJECT, 11}},
  {"duplicate-name-id", true, "{\"in\":1,\"str\":{\"pre\":0,\"str\":\"other\"}}\n", {PROBE_REJECT, 11}},
  {"name-table-alias", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"D\"}}\n", {PROBE_ACCEPT, 0}},
  {"duplicate-level-id", true, "{\"il\":1,\"succ\":1}\n", {PROBE_REJECT, 11}},
  {"duplicate-expression-id", true, "{\"ie\":0,\"sort\":0}\n", {PROBE_REJECT, 11}},
  {"reserved-name-zero", true, "{\"in\":0,\"str\":{\"pre\":0,\"str\":\"zero\"}}\n", {PROBE_REJECT, 11}},
  {"reserved-level-zero", true, "{\"il\":0,\"succ\":0}\n", {PROBE_REJECT, 11}},
  {"name-forward-reference", true, "{\"in\":6,\"str\":{\"pre\":7,\"str\":\"x\"}}\n", {PROBE_REJECT, 11}},
  {"name-self-reference", true, "{\"in\":6,\"str\":{\"pre\":6,\"str\":\"x\"}}\n", {PROBE_REJECT, 11}},
  {"name-negative-number", true, "{\"in\":6,\"num\":{\"pre\":0,\"i\":-1}}\n", {PROBE_REJECT, 11}},
  {"name-two-forms", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"x\"},\"num\":{\"pre\":0,\"i\":3}}\n", {PROBE_REJECT, 11}},
  {"level-succ-forward", true, "{\"il\":2,\"succ\":3}\n", {PROBE_REJECT, 11}},
  {"level-max-forward", true, "{\"il\":2,\"max\":[0,3]}\n", {PROBE_REJECT, 11}},
  {"level-imax-forward", true, "{\"il\":2,\"imax\":[3,0]}\n", {PROBE_REJECT, 11}},
  {"level-param-forward", true, "{\"il\":2,\"param\":6}\n", {PROBE_REJECT, 11}},
  {"level-max-arity", true, "{\"il\":2,\"max\":[0,1,1]}\n", {PROBE_REJECT, 11}},
  {"level-negative-reference", true, "{\"il\":2,\"succ\":-1}\n", {PROBE_REJECT, 11}},
  {"level-max", true, "{\"il\":2,\"max\":[0,1]}\n", {PROBE_ACCEPT, 0}},
  {"level-imax", true, "{\"il\":2,\"imax\":[1,0]}\n", {PROBE_ACCEPT, 0}},
  {"level-param", true, "{\"il\":2,\"param\":2}\n", {PROBE_ACCEPT, 0}},
  {"sort-forward-level", true, "{\"ie\":3,\"sort\":2}\n", {PROBE_REJECT, 11}},
  {"bvar-negative", true, "{\"ie\":3,\"bvar\":-1}\n", {PROBE_REJECT, 11}},
  {"const-forward-name", true, "{\"ie\":3,\"const\":{\"name\":6,\"us\":[0]}}\n", {PROBE_REJECT, 11}},
  {"const-forward-level", true, "{\"ie\":3,\"const\":{\"name\":1,\"us\":[2]}}\n", {PROBE_REJECT, 11}},
  {"app-forward-function", true, "{\"ie\":3,\"app\":{\"fn\":4,\"arg\":1}}\n", {PROBE_REJECT, 11}},
  {"app-forward-argument", true, "{\"ie\":3,\"app\":{\"fn\":0,\"arg\":4}}\n", {PROBE_REJECT, 11}},
  {"expression-self-reference", true, "{\"ie\":3,\"app\":{\"fn\":3,\"arg\":1}}\n", {PROBE_REJECT, 11}},
  {"const", true, "{\"ie\":3,\"const\":{\"name\":1,\"us\":[0,1]}}\n", {PROBE_ACCEPT, 0}},
  {"app", true, "{\"ie\":3,\"app\":{\"fn\":0,\"arg\":1}}\n", {PROBE_ACCEPT, 0}},
  {"lambda", true, "{\"ie\":3,\"lam\":{\"name\":1,\"type\":0,\"body\":1,\"binderInfo\":\"default\"}}\n", {PROBE_ACCEPT, 0}},
  {"forall", true, "{\"ie\":3,\"forallE\":{\"name\":1,\"type\":0,\"body\":1,\"binderInfo\":\"default\"}}\n", {PROBE_ACCEPT, 0}},
  {"lambda-invalid-binder", true, "{\"ie\":3,\"lam\":{\"binderInfo\":\"unknown\",\"name\":1,\"type\":0,\"body\":1}}\n", {PROBE_REJECT, 11}},
  {"lambda-forward-name", true, "{\"ie\":3,\"lam\":{\"name\":6,\"type\":0,\"body\":1,\"binderInfo\":\"default\"}}\n", {PROBE_REJECT, 11}},
  {"lambda-forward-type", true, "{\"ie\":3,\"lam\":{\"type\":4,\"name\":1,\"body\":1,\"binderInfo\":\"default\"}}\n", {PROBE_REJECT, 11}},
  {"forall-forward-body", true, "{\"ie\":3,\"forallE\":{\"body\":4,\"name\":1,\"type\":0,\"binderInfo\":\"default\"}}\n", {PROBE_REJECT, 11}},
  {"let", true, "{\"ie\":3,\"letE\":{\"name\":1,\"type\":0,\"value\":1,\"body\":1,\"nondep\":false}}\n", {PROBE_ACCEPT, 0}},
  {"let-forward-value", true, "{\"ie\":3,\"letE\":{\"value\":4,\"name\":1,\"type\":0,\"body\":1,\"nondep\":false}}\n", {PROBE_REJECT, 11}},
  {"let-invalid-bool", true, "{\"ie\":3,\"letE\":{\"nondep\":0,\"name\":1,\"type\":0,\"value\":1,\"body\":1}}\n", {PROBE_REJECT, 11}},
  {"projection", true, "{\"ie\":3,\"proj\":{\"typeName\":1,\"idx\":0,\"struct\":1}}\n", {PROBE_ACCEPT, 0}},
  {"projection-forward-name", true, "{\"ie\":3,\"proj\":{\"typeName\":6,\"idx\":0,\"struct\":1}}\n", {PROBE_REJECT, 11}},
  {"projection-forward-structure", true, "{\"ie\":3,\"proj\":{\"struct\":4,\"typeName\":1,\"idx\":0}}\n", {PROBE_REJECT, 11}},
  {"projection-negative-field", true, "{\"ie\":3,\"proj\":{\"idx\":-1,\"typeName\":1,\"struct\":1}}\n", {PROBE_REJECT, 11}},
  {"large-natural-literal", true, "{\"ie\":3,\"natVal\":\"9999999999999999999999999999999999999999\"}\n", {PROBE_ACCEPT, 0}},
  {"natural-number-not-string", true, "{\"ie\":3,\"natVal\":1}\n", {PROBE_REJECT, 11}},
  {"natural-negative", true, "{\"ie\":3,\"natVal\":\"-1\"}\n", {PROBE_REJECT, 11}},
  {"natural-empty", true, "{\"ie\":3,\"natVal\":\"\"}\n", {PROBE_REJECT, 11}},
  {"string-literal", true, "{\"ie\":3,\"strVal\":\"hello\"}\n", {PROBE_ACCEPT, 0}},
  {"metadata-expression", true, "{\"ie\":3,\"mdata\":{\"expr\":0,\"data\":{\"x\":[null,1,true]}}}\n", {PROBE_ACCEPT, 0}},
  {"metadata-expression-forward", true, "{\"ie\":3,\"mdata\":{\"expr\":4,\"data\":{}}}\n", {PROBE_REJECT, 11}},
  {"metadata-expression-duplicate-key", true, "{\"ie\":3,\"mdata\":{\"expr\":0,\"data\":{\"x\":0,\"x\":1}}}\n", {PROBE_REJECT, 11}},
  {"metadata-expression-bad-data", true, "{\"ie\":3,\"mdata\":{\"expr\":0,\"data\":[]}}\n", {PROBE_REJECT, 11}},
  {"axiom", true, "{\"axiom\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n", {PROBE_ACCEPT, 0}},
  {"definition", true, "{\"def\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"hints\":\"abbrev\",\"safety\":\"safe\",\"all\":[1]}}\n", {PROBE_ACCEPT, 0}},
  {"theorem", true, "{\"thm\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"all\":[1]}}\n", {PROBE_ACCEPT, 0}},
  {"opaque", true, "{\"opaque\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"isUnsafe\":false,\"all\":[1]}}\n", {PROBE_ACCEPT, 0}},
  {"quotient", true, "{\"quot\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"kind\":\"type\"}}\n", {PROBE_ACCEPT, 0}},
  {"inductive-group", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_ACCEPT, 0}},
  {"declaration-name-forward", true, "{\"axiom\":{\"name\":6,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n", {PROBE_REJECT, 11}},
  {"declaration-type-forward", true, "{\"axiom\":{\"type\":3,\"name\":1,\"levelParams\":[2],\"isUnsafe\":false}}\n", {PROBE_REJECT, 11}},
  {"declaration-level-parameter-forward", true, "{\"axiom\":{\"levelParams\":[6],\"name\":1,\"type\":0,\"isUnsafe\":false}}\n", {PROBE_REJECT, 11}},
  {"axiom-boolean-required", true, "{\"axiom\":{\"isUnsafe\":\"false\",\"name\":1,\"levelParams\":[2],\"type\":0}}\n", {PROBE_REJECT, 11}},
  {"declaration-missing-field", true, "{\"axiom\":{\"name\":1,\"levelParams\":[2],\"isUnsafe\":false}}\n", {PROBE_REJECT, 11}},
  {"declaration-extra-field", true, "{\"axiom\":{\"extra\":0,\"name\":1,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n", {PROBE_REJECT, 11}},
  {"definition-value-forward", true, "{\"def\":{\"value\":3,\"name\":1,\"levelParams\":[2],\"type\":0,\"hints\":\"abbrev\",\"safety\":\"safe\",\"all\":[1]}}\n", {PROBE_REJECT, 11}},
  {"definition-all-forward", true, "{\"def\":{\"all\":[6],\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"hints\":\"abbrev\",\"safety\":\"safe\"}}\n", {PROBE_REJECT, 11}},
  {"definition-invalid-safety", true, "{\"def\":{\"safety\":\"maybe\",\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"hints\":\"abbrev\",\"all\":[1]}}\n", {PROBE_REJECT, 11}},
  {"definition-invalid-hints", true, "{\"def\":{\"hints\":\"maybe\",\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"safety\":\"safe\",\"all\":[1]}}\n", {PROBE_REJECT, 11}},
  {"definition-negative-regular-hint", true, "{\"def\":{\"hints\":{\"regular\":-1},\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"safety\":\"safe\",\"all\":[1]}}\n", {PROBE_REJECT, 11}},
  {"definition-regular-hint", true, "{\"def\":{\"hints\":{\"regular\":2},\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"safety\":\"safe\",\"all\":[1]}}\n", {PROBE_ACCEPT, 0}},
  {"theorem-value-forward", true, "{\"thm\":{\"value\":3,\"name\":1,\"levelParams\":[2],\"type\":0,\"all\":[1]}}\n", {PROBE_REJECT, 11}},
  {"theorem-all-forward", true, "{\"thm\":{\"all\":[6],\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1}}\n", {PROBE_REJECT, 11}},
  {"opaque-value-forward", true, "{\"opaque\":{\"value\":3,\"name\":1,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false,\"all\":[1]}}\n", {PROBE_REJECT, 11}},
  {"opaque-all-forward", true, "{\"opaque\":{\"all\":[6],\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"isUnsafe\":false}}\n", {PROBE_REJECT, 11}},
  {"opaque-invalid-bool", true, "{\"opaque\":{\"isUnsafe\":1,\"name\":1,\"levelParams\":[2],\"type\":0,\"value\":1,\"all\":[1]}}\n", {PROBE_REJECT, 11}},
  {"quotient-invalid-kind", true, "{\"quot\":{\"kind\":\"elim\",\"name\":1,\"levelParams\":[2],\"type\":0}}\n", {PROBE_REJECT, 11}},
  {"inductive-all-forward", true, "{\"inductive\":{\"types\":[{\"all\":[6],\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"inductive-ctors-forward", true, "{\"inductive\":{\"types\":[{\"ctors\":[6],\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"inductive-negative-count", true, "{\"inductive\":{\"types\":[{\"numIndices\":-1,\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"constructor-induct-forward", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"induct\":6,\"name\":3,\"levelParams\":[2],\"type\":0,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"constructor-type-forward", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"type\":3,\"name\":3,\"levelParams\":[2],\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"recursor-all-forward", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"all\":[6],\"name\":4,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"recursor-rule-ctor-forward", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"rules\":[{\"ctor\":6,\"nfields\":0,\"rhs\":1}],\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"recursor-rule-rhs-forward", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"rules\":[{\"rhs\":3,\"ctor\":3,\"nfields\":0}],\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"recursor-rule-negative-fields", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"rules\":[{\"nfields\":-1,\"ctor\":3,\"rhs\":1}],\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"recursor-invalid-k", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"k\":\"false\",\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"constructor-index-mismatch", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"cidx\":1,\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"constructor-outside-ctors-list", true, "{\"inductive\":{\"types\":[{\"ctors\":[1],\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"recursor-rule-field-mismatch", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"rules\":[{\"nfields\":2,\"ctor\":3,\"rhs\":1}],\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"recursor-rule-names-non-constructor", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"rules\":[{\"ctor\":4,\"nfields\":0,\"rhs\":1}],\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_REJECT, 11}},
  {"duplicate-declaration-id", true, "{\"axiom\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n", {PROBE_REJECT, 12}},
  {"duplicate-declaration-via-name-alias", true, "{\"axiom\":{\"name\":1,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n{\"in\":6,\"str\":{\"pre\":0,\"str\":\"D\"}}\n{\"axiom\":{\"name\":6,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n", {PROBE_REJECT, 13}},
  {"numeric-and-string-name-identity", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"1\"}}\n{\"in\":7,\"num\":{\"pre\":0,\"i\":1}}\n{\"axiom\":{\"name\":6,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":7,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n", {PROBE_NUMERIC, 0}},
  {"group-counts-all-members", true, "{\"inductive\":{\"types\":[{\"name\":1,\"levelParams\":[2],\"type\":0,\"numParams\":0,\"numIndices\":0,\"all\":[1],\"ctors\":[3],\"numNested\":0,\"isRec\":false,\"isUnsafe\":false,\"isReflexive\":false}],\"ctors\":[{\"name\":3,\"levelParams\":[2],\"type\":0,\"induct\":1,\"cidx\":0,\"numParams\":0,\"numFields\":0,\"isUnsafe\":false}],\"recs\":[{\"name\":4,\"levelParams\":[2],\"type\":0,\"all\":[1],\"numParams\":0,\"numIndices\":0,\"numMotives\":1,\"numMinors\":1,\"rules\":[{\"ctor\":3,\"nfields\":0,\"rhs\":1}],\"k\":false,\"isUnsafe\":false}]}}\n", {PROBE_GROUP, 0}},
  {"dotted-component-and-name-path-identity", true, "{\"in\":6,\"str\":{\"pre\":0,\"str\":\"x.y\"}}\n{\"in\":7,\"str\":{\"pre\":0,\"str\":\"x\"}}\n{\"in\":8,\"str\":{\"pre\":7,\"str\":\"y\"}}\n{\"axiom\":{\"name\":6,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":8,\"levelParams\":[2],\"type\":0,\"isUnsafe\":false}}\n", {PROBE_DOTTED, 0}},
};
static const size_t CASE_COUNT = 120;

static Str cat(Heap *heap, Str a, Str b) { return heap_str_cat(heap, a, b); }

static Str *boxed(Heap *heap, Str value) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box != NULL) *box = value;
  return box;
}

/* the source text of one case */
static Str case_source(Heap *heap, const Case *item) {
  if (item->base) return cat(heap, str_lit(BASE), str_lit(item->text));
  return str_lit(item->text);
}

/* ---- import_protocol.bend ---- */

/* Result<String, Unit> */
typedef struct {
  bool ok;
  Str message;
} Check;

/* Result<String, String> */
typedef struct {
  bool ok;
  Str message;
  Str text;
} TextFound;

static Check done(void) {
  Check result = {true, str_lit("")};
  return result;
}

static Check fail(Str message) {
  Check result = {false, message};
  return result;
}

/* require(ok, message) */
static Check require(bool ok, Str message) {
  if (ok) return done();
  return fail(message);
}

/* parsed(value): the Fail side as "line N: MESSAGE" */
static Check parse_failure(Heap *heap, ExportError error) {
  Str line = cat(heap, str_lit("line "), str_of_u64(heap->arena, error.line));
  return fail(cat(heap, cat(heap, line, str_lit(": ")), error.message));
}

/* reject(line, value) */
static Check reject(Heap *heap, uint32_t line, const ExportResult *value) {
  if (value->kind != JSON_FAIL) return fail(str_lit("malformed input was accepted"));
  uint32_t actual = value->error.line;
  Str message = value->error.message;
  Str text = cat(heap, str_lit("wrong location or empty diagnostic: "), str_of_u64(heap->arena, actual));
  return require(line == actual && !str_eq(message, str_lit("")),
                 cat(heap, cat(heap, text, str_lit(": ")), message));
}

/* str_name(value, prefix, text) */
static bool str_name(ExportName value, const char *prefix, const char *text) {
  return value.present && value.value.kind == NAME_STR && str_eq(value.value.prefix, str_lit(prefix)) &&
         str_eq(value.value.text, str_lit(text));
}

/* num_name(value) */
static bool num_name(ExportName value) {
  return value.present && value.value.kind == NAME_NUM && str_eq(value.value.prefix, str_lit("0")) &&
         str_eq(value.value.index, str_lit("1"));
}

/* declarations(value, n) */
static bool declarations(ExportCounts value, size_t n) { return value.declarations == n; }

/* different(a, b) */
static bool different(ExportText a, ExportText b) { return a.present && b.present && !str_eq(a.value, b.value); }

/* inspect(probe, table) */
static Check inspect(Probe probe, const Export *table) {
  switch (probe.kind) {
  case PROBE_NUMERIC: {
    Check check = require(str_name(export_name(table, str_lit("6")), "0", "1"), str_lit("string name segment changed"));
    if (!check.ok) return check;
    check = require(num_name(export_name(table, str_lit("7"))), str_lit("numeric name segment changed"));
    if (!check.ok) return check;
    return require(declarations(export_counts(table), 2), str_lit("distinct names lost a declaration"));
  }
  case PROBE_GROUP: {
    Check check = require(declarations(export_counts(table), 3), str_lit("group members missing"));
    if (!check.ok) return check;
    return require(export_groups(table).len == 1, str_lit("group identity lost"));
  }
  case PROBE_DOTTED: {
    Check check = require(str_name(export_name(table, str_lit("6")), "0", "x.y"),
                          str_lit("dotted component structure changed"));
    if (!check.ok) return check;
    check = require(str_name(export_name(table, str_lit("8")), "7", "y"), str_lit("nested name structure changed"));
    if (!check.ok) return check;
    return require(different(export_name_string(table, str_lit("6")), export_name_string(table, str_lit("8"))),
                   str_lit("distinct name structures have the same display"));
  }
  case PROBE_ACCEPT:
    return done();
  case PROBE_REJECT:
    return done();
  }
  return done();
}

/* test(probe, value) */
static Check test(Heap *heap, Probe probe, const ExportResult *value) {
  if (probe.kind == PROBE_REJECT) return reject(heap, probe.line, value);
  if (value->kind == JSON_FAIL) return parse_failure(heap, value->error);
  return inspect(probe, &value->value);
}

/* inhouse(name) */
static bool inhouse(Str name) {
  return str_eq(name, str_lit("UnifiedAggregation")) || str_starts_with(name, str_lit("UnifiedAggregation.")) ||
         str_eq(name, str_lit("ArrowCat")) || str_starts_with(name, str_lit("ArrowCat.")) ||
         str_eq(name, str_lit("CompCatTheory")) || str_starts_with(name, str_lit("CompCatTheory."));
}

/* name_of(source, id) */
static TextFound name_of(const Export *source, Str id) {
  ExportText text = export_name_string(source, id);
  TextFound missing = {false, str_lit("missing name after read"), str_lit("")};
  TextFound found = {true, str_lit(""), text.value};
  if (text.present) return found;
  return missing;
}

/* frozen_counts(value) */
static bool frozen_counts(ExportCounts value) {
  return value.lines == 219778 && value.names == 17759 && value.levels == 146 && value.exprs == 198927 &&
         value.declarations == 3202;
}

/* kind_count(rows, kind): rows are DeclEntry* items */
static size_t kind_count(const Vec *rows, const char *kind) {
  size_t count = 0;
  size_t i = 0;
  for (i = 0; i < rows->len; i += 1) {
    const DeclEntry *entry = rows->items[i];
    if (str_eq(decl_kind_name(&entry->kind), str_lit(kind))) count += 1;
  }
  return count;
}

/* kind_counts(rows) */
static Check kind_counts(const Vec *rows) {
  static const char *const KINDS[] = {"axiom", "def", "thm", "opaque", "quot", "inductive", "constructor", "recursor"};
  static const size_t EXPECTED[] = {3, 1176, 1649, 1, 4, 112, 143, 114};
  static const char *const MESSAGES[] = {"kind axiom count changed",     "kind def count changed",
                                         "kind thm count changed",       "kind opaque count changed",
                                         "kind quot count changed",      "kind inductive count changed",
                                         "kind constructor count changed", "kind recursor count changed"};
  size_t i = 0;
  for (i = 0; i < sizeof(KINDS) / sizeof(KINDS[0]); i += 1) {
    Check check = require(kind_count(rows, KINDS[i]) == EXPECTED[i], str_lit(MESSAGES[i]));
    if (!check.ok) return check;
  }
  return done();
}

/* List.sort(String, String.is_le, _) order on Str* items */
static int by_text(const void *a, const void *b) {
  const Str *x = *(void *const *)a;
  const Str *y = *(void *const *)b;
  return str_cmp(*x, *y);
}

/* axiom_set(value): value is sorted, Str* items */
static bool axiom_set(const Vec *value) {
  static const char *const AXIOMS[] = {"Classical.choice", "Quot.sound", "propext"};
  if (value->len != 3) return false;
  size_t i = 0;
  for (i = 0; i < 3; i += 1) {
    const Str *text = value->items[i];
    if (!str_eq(*text, str_lit(AXIOMS[i]))) return false;
  }
  return true;
}

/* declaration_stats(rows, source, 0n, []) */
static Check declaration_stats(Heap *heap, const Vec *rows, const Export *source) {
  size_t total = 0;
  Vec axioms = vec_new(heap->arena);
  size_t i = 0;
  for (i = 0; i < rows->len; i += 1) {
    const DeclEntry *entry = rows->items[i];
    TextFound text = name_of(source, entry->name);
    if (!text.ok) return fail(text.message);
    if (!inhouse(text.text)) total += 1;
    if (str_eq(decl_kind_name(&entry->kind), str_lit("axiom"))) {
      Str *box = boxed(heap, text.text);
      if (box == NULL || !vec_push(&axioms, box)) heap->oom = true;
    }
  }
  if (heap->oom) return fail(str_lit("out of memory"));
  Check check = require(total == 2543, str_lit("external declaration denominator changed"));
  if (!check.ok) return check;
  if (axioms.len > 1) qsort(axioms.items, axioms.len, sizeof(axioms.items[0]), by_text);
  return require(axiom_set(&axioms), str_lit("axiom set changed"));
}

/* the Unit{} value stored by Table.put (a non-NULL row) */
static int unit_value = 0;

/* constant(expr, source, constants) */
static Check constant(const Expr *expr, const Export *source, Heap *heap, Table *constants) {
  if (expr->kind != EXPR_CONST) return done();
  TextFound text = name_of(source, expr->name);
  if (!text.ok) return fail(text.message);
  table_put(heap, constants, text.text, &unit_value);
  return done();
}

/* constants_loop(remaining, index, source, constants) */
static Check constants_loop(Heap *heap, size_t remaining, const Export *source, Table *constants) {
  size_t index = 0;
  for (index = 0; index < remaining; index += 1) {
    Str key = str_of_u64(heap->arena, index);
    ExportExpr expr = export_expr(source, key);
    if (!expr.present) return fail(cat(heap, str_lit("missing frozen expression "), key));
    Check check = constant(&expr.value, source, heap, constants);
    if (!check.ok) return check;
  }
  return done();
}

/* external_names(names): Str* items */
static size_t external_names(const Vec *names) {
  size_t count = 0;
  size_t i = 0;
  for (i = 0; i < names->len; i += 1) {
    const Str *name = names->items[i];
    if (!inhouse(*name)) count += 1;
  }
  return count;
}

/* corpus_table(source) */
static Check corpus_table(Heap *heap, const Export *source) {
  Check check = require(frozen_counts(export_counts(source)), str_lit("frozen table counts changed"));
  if (!check.ok) return check;
  Vec rows = export_declarations(source);
  check = kind_counts(&rows);
  if (!check.ok) return check;
  check = declaration_stats(heap, &rows, source);
  if (!check.ok) return check;
  Table constants = table_empty(heap);
  check = constants_loop(heap, 198927, source, &constants);
  if (!check.ok) return check;
  Vec keys = table_keys(heap, &constants);
  return require(table_size(&constants) == 3017 && external_names(&keys) == 2477,
                 str_lit("const-node denominator changed"));
}

/* corpus_result(value): corpus_input, then parsed(E.read_string(text)), then corpus_table */
static Check corpus_result(Heap *heap, IoResult value) {
  if (value.status == IO_FAIL) return fail(cat(heap, str_lit("line 0: "), value.value));
  ExportResult table = export_read_string(heap, value.value);
  if (table.kind == JSON_FAIL) return parse_failure(heap, table.error);
  return corpus_table(heap, &table.value);
}

/* report(value, name): prints the verdict line, returns 1 on a failure */
static size_t report(Heap *heap, Check value, Str name) {
  if (value.ok) {
    driver_print(cat(heap, str_lit("PASS import "), name));
    return 0;
  }
  driver_print(cat(heap, cat(heap, cat(heap, str_lit("FAIL import "), name), str_lit(": ")), value.message));
  return 1;
}

/* run(cases, 0n) */
static size_t run(Heap *heap) {
  size_t failed = 0;
  size_t i = 0;
  for (i = 0; i < CASE_COUNT; i += 1) {
    const Case *item = &CASES[i];
    ExportResult value = export_read_string(heap, case_source(heap, item));
    failed += report(heap, test(heap, item->probe, &value), str_lit(item->name));
  }
  return failed;
}

/* verdict(failed, total) */
static int verdict(Heap *heap, size_t failed, size_t total) {
  if (failed == 0) return driver_print(str_lit("IMPORT-OK"));
  Str text = cat(heap, str_lit("IMPORT-FAIL "), str_of_u64(heap->arena, failed));
  text = cat(heap, cat(heap, text, str_lit(" of ")), str_of_u64(heap->arena, total));
  return driver_die_text(1, cat(heap, text, str_lit(" cases failed")));
}

/* corpus_report(value) */
static size_t corpus_report(Heap *heap, Check value) {
  if (value.ok)
    driver_print(str_lit("CORPUS-IMPORT declarations=3202 external=2477 external_declared=2543 axioms=3"));
  return report(heap, value, str_lit("frozen-corpus"));
}

/* arguments(args): args = argv without the program name */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc <= 1) {
    size_t failed = run(heap);
    return verdict(heap, failed, 120);
  }
  if (argc == 3 && str_eq(str_lit(argv[1]), str_lit("--corpus"))) {
    size_t failed = run(heap);
    IoResult text = io_read_file(heap, str_lit(argv[2]));
    size_t bad = corpus_report(heap, corpus_result(heap, text));
    return verdict(heap, failed + bad, 121);
  }
  return driver_die_text(1, str_lit("IMPORT-FAIL usage: import.exe [--corpus FILE]"));
}

/* main() */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
