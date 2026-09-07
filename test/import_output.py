#!/usr/bin/env python3
"""Check a published type DAG against its source export with Python's JSON reader."""
from collections import Counter
import json
from pathlib import Path
import sys


def main():
    source, output = Path(sys.argv[1]), Path(sys.argv[2])
    failures = []

    def check(condition, message):
        if not condition:
            failures.append(message)
            print(f"IMPORT-OUTPUT FAIL {message}")
        return condition

    names = {}
    levels = {}
    expressions = {}
    declarations = []
    with source.open() as channel:
        for line in channel:
            row = json.loads(line)
            if "in" in row:
                names[row["in"]] = row
            elif "il" in row:
                levels[row["il"]] = row
            elif "ie" in row:
                expressions[row["ie"]] = row
            elif "inductive" in row:
                group = row["inductive"]
                declarations.extend(group["types"] + group["ctors"] + group["recs"])
            elif "meta" not in row:
                declarations.append(next(iter(row.values())))

    def children(row):
        result = []
        for tag, fields in {"app": ["fn", "arg"], "lam": ["type", "body"],
                            "forallE": ["type", "body"], "letE": ["type", "value", "body"],
                            "proj": ["struct"], "mdata": ["expr"]}.items():
            if tag in row:
                result.extend(row[tag][key] for key in fields)
        return result

    reachable = set()
    pending = [row["type"] for row in declarations]
    while pending:
        current = pending.pop()
        if current not in reachable:
            reachable.add(current)
            pending.extend(children(expressions[current]))

    with (output / "types.ndjson").open() as channel:
        records = [json.loads(line) for line in channel]
    saved_names = {row["name"]: row for row in records if "name" in row}
    saved_levels = {row["level"]: row for row in records if "level" in row}
    saved_exprs = {row["expr"]: row for row in records if "expr" in row}
    saved_decls = [row["declaration"] for row in records if "declaration" in row]
    check(len(records) == len(saved_names) + len(saved_levels) + len(saved_exprs) + len(saved_decls),
          "the published table holds a record of an unknown kind")
    check(set(saved_names) == set(names) | {0}, "the published name table is not the source table")
    check(all(saved_names[index][tag] == row[tag]
              for index, row in names.items() for tag in ["str", "num"] if tag in row),
          "a published name record differs from its source")
    check(set(saved_exprs) == reachable,
          "the published expressions are not the reachable source expressions")
    level_roots = []
    for index in reachable:
        raw = expressions[index]
        if "sort" in raw:
            level_roots.append(raw["sort"])
        if "const" in raw:
            level_roots.extend(raw["const"]["us"])
    level_reachable = set()
    while level_roots:
        index = level_roots.pop()
        if index not in level_reachable:
            level_reachable.add(index)
            if index != 0:
                raw = levels[index]
                if "succ" in raw:
                    level_roots.append(raw["succ"])
                level_roots.extend(raw.get("max", []) + raw.get("imax", []))
    check(set(saved_levels) == level_reachable,
          "the published levels are not the reachable source levels")

    def name_key(index):
        if index == 0:
            return ()
        row = names[index]
        if "str" in row:
            return name_key(row["str"]["pre"]) + (("str", row["str"]["str"]),)
        return name_key(row["num"]["pre"]) + (("num", row["num"]["i"]),)

    def level_ok(index, saved):
        if index == 0:
            return saved["zero"] is True
        raw = levels[index]
        return (all(saved[tag] == raw[tag] for tag in ["succ", "max", "imax"] if tag in raw)
                and ("param" not in raw
                     or name_key(saved["param"]["source_name"]) == name_key(raw["param"])))

    def const_ok(raw, saved):
        return ("const" not in raw
                or (saved["const"]["source_name"] == raw["const"]["name"]
                    and saved["const"]["universes"] == raw["const"]["us"]
                    and all(level in saved_levels for level in saved["const"]["universes"])))

    def proj_ok(raw, saved):
        return ("proj" not in raw
                or (saved["proj"]["source_type_name"] == raw["proj"]["typeName"]
                    and saved["proj"]["idx"] == raw["proj"]["idx"]))

    def expr_ok(index, saved):
        raw = expressions[index]
        return (children(raw) == children(saved)
                and all(saved[tag] == raw[tag]
                        for tag in ["bvar", "sort", "natVal", "strVal"] if tag in raw)
                and ("sort" not in saved or saved["sort"] in saved_levels)
                and const_ok(raw, saved)
                and all(saved[tag]["binderInfo"] == raw[tag]["binderInfo"]
                        for tag in ["lam", "forallE"] if tag in raw)
                and ("letE" not in raw or saved["letE"]["nondep"] == raw["letE"]["nondep"])
                and proj_ok(raw, saved)
                and ("mdata" not in raw or saved["mdata"]["data"] == raw["mdata"]["data"]))

    bad_levels = [index for index, saved in saved_levels.items() if not level_ok(index, saved)]
    check(bad_levels == [], f"published levels differ from their source at {bad_levels[:5]}")
    bad_exprs = [index for index, saved in saved_exprs.items() if not expr_ok(index, saved)]
    check(bad_exprs == [], f"published expressions differ from their source at {bad_exprs[:5]}")
    check([(row["source_name"], row["type"], row["source_level_params"]) for row in saved_decls] == [
        (row["name"], row["type"], row["levelParams"]) for row in declarations],
        "the published declaration order or parameters differ from the source")
    counts = Counter(row["status"] for row in saved_decls)
    if failures != []:
        print(f"IMPORT-OUTPUT FAIL failing_checks={len(failures)}")
        sys.exit(1)
    print(f"IMPORT-OUTPUT OK declarations={len(saved_decls)} type_nodes={len(saved_exprs)} statuses={dict(counts)}")
    sys.exit(0)


if __name__ == "__main__":
    main()
