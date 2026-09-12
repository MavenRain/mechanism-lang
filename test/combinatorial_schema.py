"""Validate the published schemas with the optional test dependency."""

from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from jsonschema import Draft202012Validator
from mechanism_cuopt.model import load_auction, read_json
from mechanism_cuopt.optimization import compile_model
from mechanism_cuopt.settlement import exact_cases, reference_candidate


class SchemaTests(unittest.TestCase):
    def test_auction_examples(self):
        schema = read_json(ROOT / "schema/auction.schema.json")
        Draft202012Validator.check_schema(schema)
        validator = Draft202012Validator(schema)
        for path in sorted((ROOT / "examples/combinatorial").glob("*.json")):
            if path.name.endswith(".expected.json"):
                continue
            validator.validate(load_auction(path).to_dict())
        bad = load_auction(ROOT / "examples/combinatorial/three-bidders.json").to_dict()
        bad["tenants"][0]["offers"][0]["value"] = -1
        self.assertFalse(validator.is_valid(bad))

    def test_candidate_shape(self):
        schema = read_json(ROOT / "schema/candidate.schema.json")
        Draft202012Validator.check_schema(schema)
        validator = Draft202012Validator(schema)
        model = compile_model(load_auction(ROOT / "examples/combinatorial/three-bidders.json"))
        candidate = reference_candidate(model, exact_cases(model))
        validator.validate(candidate)
        bad = deepcopy(candidate)
        bad["cases"]["full"]["status"] = "TimeLimit"
        self.assertFalse(validator.is_valid(bad))


if __name__ == "__main__":
    unittest.main(verbosity=2)
