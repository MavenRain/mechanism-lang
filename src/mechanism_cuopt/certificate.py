"""Mechanism-lang specifications and closed allocation/payment certificates."""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
import tempfile

from .model import Auction, AuctionError, require, sha256
from .optimization import ExactResult, LinearModel


SOURCES = ("prelude/init.mech", "prelude/mechanism/second-price.mech", "prelude/mechanism/vcg.mech")


def mechanism_root(explicit: Path | None = None) -> Path:
    if explicit is not None:
        candidates = [explicit.resolve()]
    elif os.environ.get("MECH_ROOT"):
        candidates = [Path(os.environ["MECH_ROOT"]).resolve()]
    else:
        candidates = [Path.cwd(), *Path.cwd().parents, *Path(__file__).resolve().parents]
    for root in candidates:
        if all((root / source).is_file() for source in SOURCES):
            return root
    raise AuctionError("mechanism-lang sources not found; set MECH_ROOT or pass --mech-root")


def sum_expression(terms: list[str]) -> str:
    result = "0"
    for term in reversed(terms):
        result = f"(natAdd {term} {result})"
    return result


def equality(name: str, expression: str, expected: int) -> str:
    return f"def {name} : MechEq Nat {expression} {expected} := mechRefl Nat {expected}"


def specification(auction: Auction, root: Path) -> str:
    declarations = ["\n-- Checked public menu and submitted integer values."]
    declarations.append(f"def auctionHorizon : Nat := {auction.horizon}")
    slice_map = {part.id: part for part in auction.slices}
    host_map = {host.id: host for host in auction.hosts}
    for i, offer in enumerate(auction.offers):
        declarations.append(f"def offerValue_{i} : Nat := {offer.value}")
        declarations.append(equality(f"offerInterval_{i}", f"(natSub {offer.bundle.end} auctionHorizon)", 0))
        for j, claim in enumerate(offer.bundle.slices):
            declarations.append(equality(f"offerHbm_{i}_{j}", f"(natSub {claim.hbm_mib} {slice_map[claim.id].hbm_mib})", 0))
        for j, (host, cores) in enumerate(offer.bundle.cpu_cores):
            declarations.append(equality(f"offerCpu_{i}_{j}", f"(natSub {cores} {host_map[host].cpu_cores})", 0))
    return "\n".join([*((root / source).read_text() for source in SOURCES), *declarations]) + "\n"


@dataclass(frozen=True)
class KernelCheck:
    source_sha256: str
    compiler_sha256: str
    exports: dict[str, int]


def kernel_check(source: str, root: Path, exports: tuple[str, ...] = ()) -> KernelCheck:
    require(len(source.encode()) <= 32 * 1024 * 1024, "mechanism certificate exceeds the 32 MiB limit")
    compiler = root / "_build/default/bin/mech_cert.exe"
    require(compiler.is_file(), "build the certificate checker first: zsh dev/dunecho.sh build")
    with tempfile.TemporaryDirectory(prefix="mech-cuopt-cert-") as temporary:
        path = Path(temporary) / "certificate.mech"
        path.write_text(source, encoding="utf-8")
        try:
            completed = subprocess.run([str(compiler), str(path), *exports], capture_output=True,
                                       text=True, timeout=120, check=False)
        except subprocess.TimeoutExpired as error:
            raise AuctionError("mechanism certificate checking timed out; no settlement authorized") from error
    require(completed.returncode == 0, f"mechanism certificate rejected: {completed.stderr[-2000:]}")
    rows = completed.stdout.splitlines()
    require(bool(rows) and rows[0] == "CHECKED axioms=0", "invalid certificate checker response")
    values = {}
    for row in rows[1:]:
        fields = row.split("\t")
        require(len(fields) == 2 and fields[0] in exports and fields[0] not in values,
                "unexpected certificate export")
        require(fields[1].isascii() and fields[1].isdigit(), "certificate exported a non-natural value")
        values[fields[0]] = int(fields[1])
    require(set(values) == set(exports), "missing certificate export")
    return KernelCheck(sha256(source.encode()), sha256(compiler.read_bytes()), values)


def allocation_declarations(model: LinearModel, result: ExactResult, prefix: str,
                            excluded: str | None) -> list[str]:
    declarations = []
    for i, bit in enumerate(result.bits):
        constructor = "auctionWin" if bit else "auctionLose"
        declarations.append(f"def {prefix}Choice_{i} : AuctionChoice := {constructor}")
        declarations.append(f"def {prefix}X_{i} : Nat := auctionBit {prefix}Choice_{i}")
        if model.auction.offers[i].tenant == excluded:
            declarations.append(equality(f"{prefix}Absent_{i}", f"{prefix}X_{i}", 0))
    welfare = sum_expression([f"(natMul offerValue_{i} {prefix}X_{i})" for i in range(len(result.bits))])
    declarations.append(f"def {prefix}Welfare : Nat := {welfare}")
    declarations.append(equality(f"{prefix}WelfareChecked", f"{prefix}Welfare", result.welfare))
    for row_index, row in enumerate(model.constraints):
        consumed = sum_expression([f"(natMul {amount} {prefix}X_{i})" for i, amount in row.terms])
        declarations.append(equality(f"{prefix}Capacity_{row_index}", f"(natSub {consumed} {row.capacity})", 0))
    return declarations


def settlement_certificate(model: LinearModel, full: ExactResult,
                           without: dict[str, ExactResult], root: Path) -> tuple[str, tuple[str, ...]]:
    declarations = allocation_declarations(model, full, "full", None)
    exports = ["fullWelfare"]
    for tenant_index, tenant in enumerate(model.auction.tenants):
        prefix = f"without_{tenant_index}"
        declarations.extend(allocation_declarations(model, without[tenant], prefix, tenant))
        own = sum_expression([f"(natMul offerValue_{i} fullX_{i})"
                              for i, offer in enumerate(model.auction.offers) if offer.tenant == tenant])
        declarations.extend([
            f"def own_{tenant_index} : Nat := {own}",
            f"def others_{tenant_index} : Nat := natSub fullWelfare own_{tenant_index}",
            f"def payment_{tenant_index} : Nat := vcgNativePayment fullWelfare own_{tenant_index} {prefix}Welfare",
            equality(f"decomposition_{tenant_index}", f"(natAdd own_{tenant_index} others_{tenant_index})", full.welfare),
            equality(f"pivotIdentity_{tenant_index}", f"(natAdd payment_{tenant_index} others_{tenant_index})", without[tenant].welfare),
            equality(f"pivotBound_{tenant_index}", f"(natSub {prefix}Welfare fullWelfare)", 0),
            equality(f"individualRational_{tenant_index}", f"(natSub payment_{tenant_index} own_{tenant_index})", 0),
        ])
        exports.append(f"payment_{tenant_index}")
    declarations.append("def totalRevenue : Nat := " + sum_expression([f"payment_{i}" for i in range(len(model.auction.tenants))]))
    exports.append("totalRevenue")
    return specification(model.auction, root) + "\n".join(declarations) + "\n", tuple(exports)
