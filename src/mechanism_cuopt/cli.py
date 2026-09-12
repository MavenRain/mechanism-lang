"""Command line interface for combinatorial GPU auctions."""

import argparse
import json
from pathlib import Path
import sys

from . import gpu
from .artifacts import audit_settlement, export_auction, load_bundle, solve_and_publish, verify_and_publish, write_new_json
from .certificate import kernel_check, mechanism_root
from .model import AuctionError, integer, load_auction, read_json, require


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", action="version", version="mechanism-cuopt 0.1.0")
    commands = parser.add_subparsers(dest="command", required=True)
    export = commands.add_parser("export", help="check the mechanism and export winner/counterfactual LPs")
    export.add_argument("instance", type=Path)
    export.add_argument("--out", type=Path, required=True)
    export.add_argument("--mech-root", type=Path)
    for name in ("solve", "verify", "audit"):
        command = commands.add_parser(name)
        command.add_argument("artifact", type=Path)
        if name != "audit":
            command.add_argument("--out", type=Path, required=True)
        command.add_argument("--mech-root", type=Path)
        command.add_argument("--node-limit", type=int, default=2_000_000)
        command.add_argument("--seconds", type=float, default=30.0, help="time budget per optimization case")
        if name == "solve":
            command.add_argument("--backend", choices=("reference", "cuopt"), required=True)
        elif name == "verify":
            command.add_argument("candidate", type=Path)
        else:
            command.add_argument("settlement", type=Path)
    propose = commands.add_parser("propose", help="GPU worker: return an unverified cuOpt candidate")
    propose.add_argument("artifact", type=Path)
    propose.add_argument("--out", type=Path, required=True)
    propose.add_argument("--seconds", type=float, default=30.0)
    inspect = commands.add_parser("inspect", help="check artifact hashes and show model dimensions")
    inspect.add_argument("artifact", type=Path)
    check = commands.add_parser("check-certificate", help="recheck a standalone .mech certificate")
    check.add_argument("certificate", type=Path)
    check.add_argument("--mech-root", type=Path)
    return parser.parse_args(argv)


def main(argv=None):
    args = arguments(argv)
    try:
        if hasattr(args, "node_limit"):
            integer(args.node_limit, "node-limit", 1, 100_000_000)
        if hasattr(args, "seconds"):
            require(0 < args.seconds <= 3600, "seconds must be in (0, 3600]")
        if args.command == "export":
            result = export_auction(load_auction(args.instance), args.out.resolve(), args.mech_root)
        elif args.command == "solve":
            result = solve_and_publish(args.artifact.resolve(), args.out.resolve(), args.backend,
                                       args.mech_root, args.node_limit, args.seconds)
        elif args.command == "verify":
            result = verify_and_publish(args.artifact.resolve(), read_json(args.candidate), args.out.resolve(),
                                        args.mech_root, args.node_limit, args.seconds)
        elif args.command == "audit":
            result = audit_settlement(args.artifact.resolve(), args.settlement.resolve(), args.mech_root,
                                      args.node_limit, args.seconds)
        elif args.command == "propose":
            require(not args.out.exists(), "candidate output must be new")
            model, _source = load_bundle(args.artifact.resolve())
            candidate = gpu.propose(model, args.artifact.resolve(), args.seconds)
            write_new_json(args.out, candidate)
            result = {"candidate": str(args.out.resolve()), "validated": False}
        elif args.command == "inspect":
            model, _source = load_bundle(args.artifact.resolve())
            result = {"auction_id": model.auction.id, "auction_sha256": model.auction.digest,
                      "offers": len(model.auction.offers), "tenants": len(model.auction.tenants),
                      "constraints": len(model.constraints), "models": 1 + len(model.auction.tenants)}
        else:
            proof = kernel_check(args.certificate.read_text(), mechanism_root(args.mech_root))
            result = {"kernel_checked": True, "source_sha256": proof.source_sha256, "source_axioms": []}
        print(json.dumps(result, sort_keys=True, allow_nan=False))
        return 0
    except (AuctionError, OSError, ValueError, RuntimeError, RecursionError) as error:
        print(f"mech-cuopt: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
