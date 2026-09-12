"""Public resource menus, closed bid profiles, and strict input validation."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re


MAX_OFFERS = 128
MAX_TENANTS = 64
MAX_SLICES = 256
MAX_INPUT_BYTES = 4 * 1024 * 1024
MAX_WELFARE = (1 << 50) - 1
IDENTIFIER = re.compile(r"[A-Za-z0-9][A-Za-z0-9._:-]{0,63}\Z")


class AuctionError(ValueError):
    """A rejected input or an unverifiable auction outcome."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AuctionError(message)


def fields(value: object, names: set[str], where: str) -> dict:
    require(type(value) is dict, f"{where} must be an object")
    require(set(value) == names, f"{where} fields must be {sorted(names)}")
    return value


def integer(value: object, where: str, minimum: int = 0, maximum: int = MAX_WELFARE) -> int:
    require(type(value) is int and minimum <= value <= maximum,
            f"{where} must be an integer in {minimum}..{maximum}")
    return value


def identifier(value: object, where: str) -> str:
    require(type(value) is str and IDENTIFIER.fullmatch(value) is not None,
            f"{where} must be a 1..64 character ASCII identifier")
    return value


def sequence(value: object, where: str, maximum: int, minimum: int = 0) -> list:
    require(type(value) is list and minimum <= len(value) <= maximum,
            f"{where} must be a list of {minimum}..{maximum} entries")
    return value


def unique(items, where: str) -> None:
    identifiers = [item.id for item in items]
    require(len(identifiers) == len(set(identifiers)), f"duplicate {where} identifier")


def canonical(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n").encode()


def sha256(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def read_json(path: Path) -> object:
    require(path.stat().st_size <= MAX_INPUT_BYTES, "JSON file exceeds the size limit")

    def object_hook(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f"duplicate JSON key: {key}")
            result[key] = value
        return result

    def invalid_constant(token):
        raise AuctionError(f"non-finite JSON number: {token}")

    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=object_hook,
                      parse_constant=invalid_constant)


@dataclass(frozen=True)
class Host:
    id: str
    cpu_cores: int


@dataclass(frozen=True)
class Slice:
    id: str
    host: str
    profile: str
    hbm_mib: int


@dataclass(frozen=True)
class Claim:
    id: str
    hbm_mib: int


@dataclass(frozen=True)
class Bundle:
    start: int
    duration: int
    slices: tuple[Claim, ...]
    cpu_cores: tuple[tuple[str, int], ...]

    @property
    def end(self) -> int:
        return self.start + self.duration


@dataclass(frozen=True)
class Offer:
    id: str
    tenant: str
    value: int
    bundle: Bundle


@dataclass(frozen=True)
class Auction:
    id: str
    horizon: int
    tick_seconds: int
    hosts: tuple[Host, ...]
    slices: tuple[Slice, ...]
    tenants: tuple[str, ...]
    offers: tuple[Offer, ...]

    def to_dict(self) -> dict:
        return {
            "version": 1, "auction_id": self.id,
            "inventory": {
                "horizon": self.horizon, "tick_seconds": self.tick_seconds,
                "hosts": [{"id": h.id, "cpu_cores": h.cpu_cores} for h in self.hosts],
                "slices": [{"id": s.id, "host": s.host, "profile": s.profile,
                            "hbm_mib": s.hbm_mib} for s in self.slices],
            },
            "tenants": [{"id": tenant, "offers": [
                {"id": offer.id, "value": offer.value, "bundle": {
                    "start": offer.bundle.start, "duration": offer.bundle.duration,
                    "slices": [{"id": s.id, "hbm_mib": s.hbm_mib} for s in offer.bundle.slices],
                    "cpu_cores": dict(offer.bundle.cpu_cores),
                }} for offer in self.offers if offer.tenant == tenant
            ]} for tenant in self.tenants],
        }

    @property
    def digest(self) -> str:
        return sha256(canonical(self.to_dict()))


def parse_auction(value: object) -> Auction:
    data = fields(value, {"version", "auction_id", "inventory", "tenants"}, "auction")
    integer(data["version"], "version", 1, 1)
    auction_id = identifier(data["auction_id"], "auction_id")
    inventory = fields(data["inventory"], {"horizon", "tick_seconds", "hosts", "slices"}, "inventory")
    horizon = integer(inventory["horizon"], "horizon", 1, 1_000_000_000)
    tick_seconds = integer(inventory["tick_seconds"], "tick_seconds", 1, 86400)
    hosts = []
    for entry in sequence(inventory["hosts"], "hosts", 64, 1):
        entry = fields(entry, {"id", "cpu_cores"}, "host")
        hosts.append(Host(identifier(entry["id"], "host.id"), integer(entry["cpu_cores"], "host.cpu_cores", 1, 65536)))
    unique(hosts, "host")
    host_map = {host.id: host for host in hosts}
    slices = []
    for entry in sequence(inventory["slices"], "slices", MAX_SLICES, 1):
        entry = fields(entry, {"id", "host", "profile", "hbm_mib"}, "slice")
        host = identifier(entry["host"], "slice.host")
        require(host in host_map, "slice references an unknown host")
        slices.append(Slice(identifier(entry["id"], "slice.id"), host,
                            identifier(entry["profile"], "slice.profile"),
                            integer(entry["hbm_mib"], "slice.hbm_mib", 1, 1 << 30)))
    unique(slices, "slice")
    slice_map = {part.id: part for part in slices}
    tenants = []
    offers = []
    for entry in sequence(data["tenants"], "tenants", MAX_TENANTS, 1):
        entry = fields(entry, {"id", "offers"}, "tenant")
        tenant = identifier(entry["id"], "tenant.id")
        require(tenant not in tenants, "duplicate tenant identifier")
        tenants.append(tenant)
        for offer in sequence(entry["offers"], "tenant.offers", MAX_OFFERS):
            offer = fields(offer, {"id", "value", "bundle"}, "offer")
            offer_id = identifier(offer["id"], "offer.id")
            amount = integer(offer["value"], "offer.value")
            bundle = fields(offer["bundle"], {"start", "duration", "slices", "cpu_cores"}, "bundle")
            start = integer(bundle["start"], "bundle.start", 0, horizon - 1)
            duration = integer(bundle["duration"], "bundle.duration", 1, horizon)
            require(start + duration <= horizon, "bundle extends beyond the auction horizon")
            claims = []
            for claim in sequence(bundle["slices"], "bundle.slices", MAX_SLICES, 1):
                claim = fields(claim, {"id", "hbm_mib"}, "slice claim")
                name = identifier(claim["id"], "claim.id")
                require(name in slice_map, "bundle references an unknown slice")
                memory = integer(claim["hbm_mib"], "claim.hbm_mib", 1, slice_map[name].hbm_mib)
                claims.append(Claim(name, memory))
            unique(claims, "bundle slice")
            cpu = bundle["cpu_cores"]
            require(type(cpu) is dict and len(cpu) <= len(hosts), "bundle.cpu_cores must be a host map")
            requested_hosts = {slice_map[claim.id].host for claim in claims}
            require(set(cpu) == requested_hosts, "CPU requests must name exactly the hosts of the requested slices")
            cpu_pairs = []
            for host, cores in cpu.items():
                cpu_pairs.append((host, integer(cores, "bundle CPU cores", 0, host_map[host].cpu_cores)))
            offers.append(Offer(offer_id, tenant, amount,
                                Bundle(start, duration, tuple(sorted(claims, key=lambda c: c.id)), tuple(sorted(cpu_pairs)))))
    require(len(offers) <= MAX_OFFERS, f"auction has more than {MAX_OFFERS} offers")
    unique(offers, "offer")
    # This bounds objective sums to exact integers in the cuOpt float64 interface.
    require(sum(offer.value for offer in offers) <= MAX_WELFARE, "total offer value exceeds exact objective range")
    return Auction(auction_id, horizon, tick_seconds, tuple(sorted(hosts, key=lambda h: h.id)),
                   tuple(sorted(slices, key=lambda s: s.id)), tuple(sorted(tenants)), tuple(sorted(offers, key=lambda o: o.id)))


def load_auction(path: Path) -> Auction:
    return parse_auction(read_json(path))
