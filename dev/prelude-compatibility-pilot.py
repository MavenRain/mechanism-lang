#!/usr/bin/env python3
"""Check a bounded set of UAT type signatures against checked prelude targets."""

import argparse
import collections
import csv
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
U1 = "CompCatTheory."
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z_0-9]*(?:\.[A-Za-z_][A-Za-z_0-9]*)*\Z")


class Blocked(Exception):
    def __init__(self, code, detail):
        super().__init__(detail)
        self.code = code


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_graph(path):
    tables = {"level": {}, "expr": {}, "declaration": {}}
    with Path(path).open() as stream:
        for line in stream:
            row = json.loads(line)
            if "declaration" in row:
                declaration = row["declaration"]
                key = declaration["name"]
                table = tables["declaration"]
                value = declaration
            elif "expr" in row or "level" in row:
                kind = "expr" if "expr" in row else "level"
                key = row[kind]
                table = tables[kind]
                value = {name: value for name, value in row.items() if name != kind}
            else:
                continue
            if key in table:
                raise Blocked("DUPLICATE_NODE", str(key))
            table[key] = value
    return tables


class Renderer:
    """Render the imported graph, preserving the importer's QMany binders."""

    def __init__(self, graph, mappings, parameters=(), aliases=None):
        self.graph = graph
        self.mappings = mappings
        self.parameters = {entry["name"]: f"u{index}" for index, entry in enumerate(parameters)}
        self.aliases = aliases or {}
        self.work = 0

    def step(self, depth):
        self.work += 1
        if depth > 80 or self.work > 20000:
            raise Blocked("RENDER_LIMIT", "source type exceeds the bounded rendering budget")

    def level(self, index, depth=0):
        self.step(depth)
        node = self.graph["level"].get(index)
        if node is None:
            raise Blocked("MISSING_LEVEL", str(index))
        if "zero" in node:
            return "0"
        if "param" in node:
            name = node["param"]["name"]
            if name not in self.parameters:
                raise Blocked("UNBOUND_UNIVERSE", name)
            return self.parameters[name]
        if "succ" in node:
            return f"(succ {self.level(node['succ'], depth + 1)})"
        for operation in ("max", "imax"):
            if operation in node:
                left, right = node[operation]
                return f"({operation} {self.level(left, depth + 1)} {self.level(right, depth + 1)})"
        raise Blocked("UNSUPPORTED_LEVEL", repr(node))

    def constant(self, node):
        name = node["name"]
        levels = tuple(self.level(index) for index in node["universes"])
        if (name, levels) in self.aliases:
            return self.aliases[(name, levels)]
        if name not in self.mappings:
            raise Blocked("UNMAPPED_DEPENDENCY", name)
        declaration = self.graph["declaration"].get(name)
        if declaration is None:
            raise Blocked("MISSING_DECLARATION", name)
        if declaration.get("parameters") or levels:
            raise Blocked("UNIVERSE_SPECIALIZATION_REQUIRED", name + repr(levels))
        return self.mappings[name]

    def expression(self, index, context=(), depth=0):
        self.step(depth)
        node = self.graph["expr"].get(index)
        if node is None:
            raise Blocked("MISSING_EXPRESSION", str(index))
        if "sort" in node:
            return f"(Sort {self.level(node['sort'])})"
        if "bvar" in node:
            bound = node["bvar"]
            if not isinstance(bound, int) or not 0 <= bound < len(context):
                raise Blocked("UNBOUND_VARIABLE", str(bound))
            return context[-1 - bound]
        if "const" in node:
            return self.constant(node["const"])
        if "app" in node:
            head = self.expression(node["app"]["fn"], context, depth + 1)
            argument = self.expression(node["app"]["arg"], context, depth + 1)
            return f"({head} {argument})"
        for kind in ("forallE", "lam"):
            if kind in node:
                binder = node[kind]
                name = f"b{len(context)}"
                domain = self.expression(binder["type"], context, depth + 1)
                body = self.expression(binder["body"], (*context, name), depth + 1)
                if kind == "forallE":
                    return f"(({name} : {domain}) -> {body})"
                return f"(fun ({name} : {domain}) => {body})"
        if "letE" in node:
            binder = node["letE"]
            name = f"b{len(context)}"
            domain = self.expression(binder["type"], context, depth + 1)
            value = self.expression(binder["value"], context, depth + 1)
            body = self.expression(binder["body"], (*context, name), depth + 1)
            return f"(let {name} : {domain} := {value}; {body})"
        raise Blocked("UNSUPPORTED_EXPRESSION", next(iter(node), "empty node"))

    def witness(self, index, target, omitted=0):
        binders = []
        context = ()
        while "forallE" in self.graph["expr"].get(index, {}):
            self.step(len(context))
            binder = self.graph["expr"][index]["forallE"]
            name = f"b{len(context)}"
            domain = self.expression(binder["type"], context)
            binders.append(f"({name} : {domain})")
            context = (*context, name)
            index = binder["body"]
        if omitted > len(context):
            raise Blocked("CONSTRUCTOR_ARITY", target)
        body = " ".join((target, *context[omitted:]))
        return "fun " + " ".join(binders) + " => " + body if binders else body


def binders_and_result(graph, index):
    count = 0
    while "forallE" in graph["expr"].get(index, {}):
        count += 1
        if count > 80:
            raise Blocked("RENDER_LIMIT", "constructor family arity")
        index = graph["expr"][index]["forallE"]["body"]
    return count, index


def family_parameters(graph, declaration):
    """Count the leading constructor binders that the result passes to its family in order.

    The family type also binds indices, so its arity can exceed the parameter count.
    """
    if declaration["kind"] != "constructor":
        return 0
    parent = graph["declaration"].get(declaration["name"].rsplit(".", 1)[0])
    if parent is None or parent["kind"] != "inductive":
        raise Blocked("CONSTRUCTOR_FAMILY", declaration["name"])
    arity, _ = binders_and_result(graph, parent["type"])
    binders, index = binders_and_result(graph, declaration["type"])
    arguments = []
    while "app" in graph["expr"].get(index, {}):
        if len(arguments) >= arity:
            raise Blocked("CONSTRUCTOR_FAMILY", declaration["name"])
        arguments.insert(0, graph["expr"][index]["app"]["arg"])
        index = graph["expr"][index]["app"]["fn"]
    count = 0
    while (count < min(binders, len(arguments))
           and graph["expr"].get(arguments[count], {}).get("bvar") == binders - 1 - count):
        count += 1
    return count


def invoke(mech, command, path, timeout):
    try:
        result = subprocess.run([str(mech), command, str(path)], capture_output=True,
                                text=True, timeout=timeout, cwd=ROOT)
    except subprocess.TimeoutExpired as error:
        raise Blocked("CHECK_TIMEOUT", command) from error
    return result


def mappings_from(path):
    with Path(path).open() as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    selected = [row for row in rows if row["verdict"] == "NAME_ONLY"]
    mappings = {}
    for row in selected:
        if row["lean_name"] in mappings or not IDENTIFIER.fullmatch(row["target"]):
            raise Blocked("INVALID_MAPPING", row["lean_name"])
        mappings[row["lean_name"]] = row["target"]
    return mappings


def check_signature(graph, mappings, name, target, prelude, directory, mech, timeout,
                    group=None, aliases=None):
    result = {"name": name, "target": target, "scope": "type-signature"}
    try:
        declaration = graph["declaration"].get(name)
        if declaration is None:
            raise Blocked("MISSING_DECLARATION", name)
        result["source_line"] = declaration["source_line"]
        result["universes"] = [entry["name"] for entry in declaration.get("parameters", [])]
        result["dependencies"] = declaration.get("dependencies", [])
        if "type" not in declaration:
            raise Blocked("SOURCE_TYPE_ERROR", declaration.get("reason", "no scoped type"))
        if declaration.get("parameters") and group is None:
            raise Blocked("SYMBOLIC_TARGET_REQUIRED", "candidate needs an explicit symbolic specialization")
        renderer = Renderer(graph, mappings, declaration.get("parameters", []), aliases)
        expected = renderer.expression(declaration["type"])
        body = renderer.witness(declaration["type"], target, family_parameters(graph, declaration))
        witness = f"def compatibilityWitness : {expected} := {body}\n"
        if group is not None:
            parameters = ", ".join(renderer.parameters.values())
            witness = f"poly ({parameters}) group CompatibilityPilot where\n{group}\n{witness}end\n"
        source = prelude + "\n" + witness
        if len(source.encode()) > 256000:
            raise Blocked("SOURCE_LIMIT", "generated source exceeds 256000 bytes")
        path = directory / (name.replace(".", "-") + ".mech")
        path.write_text(source)
        result["fixture"] = path.name
        result["fixture_sha256"] = digest(path)
        checked = invoke(mech, "check", path, timeout)
        result["checker_exit"] = checked.returncode
        (directory / (path.stem + ".stdout")).write_text(checked.stdout)
        (directory / (path.stem + ".stderr")).write_text(checked.stderr)
        if checked.returncode not in (0, 1):
            raise Blocked("CHECKER_FAILURE", str(checked.returncode))
        if checked.returncode:
            result.update(status="BLOCKED", code="KERNEL_REJECTED", detail=checked.stderr.strip() or checked.stdout.strip())
        else:
            result.update(status="KERNEL_TYPE_MATCH", code="CHECKED")
    except Blocked as error:
        result.update(status="BLOCKED", code=error.code, detail=str(error))
    return result


def discharge_dependencies(results):
    """A successful check using a candidate mapping must discharge that mapping."""
    accepted = set()
    pending = {row["name"]: row for row in results if row["status"] == "KERNEL_TYPE_MATCH"}
    changed = True
    while changed:
        changed = False
        for name, row in pending.items():
            if name not in accepted and set(row["dependencies"]) <= accepted:
                accepted.add(name)
                changed = True
    for name, row in pending.items():
        if name in accepted:
            row["status"] = "NAME_AND_TYPE"
        else:
            row.update(status="BLOCKED", code="UNPROVEN_DEPENDENCY",
                       detail=", ".join(sorted(set(row["dependencies"]) - accepted)))


def record_inputs(name):
    core = "prelude/cat/category-core.mech"
    functor = "prelude/cat/heterogeneous-functor.mech"
    aliases = {}
    if name == "Category":
        files = [core]
        schema, levels, member = "MechCategoryCore", ("u0", "u1"), "Category"
    elif name in {"Functor", "NatTrans"}:
        files = [core, functor]
        levels = ("u0", "u1", "u2", "u3")
        schema, member, prefix = "MechHeterogeneousFunctor", "Functor", "Candidate"
        if name == "NatTrans":
            files.append("prelude/cat/heterogeneous-nattrans.mech")
            schema, member, prefix = "MechHeterogeneousNatTrans", "NatTrans", "Candidate_Base"
            aliases[(U1 + "Functor", levels)] = prefix + "_Functor"
        aliases[(U1 + "Category", levels[:2])] = prefix + "_Source_Category"
        aliases[(U1 + "Category", levels[2:])] = prefix + "_Target_Category"
    else:
        files = [core, "prelude/cat/composable-functors.mech",
                 "prelude/cat/heterogeneous-whiskering.mech",
                 "prelude/cat/heterogeneous-left-kan.mech"]
        levels = tuple(f"u{index}" for index in range(6))
        schema, member = "MechHeterogeneousLeftKan", "LeftKanExtension"
        prefix = "Candidate_Base_Base"
        for offset, category in ((0, "Source"), (2, "Middle"), (4, "Target")):
            aliases[(U1 + "Category", levels[offset:offset + 2])] = prefix + "_" + category + "_Category"
        aliases[(U1 + "Functor", levels[:4])] = prefix + "_First_Functor"
        aliases[(U1 + "Functor", levels[2:])] = prefix + "_Second_Functor"
        aliases[(U1 + "Functor", levels[:2] + levels[4:])] = prefix + "_Composite_Functor"
    source = "\n".join((ROOT / path).read_text() for path in files)
    group = "  specialize " + schema + " (" + ", ".join(levels) + ") as Candidate"
    return source, group, aliases, "Candidate_" + member, files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export", type=Path, default=Path("/Users/oobi/Documents/kanon-m2-corpus/corpus/lean-parity/uat/uat.export"))
    parser.add_argument("--mech", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    if args.timeout < 1:
        parser.error("timeout must be positive")
    args.export, args.mech, args.out = args.export.resolve(), args.mech.resolve(), args.out.resolve()
    expected = json.loads((ROOT / "dev/denominators.json").read_text())["uat_export_sha256"]
    if digest(args.export) != expected:
        parser.error("export differs from the frozen UAT denominator")
    args.out.mkdir(parents=True, exist_ok=False)
    imported = args.out / "import"
    try:
        binaries = {str(args.mech): digest(args.mech),
                    str(args.mech.parent / "mechanism-native"): digest(args.mech.parent / "mechanism-native")}
        process = subprocess.run([str(args.mech), "import", str(args.export), "--out", str(imported)],
                                 capture_output=True, text=True, timeout=args.timeout, cwd=ROOT)
        (args.out / "import.stdout").write_text(process.stdout)
        (args.out / "import.stderr").write_text(process.stderr)
        if process.returncode:
            raise Blocked("IMPORT_FAILED", process.stderr.strip() or process.stdout.strip())
        if digest(args.export) != expected:
            raise Blocked("EXPORT_CHANGED", "export changed during import")
        graph = read_graph(imported / "types.ndjson")
        if len(graph["declaration"]) != 3202:
            raise Blocked("DENOMINATOR_DRIFT", "expected 3202 imported declarations")
        inputs = {"prelude/init.mech", "map/prelude.map.tsv", "dev/denominators.json",
                  "dev/prelude-compatibility-pilot.py", "dev/prelude-compatibility-gates.py",
                  "test/prelude_compatibility.py"}
        for name in ("Category", "Functor", "NatTrans", "LeftKanExtension"):
            inputs.update(record_inputs(name)[4])
        input_hashes = {path: digest(ROOT / path) for path in sorted(inputs)}
        mappings = mappings_from(ROOT / "map/prelude.map.tsv")
        if len(mappings) != 19:
            raise Blocked("PILOT_SCOPE_DRIFT", "expected the 19 existing candidates")
        prelude = (ROOT / "prelude/init.mech").read_text()
        baseline = args.out / "baseline.mech"
        baseline.write_text(prelude)
        if invoke(args.mech, "check", baseline, args.timeout).returncode:
            raise Blocked("PRELUDE_FAILED", "checked foundation failed")
        axioms = invoke(args.mech, "axioms", baseline, args.timeout)
        (args.out / "axioms.stdout").write_text(axioms.stdout)
        (args.out / "axioms.stderr").write_text(axioms.stderr)
        if axioms.returncode or axioms.stdout.strip():
            raise Blocked("AXIOM_AUDIT_FAILED", axioms.stdout.strip() or axioms.stderr.strip())
        results = [check_signature(graph, mappings, name, target, prelude, args.out,
                                   args.mech, args.timeout) for name, target in mappings.items()]
        discharge_dependencies(results)
        records = []
        for name in ("Category", "Functor", "NatTrans", "LeftKanExtension"):
            source, group, aliases, target, files = record_inputs(name)
            inputs.update(files)
            records.append(check_signature(graph, mappings, U1 + name, target, source,
                                           args.out, args.mech, args.timeout, group=group, aliases=aliases))
        discharge_dependencies(records)
        if any(digest(ROOT / path) != value for path, value in input_hashes.items()):
            raise Blocked("INPUT_CHANGED", "pilot inputs changed during checking")
        if any(digest(path) != value for path, value in binaries.items()):
            raise Blocked("CHECKER_CHANGED", "checker changed during the pilot")
        report = {"version": 1, "scope": "19 existing candidates and four U1 record type signatures",
                  "export_sha256": expected, "import_graph_sha256": digest(imported / "types.ndjson"),
                  "mech_sha256": digest(args.mech.resolve()), "map_sha256": digest(ROOT / "map/prelude.map.tsv"),
                  "prelude_sha256": digest(ROOT / "prelude/init.mech"),
                  "inputs": input_hashes,
                  "native_sha256": digest(args.mech.resolve().parent / "mechanism-native"),
                  "candidates": results, "u1_records": records,
                  "counts": dict(collections.Counter(row["status"] for row in results)),
                  "u1_counts": dict(collections.Counter(row["status"] for row in records))}
        (args.out / "report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
        print("PRELUDE-COMPATIBILITY-PILOT candidates=19 records=4 checked="
              + str(report["counts"].get("NAME_AND_TYPE", 0))
              + " blocked=" + str(report["counts"].get("BLOCKED", 0))
              + " u1_checked=" + str(report["u1_counts"].get("NAME_AND_TYPE", 0))
              + " u1_blocked=" + str(report["u1_counts"].get("BLOCKED", 0)) + " OK")
        return 0
    except (Blocked, subprocess.TimeoutExpired, OSError, ValueError, KeyError) as error:
        (args.out / "failure.json").write_text(json.dumps({"code": getattr(error, "code", "PILOT_ERROR"), "detail": str(error)}, indent=2) + "\n")
        print("PRELUDE-COMPATIBILITY-PILOT FAIL " + str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
