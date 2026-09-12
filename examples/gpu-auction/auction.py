#!/usr/bin/env python3
"""Checked mechanism-lang input, cuOpt LP export, and exact result validation."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import importlib
import itertools
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
EXPORTS = (
    "gpuValueA", "gpuValueB", "gpuValueC", "gpuSlices",
    "gpuXA", "gpuXB", "gpuXC", "gpuPA", "gpuPB", "gpuPC",
    "gpuAllocated", "gpuTransportedSlices",
)
VARIABLES = ("x_A", "x_B", "x_C")
MAX_BID = 31  # Unary structural naturals bound the demo's evaluation cost.
TOLERANCE = 1e-6


class AuctionError(ValueError):
    """A rejected source, artifact, or solver result."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AuctionError(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n")


def invoke(arguments: list[str | Path], cwd: Path = ROOT) -> str:
    completed = subprocess.run(
        [str(arg) for arg in arguments], cwd=cwd,
        capture_output=True, text=True, timeout=60, check=False,
    )
    require(completed.returncode == 0,
            f"{Path(arguments[0]).name} exited {completed.returncode}: "
            f"{completed.stderr[-2000:] or completed.stdout[-2000:]}")
    return completed.stdout


@dataclass(frozen=True)
class CheckedAuction:
    bids: tuple[int, int, int]
    allocation: tuple[int, int, int]
    payments: tuple[int, int, int]
    slices: int = 8

    @property
    def coefficients(self) -> tuple[int, ...]:
        # With one winner and integer bids, a credit outweighs all priorities.
        return tuple(3 * bid + 2 - index for index, bid in enumerate(self.bids))

    @property
    def winner(self) -> int:
        return self.allocation.index(1)


def check_values(values: dict[str, int]) -> CheckedAuction:
    require(set(values) == set(EXPORTS), "missing or unexpected mechanism exports")
    require(all(type(value) is int for value in values.values()), "exports must be integers")
    bids = tuple(values[f"gpuValue{label}"] for label in "ABC")
    require(all(0 <= bid <= MAX_BID for bid in bids), f"bids must be in 0..{MAX_BID}")
    allocation = tuple(values[f"gpuX{label}"] for label in "ABC")
    payments = tuple(values[f"gpuP{label}"] for label in "ABC")
    require(values["gpuSlices"] == 8, "this milestone requires exactly eight slices")
    winner = max(range(3), key=lambda index: (bids[index], -index))
    price = max(bids[index] for index in range(3) if index != winner)
    require(allocation == tuple(int(index == winner) for index in range(3)),
            "allocation disagrees with the exact second-price rule or tie priority")
    require(payments == tuple(price if index == winner else 0 for index in range(3)),
            "payments disagree with the second-price rule")
    require(values["gpuAllocated"] == values["gpuTransportedSlices"] == 8,
            "allocation or equality transport changed the inventory count")
    return CheckedAuction(bids, allocation, payments)


def checked_source(source: Path, wasm_directory: Path, hosts: bool = False) -> CheckedAuction:
    helper = ROOT / "_build/default/test/prelude_runtime.exe"
    require(helper.is_file(), "build first: zsh dev/dunecho.sh build")
    wasm_directory.mkdir(parents=True, exist_ok=True)
    # This existing helper checks once, rejects source axioms, then evaluates
    # and emits every named export. The native Nat boundary is inherited.
    output = invoke([helper, source, wasm_directory, *EXPORTS])
    values: dict[str, int] = {}
    for row in output.splitlines():
        fields = row.split("\t")
        require(len(fields) == 2 and fields[0] not in values, "invalid kernel export record")
        values[fields[0]] = int(fields[1])
    model = check_values(values)
    if hosts:
        for name in EXPORTS:
            result = invoke(["node", ROOT / "dev/run-node.mjs", wasm_directory / f"{name}.wasm", name])
            require(result.strip() == str(values[name]), f"Wasm disagrees with kernel: {name}")
    return model


def source_parts(program: Path) -> list[Path]:
    return [ROOT / "prelude/init.mech", ROOT / "prelude/mechanism/second-price.mech", program]


def assemble(program: Path) -> str:
    return "\n".join(path.read_text() for path in source_parts(program))


def lp_text(model: CheckedAuction) -> str:
    objective = " + ".join(f"{coefficient} {variable}"
                           for coefficient, variable in zip(model.coefficients, VARIABLES))
    return (
        "\\ One bundle of eight slices. Priority A, B, C. Credits are integers.\n"
        "\\ Ranked objective = 3 * reported welfare + fixed winner priority.\n"
        f"Maximize\n ranked_welfare: {objective}\n"
        "Subject To\n one_bundle: x_A + x_B + x_C = 1\n"
        " mig_capacity: 8 x_A + 8 x_B + 8 x_C <= 8\n"
        "Bounds\n 0 <= x_A <= 1\n 0 <= x_B <= 1\n 0 <= x_C <= 1\n"
        "Binary\n x_A x_B x_C\nEnd\n"
    )


def export(program: Path, directory: Path, hosts: bool = False) -> CheckedAuction:
    require(not directory.exists(), "output directory must be new")
    source = assemble(program)
    # Check before publishing the artifact directory, so failures leave no
    # apparently ready model behind. TemporaryDirectory cleans failed checks.
    directory.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gpu-auction-", dir=directory.parent) as temporary:
        staging = Path(temporary) / "artifact"
        staging.mkdir()
        path = staging / "auction.mech"
        path.write_text(source)
        model = checked_source(path, staging / "wasm", hosts)
        lp = lp_text(model)
        (staging / "allocation.lp").write_text(lp)
        manifest = {
            "version": 1,
            "auction": asdict(model),
            "objective_coefficients": model.coefficients,
            "source_sha256": digest(path.read_bytes()),
            "lp_sha256": digest(lp.encode()),
            "compiler_sha256": digest((ROOT / "_build/default/test/prelude_runtime.exe").read_bytes()),
            "source_parts": {str(part.relative_to(ROOT)) if part.is_relative_to(ROOT) else str(part):
                             digest(part.read_bytes()) for part in source_parts(program)},
            "node_checked": hosts,
            "source_axioms": [],
        }
        write_json(staging / "manifest.json", manifest)
        staging.rename(directory)
    return model


def load_artifact(directory: Path) -> CheckedAuction:
    manifest = json.loads((directory / "manifest.json").read_text())
    require(type(manifest) is dict and manifest.get("version") == 1, "unsupported manifest")
    source = directory / "auction.mech"
    lp = (directory / "allocation.lp").read_bytes()
    require(digest(source.read_bytes()) == manifest.get("source_sha256"), "source hash mismatch")
    require(digest(lp) == manifest.get("lp_sha256"), "LP hash mismatch")
    with tempfile.TemporaryDirectory(prefix="gpu-auction-verify-") as temporary:
        model = checked_source(source, Path(temporary))
    require(lp == lp_text(model).encode(), "LP disagrees with checked mechanism inputs")
    expected = json.loads(json.dumps(asdict(model)))
    require(manifest.get("auction") == expected, "manifest disagrees with checked mechanism")
    require(manifest.get("objective_coefficients") == list(model.coefficients),
            "manifest objective disagrees with checked mechanism")
    return model


def reference_solve(model: CheckedAuction) -> dict:
    # Enumerate the binary feasible set, independently of the source winner.
    feasible = [bits for bits in itertools.product((0, 1), repeat=3)
                if sum(bits) == 1 and 8 * sum(bits) <= model.slices]
    objective = lambda bits: sum(c * x for c, x in zip(model.coefficients, bits))
    winner = max(feasible, key=objective)
    return {"version": 1, "backend": "reference", "status": "Optimal",
            "variables": dict(zip(VARIABLES, winner)), "objective": objective(winner),
            "lp_sha256": digest(lp_text(model).encode())}


def cuopt_solve(directory: Path) -> dict:
    try:
        problem_api = importlib.import_module("cuopt.linear_programming.problem")
        settings_api = importlib.import_module("cuopt.linear_programming.solver_settings")
    except ImportError as error:
        raise AuctionError("cuOpt is unavailable. Run the cuopt backend on a supported NVIDIA GPU host; "
                           "use --backend reference for local validation.") from error
    require(callable(getattr(problem_api.Problem, "read", None)),
            "this adapter targets cuOpt 26.08 with Problem.read for LP files")
    lp_sha256 = digest((directory / "allocation.lp").read_bytes())
    problem = problem_api.Problem.read(str(directory / "allocation.lp"))
    settings = settings_api.SolverSettings()
    settings.set_parameter("mip_relative_gap", 0.0)
    settings.set_parameter("mip_absolute_gap", 0.0)
    settings.set_parameter("time_limit", 30.0)
    problem.solve(settings)
    status = problem.Status.name
    require(status == "Optimal", f"cuOpt returned {status}; an optimal solution is required")
    variables = {variable.VariableName: float(variable.Value) for variable in problem.getVariables()}
    return {"version": 1, "backend": "cuopt", "status": status,
            "variables": variables, "objective": float(problem.ObjValue), "lp_sha256": lp_sha256}


def finite_number(value: object) -> bool:
    return type(value) is int or (type(value) is float and math.isfinite(value))


def validate_solution(model: CheckedAuction, solution: dict) -> dict:
    require(type(solution) is dict and solution.get("version") == 1, "unsupported solution")
    require(solution.get("status") == "Optimal", "solver must report Optimal")
    require(solution.get("backend") in ("reference", "cuopt"), "unknown solver backend")
    require(solution.get("lp_sha256") == digest(lp_text(model).encode()),
            "solution is bound to a different LP")
    variables = solution.get("variables")
    require(type(variables) is dict and set(variables) == set(VARIABLES),
            "solution must contain exactly x_A, x_B, x_C")
    bits = []
    for name in VARIABLES:
        value = variables[name]
        require(finite_number(value), f"non-finite or non-numeric variable: {name}")
        require(-TOLERANCE <= value <= 1 + TOLERANCE, f"variable out of bounds: {name}")
        bit = round(value)
        require(abs(value - bit) <= TOLERANCE, f"fractional allocation: {name}")
        bits.append(bit)
    require(sum(bits) == 1 and model.slices * sum(bits) <= 8, "infeasible bundle allocation")
    # This exact check covers optimality and fixed tie priority. It rejects
    # feasible incumbents and any falsely labelled optimal solver response.
    require(tuple(bits) == model.allocation, "solver winner disagrees with checked mechanism")
    objective = sum(coefficient * bit for coefficient, bit in zip(model.coefficients, bits))
    require(finite_number(solution.get("objective")), "invalid solver objective")
    require(abs(solution["objective"] - objective) <= TOLERANCE, "incorrect solver objective")
    winner = model.winner
    return {"version": 1, "backend": solution["backend"], "validated": True,
            "winner": "ABC"[winner], "slices": model.slices,
            "allocation": dict(zip("ABC", (model.slices * bit for bit in bits))),
            "payments": dict(zip("ABC", model.payments)),
            "reported_welfare": model.bids[winner], "ranked_objective": objective}


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    export_parser = commands.add_parser("export", help="check a .mech auction and emit a cuOpt LP")
    export_parser.add_argument("--program", type=Path, default=HERE / "three-bidders.mech")
    export_parser.add_argument("--out", type=Path, required=True)
    export_parser.add_argument("--check-wasm", action="store_true")
    solve_parser = commands.add_parser("solve", help="solve and verify a checked model")
    solve_parser.add_argument("artifact", type=Path)
    solve_parser.add_argument("--backend", choices=("reference", "cuopt"), required=True)
    propose_parser = commands.add_parser("propose", help="GPU worker: produce an unverified cuOpt candidate")
    propose_parser.add_argument("artifact", type=Path)
    propose_parser.add_argument("--out", type=Path, required=True)
    verify_parser = commands.add_parser("verify", help="reject invalid external solver results")
    verify_parser.add_argument("artifact", type=Path)
    verify_parser.add_argument("solution", type=Path)
    args = parser.parse_args(arguments)
    try:
        if args.command == "export":
            model = export(args.program.resolve(), args.out.resolve(), args.check_wasm)
            print(json.dumps({"artifact": str(args.out.resolve()), "bids": model.bids,
                              "kernel_checked": True, "node_checked": args.check_wasm}))
        elif args.command == "propose":
            require(not args.out.exists(), "candidate output path must be new")
            solution = cuopt_solve(args.artifact.resolve())
            write_json(args.out, solution)
            print(json.dumps({"candidate": str(args.out.resolve()), "validated": False}))
        else:
            directory = args.artifact.resolve()
            model = load_artifact(directory)
            if args.command == "solve":
                solution = reference_solve(model) if args.backend == "reference" else cuopt_solve(directory)
            else:
                solution = json.loads(args.solution.read_text())
            settlement = validate_solution(model, solution)
            if args.command == "solve":
                write_json(directory / f"{args.backend}-solution.json", solution)
                write_json(directory / f"{args.backend}-settlement.json", settlement)
            print(json.dumps(settlement, sort_keys=True))
    except (AuctionError, OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"gpu-auction: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
