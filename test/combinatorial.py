"""Independent allocation oracle, VCG incentives, and hostile-input regressions."""

from copy import deepcopy
import itertools
import json
from pathlib import Path
import random
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from mechanism_cuopt import gpu
from mechanism_cuopt.model import AuctionError, MAX_WELFARE, load_auction, parse_auction, read_json
from mechanism_cuopt.optimization import VerificationLimit, compile_model, solve_exact
from mechanism_cuopt.settlement import exact_cases, reference_candidate, validate_candidate


def offer(name, value, slices=(0,), start=0, duration=1, cpu=1):
    return {"id": name, "value": value, "bundle": {
        "start": start, "duration": duration,
        "slices": [{"id": f"m{i}", "hbm_mib": 1} for i in slices], "cpu_cores": {"h": cpu}}}


def profile(menus, *, capacity=4, horizon=3):
    return {"version": 1, "auction_id": "test", "inventory": {
        "horizon": horizon, "tick_seconds": 60, "hosts": [{"id": "h", "cpu_cores": capacity}],
        "slices": [{"id": f"m{i}", "host": "h", "profile": "1g.10gb", "hbm_mib": 10240}
                   for i in range(4)]},
        "tenants": [{"id": name, "offers": menu} for name, menu in menus.items()]}


def oracle_feasible(auction, bits, excluded=None):
    """Read resources directly, expanding small test horizons without LP rows."""
    selected = [o for o, bit in zip(auction.offers, bits) if bit]
    if any(o.tenant == excluded for o in selected):
        return False
    if len({o.tenant for o in selected}) != len(selected):
        return False
    for tick in range(auction.horizon):
        occupied = set()
        cpu = {h.id: 0 for h in auction.hosts}
        for item in selected:
            if tick not in range(item.bundle.start, item.bundle.end):
                continue
            for claim in item.bundle.slices:
                if claim.id in occupied:
                    return False
                occupied.add(claim.id)
            for host, cores in item.bundle.cpu_cores:
                cpu[host] += cores
        if any(cpu[h.id] > h.cpu_cores for h in auction.hosts):
            return False
    return True


def oracle(auction, excluded=None):
    best = (0, 0, (0,) * len(auction.offers))
    for mask in range(1 << len(auction.offers)):
        bits = tuple((mask >> i) & 1 for i in range(len(auction.offers)))
        if oracle_feasible(auction, bits, excluded):
            welfare = sum(o.value for o, bit in zip(auction.offers, bits) if bit)
            if welfare > best[0]:
                best = welfare, mask, bits
    return best[0], best[2]


def settled(data):
    model = compile_model(parse_auction(data))
    candidate = reference_candidate(model, exact_cases(model))
    return validate_candidate(model, candidate)[0]


class ResourceTests(unittest.TestCase):
    def test_documented_hand_cases(self):
        for path in sorted((ROOT / "examples/combinatorial").glob("*.expected.json")):
            data = read_json(path.with_name(path.name.replace(".expected.json", ".json")))
            result = settled(data)
            expected = read_json(path)
            self.assertEqual({key: result[key] for key in expected}, expected, path.name)

    def test_hand_combinatorial(self):
        model = compile_model(load_auction(ROOT / "examples/combinatorial/three-bidders.json"))
        exact = exact_cases(model)
        result, _ = validate_candidate(model, reference_candidate(model, exact))
        self.assertEqual([exact[name].welfare for name in exact], [16, 16, 12, 12])
        self.assertEqual(result["selected_offers"], ["B-first", "C-last"])
        self.assertEqual(result["payments"], {"A": 0, "B": 5, "C": 3})
        self.assertEqual(result["revenue"], 8)
        self.assertEqual(model.lp(), (ROOT / "examples/combinatorial/three-bidders.lp").read_text())

    def test_second_price_is_special_case(self):
        result = settled(profile({"A": [offer("a", 12)], "B": [offer("b", 9)], "C": [offer("c", 7)]}))
        self.assertEqual(result["selected_offers"], ["a"])
        self.assertEqual(result["payments"], {"A": 9, "B": 0, "C": 0})

    def test_random_profiles_against_independent_oracle(self):
        rng = random.Random(20260912)
        for trial in range(120):
            menus = {}
            for t in range(rng.randint(1, 4)):
                menu = []
                for j in range(rng.randint(0, 2)):
                    start = rng.randrange(3)
                    menu.append(offer(f"o{t}{j}", rng.randrange(10),
                                      rng.sample(range(4), rng.randint(1, 3)), start,
                                      rng.randint(1, 3 - start), rng.randint(0, 3)))
                menus[f"t{t}"] = menu
            auction = parse_auction(profile(menus, capacity=3))
            model = compile_model(auction)
            for mask in range(1 << len(auction.offers)):
                bits = tuple((mask >> i) & 1 for i in range(len(auction.offers)))
                self.assertEqual(model.feasible(bits), oracle_feasible(auction, bits), (trial, bits))
            for excluded in (None, *auction.tenants):
                exact = solve_exact(model, excluded)
                self.assertEqual((exact.welfare, exact.bits), oracle(auction, excluded), (trial, excluded))

    def test_xor_and_all_alternatives_removed(self):
        data = profile({"A": [offer("a0", 10, (0,)), offer("a1", 9, (1,))],
                        "B": [offer("b", 8, (0,))], "C": [offer("c", 1, (1,))]})
        model = compile_model(parse_auction(data))
        self.assertFalse(model.feasible((1, 1, 0, 0)))
        result = settled(data)
        self.assertEqual(result["selected_offers"], ["a1", "b"])
        self.assertEqual(result["payments"]["A"], 1)
        self.assertEqual(result["tenants"]["A"]["without_welfare"], 9)
        self.assertIn("x_0 = 0", model.lp("A"))
        self.assertIn("x_1 = 0", model.lp("A"))

    def test_interval_endpoints_exclusive_hbm_and_cpu(self):
        data = profile({"A": [offer("a", 4, start=0)], "B": [offer("b", 3, start=1)]})
        self.assertEqual(settled(data)["welfare"], 7)
        data["tenants"][1]["offers"][0]["bundle"]["start"] = 0
        self.assertEqual(settled(data)["welfare"], 4)
        data = profile({"A": [offer("a", 4, (0,), cpu=3)], "B": [offer("b", 3, (1,), cpu=2)]}, capacity=4)
        self.assertEqual(settled(data)["welfare"], 4)

    def test_multi_host_and_large_horizon(self):
        data = profile({"A": [offer("a", 5, (0, 1), cpu=3)], "B": [offer("b", 4, (2,), cpu=2)]})
        data["inventory"]["hosts"].append({"id": "other", "cpu_cores": 2})
        data["inventory"]["slices"][1]["host"] = "other"
        data["tenants"][0]["offers"][0]["bundle"]["cpu_cores"]["other"] = 2
        data["inventory"]["horizon"] = 1_000_000_000
        model = compile_model(parse_auction(data))
        self.assertEqual(solve_exact(model).welfare, 5)
        self.assertLess(len(model.constraints), 12)

    def test_ties_zeros_empty_and_input_order(self):
        data = profile({"B": [offer("b", 4)], "A": [offer("a", 4)]})
        first = parse_auction(data)
        data["tenants"].reverse()
        data["inventory"]["slices"].reverse()
        self.assertEqual(first.digest, parse_auction(data).digest)
        self.assertEqual(settled(data)["selected_offers"], ["a"])
        for menu in ([], [offer("a", 0)]):
            empty = settled(profile({"A": menu}))
            self.assertEqual((empty["selected_offers"], empty["payments"]), ([], {"A": 0}))
        self.assertIn("x_empty = 0", compile_model(parse_auction(profile({"A": []}))).lp())

    def test_large_integer_values_and_budget(self):
        high = MAX_WELFARE // 2
        data = profile({"A": [offer("a", high + 1)], "B": [offer("b", high)]})
        self.assertEqual(settled(data)["payments"], {"A": high, "B": 0})
        model = compile_model(parse_auction(data))
        with self.assertRaises(VerificationLimit):
            solve_exact(model, node_limit=1)
        with patch("mechanism_cuopt.optimization.time.monotonic", side_effect=[0, 2]):
            with self.assertRaises(VerificationLimit):
                solve_exact(model, seconds=1)

    def test_dsic_fixed_xor_menu_and_signed_utility(self):
        # Exhaust every truthful vector and unilateral vector report in 0..3.
        # Other tenants' reports remain fixed. Utility uses the true selected
        # value minus payment, including negative utility from overbidding.
        template = profile({"A": [offer("a0", 0, (0,)), offer("a1", 0, (0, 1))],
                            "B": [offer("b", 2, (0,))], "C": [offer("c", 1, (1,))]})
        observed_negative = False
        for truth in itertools.product(range(4), repeat=2):
            truthful = deepcopy(template)
            for item, value in zip(truthful["tenants"][0]["offers"], truth):
                item["value"] = value
            honest = settled(truthful)
            honest_utility = honest["tenants"]["A"]["utility_if_truthful"]
            self.assertGreaterEqual(honest_utility, 0)
            for report in itertools.product(range(4), repeat=2):
                deviation = deepcopy(template)
                for item, value in zip(deviation["tenants"][0]["offers"], report):
                    item["value"] = value
                outcome = settled(deviation)
                chosen = outcome["tenants"]["A"]["selected_offer"]
                utility = (truth[int(chosen[-1])] if chosen else 0) - outcome["payments"]["A"]
                observed_negative |= utility < 0
                self.assertGreaterEqual(honest_utility, utility, (truth, report))
                self.assertTrue(all(p >= 0 for p in outcome["payments"].values()))
        self.assertTrue(observed_negative)


class InputTests(unittest.TestCase):
    def test_invalid_profiles(self):
        good = profile({"A": [offer("a", 5)]})
        mutations = [
            lambda d: d.update(version=True), lambda d: d.update(extra=1),
            lambda d: d.update(auction_id="bad\nMaximize"),
            lambda d: d["tenants"].append(deepcopy(d["tenants"][0])),
            lambda d: d["tenants"][0]["offers"].append(deepcopy(d["tenants"][0]["offers"][0])),
            lambda d: d["tenants"][0]["offers"][0].update(value=-1),
            lambda d: d["tenants"][0]["offers"][0].update(value=1.0),
            lambda d: d["tenants"][0]["offers"][0].update(value=MAX_WELFARE + 1),
            lambda d: d["tenants"][0]["offers"][0]["bundle"].update(duration=0),
            lambda d: d["tenants"][0]["offers"][0]["bundle"].update(start=3),
            lambda d: d["tenants"][0]["offers"][0]["bundle"].update(start=2, duration=2),
            lambda d: d["tenants"][0]["offers"][0]["bundle"].update(cpu_cores={"missing": 1}),
            lambda d: d["tenants"][0]["offers"][0]["bundle"].update(cpu_cores={"h": 5}),
            lambda d: d["tenants"][0]["offers"][0]["bundle"].update(slices=[]),
            lambda d: d["tenants"][0]["offers"][0]["bundle"]["slices"][0].update(hbm_mib=10241),
            lambda d: d["tenants"][0]["offers"][0]["bundle"]["slices"][0].update(id="absent"),
        ]
        for mutate in mutations:
            data = deepcopy(good)
            mutate(data)
            with self.subTest(mutation=mutations.index(mutate)), self.assertRaises(AuctionError):
                parse_auction(data)

    def test_json_duplicate_keys_nonfinite_and_size(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "bad.json"
            for text in ('{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}', " " * (4 * 1024 * 1024 + 1)):
                path.write_text(text)
                with self.assertRaises(AuctionError):
                    read_json(path)


class CandidateTests(unittest.TestCase):
    def setUp(self):
        self.model = compile_model(parse_auction(profile({"A": [offer("a", 5)], "B": [offer("b", 3)]})))
        self.good = reference_candidate(self.model, exact_cases(self.model))

    def test_bad_candidate_rejected_before_exact_search(self):
        with patch("mechanism_cuopt.settlement.exact_cases") as search:
            with self.assertRaises(AuctionError):
                validate_candidate(self.model, {"version": 1})
            search.assert_not_called()

    def test_rejects_forged_candidates(self):
        mutations = [
            lambda c: c.update(version=True), lambda c: c.update(auction_sha256="wrong"),
            lambda c: c["cases"].pop("without_1"), lambda c: c["cases"].update(extra={}),
            lambda c: c["cases"]["full"].update(status="FeasibleFound"),
            lambda c: c["cases"]["full"].update(lp_sha256="wrong"),
            lambda c: c["cases"]["full"].update(objective=4),
            lambda c: c["cases"]["full"].update(objective=float("nan")),
            lambda c: c["cases"]["full"]["variables"].update(x_0=0.5),
            lambda c: c["cases"]["full"]["variables"].update(x_0=True),
            lambda c: c["cases"]["full"]["variables"].update(x_0=float("inf")),
            lambda c: c["cases"]["full"]["variables"].update(x_0=10**300),
            lambda c: c["cases"]["full"].update(variables={"x_0": 1, "x_1": 1}, objective=8),
            lambda c: c["cases"]["full"].update(variables={"x_0": 0, "x_1": 1}, objective=3),
            lambda c: c["cases"]["without_0"].update(variables={"x_0": 1, "x_1": 0}, objective=5),
            lambda c: c["cases"]["without_1"].update(variables={"x_0": 0, "x_1": 0}, objective=0),
        ]
        for mutate in mutations:
            bad = deepcopy(self.good)
            mutate(bad)
            with self.subTest(mutation=mutations.index(mutate)), self.assertRaises(AuctionError):
                validate_candidate(self.model, bad)

    def test_solver_tolerance_and_tie_canonicalization(self):
        candidate = deepcopy(self.good)
        candidate["cases"]["full"]["variables"]["x_0"] = 1 - 1e-8
        self.assertEqual(validate_candidate(self.model, candidate)[0]["welfare"], 5)
        model = compile_model(parse_auction(profile({"A": [offer("a", 3)], "B": [offer("b", 3)]})))
        candidate = reference_candidate(model, exact_cases(model))
        candidate["cases"]["full"]["variables"] = {"x_0": 0, "x_1": 1}
        result, _ = validate_candidate(model, candidate)
        self.assertEqual(result["selected_offers"], ["a"])
        self.assertEqual(result["solver_selected_offers"], ["b"])
        self.assertTrue(result["tie_canonicalized"])

    def test_gpu_adapter_all_cases_and_zero_gaps(self):
        calls, parameters = [], []
        model, candidate = self.model, self.good

        class Settings:
            def set_parameter(self, name, value):
                parameters.append((name, value))

        class Problem:
            @staticmethod
            def read(path):
                name = Path(path).stem
                calls.append(name)
                instance = Problem()
                instance.result = candidate["cases"][name]
                instance.Status = types.SimpleNamespace(name="Optimal")
                instance.ObjValue = instance.result["objective"]
                return instance

            def solve(self, settings):
                self.settings = settings

            def getVariables(self):
                return [types.SimpleNamespace(VariableName=name, Value=value)
                        for name, value in self.result["variables"].items()]

        def imports(name):
            return types.SimpleNamespace(Problem=Problem) if name.endswith(".problem") else types.SimpleNamespace(SolverSettings=Settings)

        with patch("mechanism_cuopt.gpu.importlib.import_module", side_effect=imports):
            result = gpu.propose(model, Path("/portable-worker"))
        self.assertEqual(calls, ["full", "without_0", "without_1"])
        self.assertEqual(parameters.count(("mip_relative_gap", 0.0)), 3)
        self.assertEqual(parameters.count(("mip_absolute_gap", 0.0)), 3)
        self.assertEqual(validate_candidate(model, result)[0]["payments"], {"A": 3, "B": 0})

    def test_missing_cuopt_is_error(self):
        with patch("mechanism_cuopt.gpu.importlib.import_module", side_effect=ImportError("unavailable")):
            with self.assertRaisesRegex(AuctionError, "cuOpt is unavailable"):
                gpu.propose(self.model, Path("/portable-worker"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
