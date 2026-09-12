"""VCG counterfactuals, exact candidate validation, and deterministic settlement."""

from __future__ import annotations

import math

from .model import AuctionError, fields, integer, require
from .optimization import ExactResult, LinearModel, solve_exact


def cases(model: LinearModel) -> tuple[tuple[str, str | None], ...]:
    return (("full", None), *((f"without_{i}", tenant) for i, tenant in enumerate(model.auction.tenants)))


def exact_cases(model: LinearModel, node_limit: int = 2_000_000, seconds: float = 30.0) -> dict[str, ExactResult]:
    return {name: solve_exact(model, tenant, node_limit, seconds) for name, tenant in cases(model)}


def encoded_result(model: LinearModel, result: ExactResult, excluded: str | None) -> dict:
    return {"status": "Optimal", "lp_sha256": model.digest(excluded), "objective": result.welfare,
            "variables": dict(zip(model.variables, result.bits or (0,)))}


def reference_candidate(model: LinearModel, results: dict[str, ExactResult]) -> dict:
    return {"version": 1, "backend": "reference", "auction_sha256": model.auction.digest,
            "cases": {name: encoded_result(model, results[name], tenant) for name, tenant in cases(model)}}


def finite(value: object) -> bool:
    return type(value) is int or (type(value) is float and math.isfinite(value))


def candidate_bits(model: LinearModel, result: object, excluded: str | None) -> tuple[int, ...]:
    result = fields(result, {"status", "lp_sha256", "objective", "variables"}, "candidate case")
    require(result["status"] == "Optimal", "every full and counterfactual solve must report Optimal")
    require(result["lp_sha256"] == model.digest(excluded), "candidate LP hash mismatch")
    variables = fields(result["variables"], set(model.variables), "candidate variables")
    bits = []
    for name in model.variables:
        value = variables[name]
        require(finite(value) and -1e-6 <= value <= 1 + 1e-6, f"invalid variable {name}")
        bit = round(value)
        require(abs(bit - value) <= 1e-6, f"fractional variable {name}")
        bits.append(bit)
    if not model.auction.offers:
        require(bits == [0], "the empty-auction variable must be zero")
        bits = []
    result_bits = tuple(bits)
    require(model.feasible(result_bits, excluded), "candidate violates inventory, interval, CPU, XOR, or exclusion constraints")
    require(finite(result["objective"]) and abs(result["objective"] - model.welfare(result_bits)) <= 1e-6,
            "candidate objective disagrees with exact integer welfare")
    return result_bits


def validate_candidate(model: LinearModel, candidate: object, node_limit: int = 2_000_000,
                       seconds: float = 30.0) -> tuple[dict, dict[str, ExactResult]]:
    # Reject malformed or infeasible submissions before spending the exact
    # search budget. The reference path also uses the same admission checks.
    _submitted_allocations(model, candidate)
    return _validate_with_exact(model, candidate, exact_cases(model, node_limit, seconds))


def _submitted_allocations(model: LinearModel, candidate: object) -> tuple[dict, dict[str, tuple[int, ...]]]:
    candidate = fields(candidate, {"version", "backend", "auction_sha256", "cases"}, "candidate")
    integer(candidate["version"], "candidate.version", 1, 1)
    require(candidate["backend"] in ("reference", "cuopt"), "unknown candidate backend")
    require(candidate["auction_sha256"] == model.auction.digest, "candidate belongs to another auction")
    candidate_cases = fields(candidate["cases"], {name for name, _ in cases(model)}, "candidate.cases")
    submitted = {name: candidate_bits(model, candidate_cases[name], tenant) for name, tenant in cases(model)}
    return candidate, submitted


def _validate_with_exact(model: LinearModel, candidate: object,
                         exact: dict[str, ExactResult]) -> tuple[dict, dict[str, ExactResult]]:
    candidate, submitted = _submitted_allocations(model, candidate)
    for name, _ in cases(model):
        require(model.welfare(submitted[name]) == exact[name].welfare,
                f"{name} is not an exact welfare optimum; no settlement authorized")
    full = exact["full"]
    payments = {}
    tenant_details = {}
    for i, tenant in enumerate(model.auction.tenants):
        selected = [offer for bit, offer in zip(full.bits, model.auction.offers) if bit and offer.tenant == tenant]
        own = sum(offer.value for offer in selected)
        others = full.welfare - own
        pivot = exact[f"without_{i}"].welfare
        payment = pivot - others
        require(0 <= payment <= own, "VCG payment violates non-deficit or individual rationality")
        payments[tenant] = payment
        tenant_details[tenant] = {"selected_offer": selected[0].id if selected else None, "reported_value": own,
                                  "others_welfare": others, "without_welfare": pivot,
                                  "payment": payment, "utility_if_truthful": own - payment}
    selected_ids = [offer.id for bit, offer in zip(full.bits, model.auction.offers) if bit]
    solver_ids = [offer.id for bit, offer in zip(submitted["full"], model.auction.offers) if bit]
    return {"version": 1, "auction_id": model.auction.id, "auction_sha256": model.auction.digest,
            "backend": candidate["backend"], "selected_offers": selected_ids,
            "solver_selected_offers": solver_ids, "tie_canonicalized": selected_ids != solver_ids,
            "welfare": full.welfare, "payments": payments, "revenue": sum(payments.values()),
            "tenants": tenant_details, "exact_verification_nodes": {name: result.nodes for name, result in exact.items()}}, exact
