"""Portable models, candidate receipts, and checked settlement artifacts."""

from __future__ import annotations

from pathlib import Path
import tempfile

from .certificate import kernel_check, mechanism_root, settlement_certificate, specification
from .model import Auction, canonical, fields, integer, load_auction, read_json, require, sha256
from .optimization import LinearModel, compile_model
from .settlement import _validate_with_exact, cases, exact_cases, reference_candidate, validate_candidate
from . import gpu


def write_new_json(path: Path, value: object) -> None:
    data = canonical(value).decode()
    with path.open("x", encoding="utf-8") as output:
        output.write(data)


def publish_directory(target: Path, files: dict[str, bytes]) -> None:
    require(not target.exists(), "output directory must be new")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mech-cuopt-publish-", dir=target.parent) as temporary:
        stage = Path(temporary) / "ready"
        stage.mkdir()
        for name, data in files.items():
            require(Path(name).name == name, "invalid artifact filename")
            (stage / name).write_bytes(data)
        require(not target.exists(), "output directory appeared during publication")
        stage.rename(target)


def export_auction(auction: Auction, directory: Path, root: Path | None = None) -> dict:
    require(not directory.exists(), "output directory must be new")
    root = mechanism_root(root)
    model = compile_model(auction)
    source = specification(auction, root)
    exports = tuple(f"offerValue_{i}" for i in range(len(auction.offers)))
    proof = kernel_check(source, root, exports)
    require([proof.exports[name] for name in exports] == [offer.value for offer in auction.offers],
            "checked mechanism bids disagree with the LP inputs")
    models = {name: model.lp(excluded).encode() for name, excluded in cases(model)}
    manifest = {"version": 1, "auction_sha256": auction.digest, "spec_sha256": proof.source_sha256,
                "models": {name: sha256(data) for name, data in models.items()},
                "kernel": {"source_axioms": [], "checker_sha256": proof.compiler_sha256}}
    publish_directory(directory, {"auction.json": canonical(auction.to_dict()), "mechanism.mech": source.encode(),
                                  "manifest.json": canonical(manifest),
                                  **{f"{name}.lp": data for name, data in models.items()}})
    return {"auction_sha256": auction.digest, "offers": len(auction.offers),
            "tenants": len(auction.tenants), "models": len(models), "kernel_checked": True}


def load_bundle(directory: Path) -> tuple[LinearModel, str]:
    manifest = fields(read_json(directory / "manifest.json"),
                      {"version", "auction_sha256", "spec_sha256", "models", "kernel"}, "manifest")
    integer(manifest["version"], "manifest.version", 1, 1)
    auction = load_auction(directory / "auction.json")
    require(auction.digest == manifest["auction_sha256"], "auction hash mismatch")
    model = compile_model(auction)
    expected_cases = {name for name, _ in cases(model)}
    hashes = fields(manifest["models"], expected_cases, "manifest.models")
    spec_path = directory / "mechanism.mech"
    require(spec_path.stat().st_size <= 32 * 1024 * 1024, "mechanism specification exceeds the size limit")
    source = spec_path.read_text(encoding="utf-8")
    require(sha256(source.encode()) == manifest["spec_sha256"], "mechanism source hash mismatch")
    for name, excluded in cases(model):
        path = directory / f"{name}.lp"
        require(path.stat().st_size <= 32 * 1024 * 1024, "LP exceeds the size limit")
        data = path.read_bytes()
        require(sha256(data) == hashes[name] and data == model.lp(excluded).encode(), f"{name} LP disagrees with the auction")
    return model, source


def verify_and_publish(directory: Path, candidate: object, output: Path, root: Path | None = None,
                       node_limit: int = 2_000_000, seconds: float = 30.0) -> dict:
    return _verify_and_publish(directory, candidate, output, root, node_limit, seconds, None)


def _verify_and_publish(directory: Path, candidate: object, output: Path, root: Path | None,
                        node_limit: int, seconds: float, known_exact) -> dict:
    require(not output.exists(), "settlement output directory must be new")
    model, source = load_bundle(directory)
    root = mechanism_root(root)
    require(source == specification(model.auction, root), "mechanism specification differs from the installed VCG contract")
    # The closed certificate includes the generic contract and submitted bids,
    # so the kernel checks the specification and the settlement in one pass.
    settlement, exact = (validate_candidate(model, candidate, node_limit, seconds) if known_exact is None
                         else _validate_with_exact(model, candidate, known_exact))
    without = {tenant: exact[f"without_{i}"] for i, tenant in enumerate(model.auction.tenants)}
    certificate, exports = settlement_certificate(model, exact["full"], without, root)
    proof = kernel_check(certificate, root, exports)
    expected = {"fullWelfare": settlement["welfare"], "totalRevenue": settlement["revenue"],
                **{f"payment_{i}": settlement["payments"][tenant] for i, tenant in enumerate(model.auction.tenants)}}
    require(proof.exports == expected, "kernel settlement disagrees with VCG accounting")
    settlement.update({"validated": True, "kernel_checked": True, "certificate_sha256": proof.source_sha256,
                       "candidate_sha256": sha256(canonical(candidate))})
    receipt = {"version": 1, "checker_sha256": proof.compiler_sha256, "source_axioms": [],
               "certificate_sha256": proof.source_sha256,
               "settlement_sha256": sha256(canonical(settlement)), "auction_sha256": model.auction.digest}
    publish_directory(output, {"settlement.json": canonical(settlement), "certificate.mech": certificate.encode(),
                               "candidate.json": canonical(candidate), "receipt.json": canonical(receipt)})
    return settlement


def solve_and_publish(directory: Path, output: Path, backend: str, root: Path | None = None,
                      node_limit: int = 2_000_000, seconds: float = 30.0) -> dict:
    require(not output.exists(), "settlement output directory must be new")
    model, _source = load_bundle(directory)
    if backend == "reference":
        exact = exact_cases(model, node_limit, seconds)
        candidate = reference_candidate(model, exact)
    else:
        require(backend == "cuopt", "unknown solver backend")
        candidate = gpu.propose(model, directory, seconds)
        exact = None
    return _verify_and_publish(directory, candidate, output, root, node_limit, seconds, exact)


def audit_settlement(directory: Path, settlement_directory: Path, root: Path | None = None,
                     node_limit: int = 2_000_000, seconds: float = 30.0) -> dict:
    """Recompute a portable settlement and bind its JSON to its checked proof."""
    receipt = fields(read_json(settlement_directory / "receipt.json"),
                     {"version", "checker_sha256", "source_axioms", "certificate_sha256",
                      "settlement_sha256", "auction_sha256"}, "receipt")
    integer(receipt["version"], "receipt.version", 1, 1)
    require(receipt["source_axioms"] == [], "receipt must not declare source axioms")
    settlement = read_json(settlement_directory / "settlement.json")
    candidate = read_json(settlement_directory / "candidate.json")
    certificate_path = settlement_directory / "certificate.mech"
    require(certificate_path.stat().st_size <= 32 * 1024 * 1024, "certificate exceeds the size limit")
    certificate = certificate_path.read_bytes()
    require(sha256(canonical(settlement)) == receipt["settlement_sha256"], "settlement receipt hash mismatch")
    require(sha256(certificate) == receipt["certificate_sha256"], "certificate receipt hash mismatch")
    with tempfile.TemporaryDirectory(prefix="mech-cuopt-audit-") as temporary:
        output = Path(temporary) / "rechecked"
        verified = verify_and_publish(directory, candidate, output, root, node_limit, seconds)
        require(settlement == verified, "stored settlement differs from independent verification")
        require(certificate == (output / "certificate.mech").read_bytes(), "stored certificate differs from verified settlement")
        require(receipt["auction_sha256"] == verified["auction_sha256"], "receipt belongs to another auction")
    return {"validated": True, "kernel_checked": True, "auction_sha256": verified["auction_sha256"],
            "certificate_sha256": verified["certificate_sha256"], "welfare": verified["welfare"],
            "payments": verified["payments"]}
