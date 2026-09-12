"""Check real mechanism proofs, artifact integrity, and CLI failure behavior."""

from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from mechanism_cuopt.artifacts import audit_settlement, export_auction, load_bundle, solve_and_publish, verify_and_publish
from mechanism_cuopt.certificate import kernel_check, settlement_certificate
from mechanism_cuopt.model import AuctionError, MAX_WELFARE, canonical, load_auction, parse_auction, read_json, sha256
from mechanism_cuopt.optimization import VerificationLimit, compile_model
from mechanism_cuopt.settlement import exact_cases, reference_candidate


class KernelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.auction = load_auction(ROOT / "examples/combinatorial/three-bidders.json")
        cls.model = compile_model(cls.auction)
        cls.exact = exact_cases(cls.model)
        cls.candidate = reference_candidate(cls.model, cls.exact)
        cls.certificate, cls.exports = settlement_certificate(
            cls.model, cls.exact["full"], {t: cls.exact[f"without_{i}"] for i, t in enumerate(cls.auction.tenants)}, ROOT)

    def test_closed_certificate_and_exact_exports(self):
        result = kernel_check(self.certificate, ROOT, self.exports)
        self.assertEqual(result.exports, {"fullWelfare": 16, "payment_0": 0, "payment_1": 5, "payment_2": 3, "totalRevenue": 8})

    def test_mutated_proofs_rejected(self):
        changes = [
            ("(vcgAdd deviation pivot) (vcgAdd truth pivot)", "(vcgAdd truth pivot) (vcgAdd deviation pivot)"),
            ("(optimal deviation allowed)", "allowed"),
            ("def fullChoice_0 : AuctionChoice := auctionLose", "def fullChoice_0 : AuctionChoice := auctionWin"),
            ("def without_1Choice_1 : AuctionChoice := auctionLose", "def without_1Choice_1 : AuctionChoice := auctionWin"),
            ("def payment_1 : Nat := vcgNativePayment fullWelfare own_1 without_1Welfare", "def payment_1 : Nat := 0"),
            ("def offerValue_1 : Nat := 9", "def offerValue_1 : Nat := 8"),
        ]
        for before, after in changes:
            self.assertIn(before, self.certificate)
            with self.subTest(mutation=after), self.assertRaises(AuctionError):
                kernel_check(self.certificate.replace(before, after, 1), ROOT)

    def test_capacity_and_payment_bounds_are_not_vacuous(self):
        # These otherwise well-formed false arithmetic claims must fail in
        # the real checker, not only at the host's integer validation layer.
        false_claims = [
            "def falseCpu : MechEq Nat (natSub 9 8) 0 := mechRefl Nat 0\n",
            "def falseIr : MechEq Nat (natSub 10 9) 0 := mechRefl Nat 0\n",
            "def falsePayment : MechEq Nat (natAdd 4 7) 12 := mechRefl Nat 12\n",
        ]
        for claim in false_claims:
            with self.subTest(claim=claim), self.assertRaises(AuctionError):
                kernel_check(self.certificate + claim, ROOT)

    def test_source_axiom_and_invalid_exports_rejected(self):
        for source, exports in ((self.certificate + "axiom unproved : Nat\n", ()),
                                (self.certificate, ("doesNotExist",)),
                                (self.certificate + 'def textValue : String := "hello"\n', ("textValue",))):
            with self.subTest(exports=exports), self.assertRaises(AuctionError):
                kernel_check(source, ROOT, exports)

    def test_empty_and_large_values_kernel(self):
        empty = self.auction.to_dict()
        for tenant in empty["tenants"]:
            tenant["offers"] = []
        large = self.auction.to_dict()
        # Keep the total at the admitted maximum. This would not fit Wasm i32.
        high = MAX_WELFARE // 2
        large["tenants"][0]["offers"][0]["value"] = high + 1
        large["tenants"][1]["offers"][0]["value"] = high
        large["tenants"][2]["offers"][0]["value"] = 0
        for data, expected in ((empty, 0), (large, high + 1)):
            model = compile_model(parse_auction(data))
            exact = exact_cases(model)
            certificate, exports = settlement_certificate(model, exact["full"],
                {t: exact[f"without_{i}"] for i, t in enumerate(model.auction.tenants)}, ROOT)
            self.assertEqual(kernel_check(certificate, ROOT, exports).exports["fullWelfare"], expected)

    def test_export_solve_verify_audit_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            artifact, output = base / "model", base / "settlement"
            export_auction(self.auction, artifact, ROOT)
            result = solve_and_publish(artifact, output, "reference", ROOT)
            self.assertTrue(result["validated"] and result["kernel_checked"])
            self.assertEqual(audit_settlement(artifact, output, ROOT)["payments"], {"A": 0, "B": 5, "C": 3})
            with self.assertRaises(AuctionError):
                solve_and_publish(artifact, output, "reference", ROOT)
            altered = read_json(output / "settlement.json")
            altered["payments"]["B"] = 0
            (output / "settlement.json").write_bytes(canonical(altered))
            # Even rewriting the receipt cannot pass independent replay.
            receipt = read_json(output / "receipt.json")
            receipt["settlement_sha256"] = sha256(canonical(altered))
            (output / "receipt.json").write_bytes(canonical(receipt))
            with self.assertRaisesRegex(AuctionError, "stored settlement"):
                audit_settlement(artifact, output, ROOT)

    def test_tampered_lp_and_spec_even_with_rehashed_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            artifact = Path(temporary) / "model"
            export_auction(self.auction, artifact, ROOT)
            manifest = read_json(artifact / "manifest.json")
            original = (artifact / "full.lp").read_bytes()
            modified = original.replace(b"12 x_0", b"99 x_0")
            self.assertNotEqual(original, modified)
            (artifact / "full.lp").write_bytes(modified)
            manifest["models"]["full"] = sha256(modified)
            (artifact / "manifest.json").write_bytes(canonical(manifest))
            with self.assertRaisesRegex(AuctionError, "LP disagrees"):
                load_bundle(artifact)
            (artifact / "full.lp").write_bytes(original)
            manifest["models"]["full"] = sha256(original)
            source = (artifact / "mechanism.mech").read_bytes() + b"\n-- changed contract\n"
            (artifact / "mechanism.mech").write_bytes(source)
            manifest["spec_sha256"] = sha256(source)
            (artifact / "manifest.json").write_bytes(canonical(manifest))
            output = Path(temporary) / "must-not-exist"
            with self.assertRaisesRegex(AuctionError, "installed VCG contract"):
                verify_and_publish(artifact, self.candidate, output, ROOT)
            self.assertFalse(output.exists())

    def test_failed_optimality_or_kernel_never_publishes(self):
        with tempfile.TemporaryDirectory() as temporary:
            artifact = Path(temporary) / "model"
            export_auction(self.auction, artifact, ROOT)
            output = Path(temporary) / "must-not-exist"
            with self.assertRaises(VerificationLimit):
                verify_and_publish(artifact, self.candidate, output, ROOT, node_limit=1)
            with patch("mechanism_cuopt.artifacts.kernel_check", side_effect=AuctionError("rejected")):
                with self.assertRaises(AuctionError):
                    verify_and_publish(artifact, self.candidate, output, ROOT)
            self.assertFalse(output.exists())

    def test_cli_roundtrip_from_outside_source(self):
        def cli(*arguments, expected=0):
            result = subprocess.run([sys.executable, "-P", str(ROOT / "dev/mech-cuopt.py"), *map(str, arguments)],
                                    cwd=temporary, capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, expected, result.stderr)
            return json.loads(result.stdout) if expected == 0 else result.stderr

        with tempfile.TemporaryDirectory() as temporary:
            artifact, output = Path(temporary) / "model", Path(temporary) / "result"
            cli("export", ROOT / "examples/combinatorial/three-bidders.json", "--out", artifact, "--mech-root", ROOT)
            self.assertEqual(cli("inspect", artifact)["models"], 4)
            self.assertEqual(cli("solve", artifact, "--out", output, "--backend", "reference", "--mech-root", ROOT)["revenue"], 8)
            self.assertTrue(cli("check-certificate", output / "certificate.mech", "--mech-root", ROOT)["kernel_checked"])
            cli("verify", artifact, output / "candidate.json", "--out", Path(temporary) / "again", "--mech-root", ROOT)
            cli("audit", artifact, output, "--mech-root", ROOT)
            cli("solve", artifact, "--backend", "reference", "--out", Path(temporary) / "bad", "--seconds", "nan", expected=2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
