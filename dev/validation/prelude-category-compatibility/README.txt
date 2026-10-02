Source-compatible Category record validation

Base: d6ac149529e2511734cf7e11c5b2d0acecb111b1. Date: 2026-10-02.

PRELUDE-CATEGORY-COMPATIBILITY exits 0: eight Category signatures, one
contextual Eq signature, three generic data projection computations, a
stored-field order witness, an empty mech axioms audit, a passing
prelude.exe --audit empty-environment audit and eight controls. The
reordered and erased adapters check on their own. The stored-field order
witness rejects the reorder and the imported Category.mk signature rejects
the erasure. The same-type altered identity and composition adapters pass
their signature checks and fail their computation witnesses with kernel
mismatches.

report.json records input, checker, imported graph and fixture hashes.
The imported types.ndjson graph is reproducible from the frozen UAT export;
its manifest, summary and import logs are retained here. The graph itself
is not duplicated in Git. Signature witnesses, computation witnesses and
control diagnostics are retained. gate.stdout and gate.stderr record the
completed production run. gate.stderr names the run's evidence directory,
which is not retained.

Reproduce with make prelude-category-compatibility-test, or with:
python3 -P dev/prelude-category-compatibility.py --mech _bend2/bin/mech.exe \
  --export /path/to/uat.export --out /absolute/path/to/new-evidence
The export must match uat_export_sha256 in dev/denominators.json.

The report applies to MechSignatureCategory, whose nested object arguments
match the source export. Its outer Obj parameter stays erased. Conversion
to the erased MechCategoryCore record, recursor representation, whole-record
equality, source theorem bodies and runtime parity remain open. The mapping
and NEVER inventories are unchanged. No full M0 or performance acceptance
result is claimed. The prepared native checker and JavaScript test build
were reused because kernel and frontend sources did not change.
