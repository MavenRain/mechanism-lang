"""Economic boundary, solver boundary, source proof, and host regressions."""

import importlib.util
import io
import itertools
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("gpu_auction_bridge", ROOT / "examples/gpu-auction/auction.py")
bridge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge
SPEC.loader.exec_module(bridge)


def hand_model(bids):
    winner = sorted(range(3), key=lambda i: (-bids[i], i))[0]
    price = sorted(bids, reverse=True)[1]
    return bridge.CheckedAuction(tuple(bids), tuple(int(i == winner) for i in range(3)),
                                 tuple(price if i == winner else 0 for i in range(3)))


class SolverBoundary(unittest.TestCase):
    def test_gpu_worker_needs_no_mechanism_compiler(self):
        solution = {**bridge.reference_solve(hand_model((12, 9, 7))), "backend": "cuopt"}
        with tempfile.TemporaryDirectory(prefix="gpu-worker-") as temporary:
            output = Path(temporary) / "candidate.json"
            with patch.object(bridge, "cuopt_solve", return_value=solution), \
                    patch.object(bridge, "checked_source", side_effect=AssertionError("compiler invoked")), \
                    patch("sys.stdout", new_callable=io.StringIO) as stdout:
                self.assertEqual(bridge.main(["propose", temporary, "--out", str(output)]), 0)
                self.assertFalse(json.loads(stdout.getvalue())["validated"])
                self.assertEqual(json.loads(output.read_text()), solution)

    def test_hand_solved_case(self):
        model = hand_model((12, 9, 7))
        settlement = bridge.validate_solution(model, bridge.reference_solve(model))
        self.assertEqual(settlement["allocation"], {"A": 8, "B": 0, "C": 0})
        self.assertEqual(settlement["payments"], {"A": 9, "B": 0, "C": 0})
        self.assertEqual(settlement["reported_welfare"], 12)
        self.assertEqual(settlement["ranked_objective"], 38)

    def test_lp_objective_preserves_every_small_bid_order_and_tie(self):
        for bids in itertools.product(range(5), repeat=3):
            model = hand_model(bids)
            lp = bridge.lp_text(model)
            # Read the actual exported objective, independently of the model property.
            row = next(line for line in lp.splitlines() if "ranked_welfare:" in line)
            terms = re.findall(r"(\d+) (x_[ABC])", row)
            self.assertEqual(len(terms), 3)
            coefficients = {name: int(value) for value, name in terms}
            candidates = ((1, 0, 0), (0, 1, 0), (0, 0, 1))
            selected = max(candidates, key=lambda bits: sum(
                coefficients[name] * bit for name, bit in zip(bridge.VARIABLES, bits)))
            self.assertEqual(selected, model.allocation)
            self.assertIn("one_bundle: x_A + x_B + x_C = 1", lp)
            self.assertIn("mig_capacity: 8 x_A + 8 x_B + 8 x_C <= 8", lp)
            self.assertIn("Binary\n x_A x_B x_C\nEnd", lp)

    def test_signed_utility_including_overbidding(self):
        count = 0
        for bidder in range(3):
            for profile in itertools.product(range(5), repeat=3):
                truth = hand_model(profile)
                value = profile[bidder]
                truthful_utility = value * truth.allocation[bidder] - truth.payments[bidder]
                self.assertGreaterEqual(truthful_utility, 0)
                self.assertTrue(all(payment >= 0 for payment in truth.payments))
                for report in range(5):
                    changed = list(profile)
                    changed[bidder] = report
                    deviation = hand_model(changed)
                    utility = value * deviation.allocation[bidder] - deviation.payments[bidder]
                    self.assertGreaterEqual(truthful_utility, utility)
                    count += 1
        self.assertEqual(count, 1875)
        overbid = hand_model((12, 13, 7))
        self.assertEqual(9 * overbid.allocation[1] - overbid.payments[1], -3)

    def test_reject_solver_corruption(self):
        model = hand_model((12, 9, 7))
        valid = bridge.reference_solve(model)
        variants = [
            {"status": "TimeLimit"}, {"status": "PrimalFeasible"}, {"objective": 37},
            {"objective": float("nan")}, {"objective": True}, {"backend": "unknown"},
            {"objective": 10 ** 1000}, {"lp_sha256": "wrong-model"},
            {"variables": {"x_A": 0.5, "x_B": 0.5, "x_C": 0}},
            {"variables": {"x_A": 1, "x_B": 1, "x_C": 0}},
            {"variables": {"x_A": 0, "x_B": 0, "x_C": 0}},
            {"variables": {"x_A": 0, "x_B": 1, "x_C": 0}},
            {"variables": {"x_A": True, "x_B": 0, "x_C": 0}},
            {"variables": {"x_A": float("inf"), "x_B": 0, "x_C": 0}},
            {"variables": {"x_A": 10 ** 1000, "x_B": 0, "x_C": 0}},
            {"variables": {"x_A": -1, "x_B": 2, "x_C": 0}},
            {"variables": {"x_A": 1, "x_B": 0}},
            {"variables": {"x_A": 1, "x_B": 0, "x_C": 0, "x_D": 0}},
        ]
        for changes in variants:
            with self.subTest(changes=changes), self.assertRaises(bridge.AuctionError):
                bridge.validate_solution(model, {**valid, **changes})
        tied = hand_model((9, 9, 9))
        wrong_tie = {**bridge.reference_solve(tied), "variables": {"x_A": 0, "x_B": 1, "x_C": 0}}
        with self.assertRaises(bridge.AuctionError):
            bridge.validate_solution(tied, wrong_tie)

    def test_cuopt_api_adapter_and_incomplete_status(self):
        settings = {}
        problem = types.SimpleNamespace(
            Status=types.SimpleNamespace(name="Optimal"), ObjValue=38.0,
            solve=lambda config: None,
            getVariables=lambda: [types.SimpleNamespace(VariableName=name, Value=value)
                                  for name, value in zip(bridge.VARIABLES, (1, 0, 0))],
        )
        requested = []
        modules = {
            "cuopt.linear_programming.problem": types.SimpleNamespace(
                Problem=types.SimpleNamespace(read=lambda path: requested.append(path) or problem)),
            "cuopt.linear_programming.solver_settings": types.SimpleNamespace(
                SolverSettings=lambda: types.SimpleNamespace(set_parameter=settings.__setitem__)),
        }
        with tempfile.TemporaryDirectory(prefix="gpu-auction-api-") as temporary, \
                patch.object(bridge.importlib, "import_module", side_effect=modules.__getitem__):
            directory = Path(temporary)
            (directory / "allocation.lp").write_text(bridge.lp_text(hand_model((12, 9, 7))))
            result = bridge.cuopt_solve(directory)
            self.assertTrue(bridge.validate_solution(hand_model((12, 9, 7)), result)["validated"])
            self.assertEqual(requested, [str(directory / "allocation.lp")])
            self.assertEqual(settings["mip_relative_gap"], 0.0)
            self.assertEqual(settings["mip_absolute_gap"], 0.0)
            problem.Status.name = "TimeLimit"
            with self.assertRaises(bridge.AuctionError):
                bridge.cuopt_solve(directory)


class KernelIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="gpu-auction-test-")
        cls.directory = Path(cls.temporary.name)
        cls.artifact = cls.directory / "hand-case"
        cls.model = bridge.export(bridge.HERE / "three-bidders.mech", cls.artifact, hosts=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def check_source(self, text):
        path = self.directory / "control.mech"
        path.write_text(text)
        return subprocess.run([str(ROOT / "_build/default/bin/mech.exe"), "check", str(path)],
                              capture_output=True, text=True, timeout=60, check=False)

    def test_kernel_node_and_reference_agree(self):
        self.assertEqual(self.model, hand_model((12, 9, 7)))
        self.assertEqual(bridge.load_artifact(self.artifact), self.model)

    def test_false_equality_utility_and_payment_proofs_fail(self):
        source = (self.artifact / "auction.mech").read_text()
        mutations = [
            ("MechEq Nat gpuPA 9 := mechRefl Nat 9", "MechEq Nat gpuPA 8 := mechRefl Nat 8"),
            ("| auctionWin => AuctionLe value price", "| auctionWin => AuctionLe price value"),
            ("| auctionWin => price\n", "| auctionWin => mechZero\n"),
        ]
        for old, new in mutations:
            self.assertIn(old, source)
            with self.subTest(mutation=old):
                result = self.check_source(source.replace(old, new, 1))
                self.assertEqual(result.returncode, 1, result.stderr)

    def test_axiom_injection_rejected(self):
        path = self.directory / "axiom.mech"
        path.write_text((self.artifact / "auction.mech").read_text() + "\naxiom untrusted : Nat\n")
        with self.assertRaisesRegex(bridge.AuctionError, "declares axioms"):
            bridge.checked_source(path, self.directory / "axiom-wasm")

    def test_lp_tampering_rejected(self):
        original = (self.artifact / "allocation.lp").read_text()
        try:
            (self.artifact / "allocation.lp").write_text(original.replace("38 x_A", "1 x_A"))
            with self.assertRaisesRegex(bridge.AuctionError, "LP hash mismatch"):
                bridge.load_artifact(self.artifact)
        finally:
            (self.artifact / "allocation.lp").write_text(original)

    def test_all_winners_and_tie_positions(self):
        fixture = (bridge.HERE / "three-bidders.mech").read_text()
        suffix = fixture[fixture.index("def gpuSlices"):fixture.index("-- Hand calculation")]
        for number, bids in enumerate(((0, 0, 0), (3, 3, 1), (1, 3, 3),
                                       (3, 1, 3), (1, 3, 2), (1, 2, 3), (0, 0, 3),
                                       (bridge.MAX_BID, bridge.MAX_BID, bridge.MAX_BID - 1))):
            program = self.directory / f"profile-{number}.mech"
            definitions = "\n".join(f"def gpuBid{label} : MechNat := " +
                                     "mechSucc (" * bid + "mechZero" + ")" * bid
                                     for label, bid in zip("ABC", bids))
            program.write_text(definitions + "\n" + suffix +
                               "def gpuCapacityChecked : MechEq Nat gpuAllocated 8 := mechRefl Nat 8\n"
                               "def gpuTransportedSlices : Nat := mechTransport Nat (fun (n : Nat) => Nat) "
                               "gpuAllocated 8 gpuCapacityChecked gpuSlices\n")
            with self.subTest(bids=bids):
                result = bridge.export(program, self.directory / f"artifact-{number}", hosts=True)
                self.assertEqual(result, hand_model(bids))


if __name__ == "__main__":
    unittest.main()
