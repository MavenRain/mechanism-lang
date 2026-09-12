"""Integer allocation models and a bounded, exact branch-and-bound verifier."""

from __future__ import annotations

from dataclasses import dataclass
import time

from .model import Auction, AuctionError, integer, require, sha256


@dataclass(frozen=True)
class Constraint:
    name: str
    terms: tuple[tuple[int, int], ...]
    capacity: int


@dataclass(frozen=True)
class LinearModel:
    auction: Auction
    constraints: tuple[Constraint, ...]

    @property
    def variables(self) -> tuple[str, ...]:
        return tuple(f"x_{i}" for i in range(len(self.auction.offers))) or ("x_empty",)

    def feasible(self, bits: tuple[int, ...], excluded: str | None = None) -> bool:
        if len(bits) != len(self.auction.offers) or any(type(bit) is not int or bit not in (0, 1) for bit in bits):
            return False
        if any(bit and offer.tenant == excluded for bit, offer in zip(bits, self.auction.offers)):
            return False
        return all(sum(bits[index] * amount for index, amount in row.terms) <= row.capacity
                   for row in self.constraints)

    def welfare(self, bits: tuple[int, ...]) -> int:
        return sum(bit * offer.value for bit, offer in zip(bits, self.auction.offers))

    def lp(self, excluded: str | None = None) -> str:
        require(excluded is None or excluded in self.auction.tenants, "unknown excluded tenant")
        if not self.auction.offers:
            return "Maximize\n welfare: 0 x_empty\nSubject To\n empty: x_empty = 0\nBounds\n x_empty = 0\nBinary\n x_empty\nEnd\n"
        expression = lambda terms: " + ".join(f"{amount} x_{index}" for index, amount in terms)
        lines = ["\\ Integer welfare; equal optima use the documented verifier tie rule.",
                 "Maximize", " welfare: " + expression(tuple((i, offer.value) for i, offer in enumerate(self.auction.offers))),
                 "Subject To"]
        lines.extend(f" {row.name}: {expression(row.terms)} <= {row.capacity}" for row in self.constraints)
        lines.append("Bounds")
        lines.extend(f" x_{i} = 0" if offer.tenant == excluded else f" 0 <= x_{i} <= 1"
                     for i, offer in enumerate(self.auction.offers))
        lines.extend(["Binary", *(f" x_{i}" for i in range(len(self.auction.offers))), "End"])
        return "\n".join(lines) + "\n"

    def digest(self, excluded: str | None = None) -> str:
        return sha256(self.lp(excluded).encode())


def compile_model(auction: Auction) -> LinearModel:
    rows = []
    for tenant_index, tenant in enumerate(auction.tenants):
        terms = tuple((index, 1) for index, offer in enumerate(auction.offers) if offer.tenant == tenant)
        if terms:
            rows.append(Constraint(f"tenant_{tenant_index}", terms, 1))
    events = sorted({point for offer in auction.offers for point in (offer.bundle.start, offer.bundle.end)})
    # Occupancy changes only at offer boundaries; the horizon need not be expanded.
    seen = set()
    for segment, (start, end) in enumerate(zip(events, events[1:])):
        active = [(i, offer) for i, offer in enumerate(auction.offers)
                  if offer.bundle.start < end and start < offer.bundle.end]
        for part_index, part in enumerate(auction.slices):
            terms = tuple((i, 1) for i, offer in active if any(claim.id == part.id for claim in offer.bundle.slices))
            key = ("slice", part.id, terms)
            if terms and key not in seen:
                seen.add(key)
                rows.append(Constraint(f"slice_{part_index}_{segment}", terms, 1))
        for host_index, host in enumerate(auction.hosts):
            terms = tuple((i, dict(offer.bundle.cpu_cores).get(host.id, 0)) for i, offer in active
                          if dict(offer.bundle.cpu_cores).get(host.id, 0) > 0)
            key = ("cpu", host.id, terms)
            if terms and key not in seen:
                seen.add(key)
                rows.append(Constraint(f"cpu_{host_index}_{segment}", terms, host.cpu_cores))
    return LinearModel(auction, tuple(rows))


@dataclass(frozen=True)
class ExactResult:
    bits: tuple[int, ...]
    welfare: int
    nodes: int


class VerificationLimit(AuctionError):
    """No optimality claim is made when the exact verification budget expires."""


def solve_exact(model: LinearModel, excluded: str | None = None,
                node_limit: int = 2_000_000, seconds: float = 30.0) -> ExactResult:
    integer(node_limit, "node_limit", 1, 100_000_000)
    require(type(seconds) in (int, float) and 0 < seconds <= 3600, "seconds must be in (0, 3600]")
    require(excluded is None or excluded in model.auction.tenants, "unknown excluded tenant")
    offers = model.auction.offers
    uses = [[] for _ in offers]
    residual = [row.capacity for row in model.constraints]
    for row_index, row in enumerate(model.constraints):
        for index, amount in row.terms:
            uses[index].append((row_index, amount))
    # Zero-valued offers consume capacity and increase the tie mask. Omitting
    # them cannot worsen either welfare or the public tie order.
    order = sorted((i for i, offer in enumerate(offers) if offer.value > 0 and offer.tenant != excluded),
                   key=lambda i: (-offers[i].value, i))
    best_welfare, best_mask, nodes = 0, 0, 0
    deadline = time.monotonic() + seconds

    def fits(index):
        return all(amount <= residual[row] for row, amount in uses[index])

    def visit(position, welfare, mask):
        nonlocal best_welfare, best_mask, nodes
        nodes += 1
        if nodes > node_limit or (nodes % 256 == 1 and time.monotonic() > deadline):
            raise VerificationLimit("exact optimality verification exhausted its budget; no settlement authorized")
        if welfare > best_welfare or (welfare == best_welfare and mask < best_mask):
            best_welfare, best_mask = welfare, mask
        upper_by_tenant = {}
        for index in order[position:]:
            if fits(index):
                offer = offers[index]
                upper_by_tenant[offer.tenant] = max(upper_by_tenant.get(offer.tenant, 0), offer.value)
        upper = welfare + sum(upper_by_tenant.values())
        if upper < best_welfare or (upper == best_welfare and mask >= best_mask):
            return
        if position == len(order):
            return
        index = order[position]
        if fits(index):
            for row, amount in uses[index]:
                residual[row] -= amount
            visit(position + 1, welfare + offers[index].value, mask | (1 << index))
            for row, amount in uses[index]:
                residual[row] += amount
        visit(position + 1, welfare, mask)

    visit(0, 0, 0)
    bits = tuple((best_mask >> i) & 1 for i in range(len(offers)))
    require(model.feasible(bits, excluded), "internal exact solver returned an infeasible allocation")
    return ExactResult(bits, best_welfare, nodes)
