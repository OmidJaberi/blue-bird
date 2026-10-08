#!/usr/bin/env python3
"""Self-test for autobahn.py (no Docker needed): python3 tests/load/autobahn/test_autobahn.py"""

import json
import os
import subprocess
import sys
import tempfile
import unittest

def jload(path):
    with open(path) as f:
        return json.load(f)


def jdump(obj, path):
    with open(path, "w") as f:
        json.dump(obj, f)


def read(path):
    with open(path) as f:
        return f.read()


HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "autobahn.py")
BASELINE = os.path.join(HERE, "expected-results.json")


def run_summary(index_cases, expected=None, extra=()):
    """index_cases: {case: (behavior, behaviorClose)}. Returns (rc, stdout, results, markdown, expected_path)."""
    d = tempfile.mkdtemp()
    index = {"bluebird": {c: {"behavior": b, "behaviorClose": cl} for c, (b, cl) in index_cases.items()}}
    index_path = os.path.join(d, "index.json")
    jdump(index, index_path)

    cmd = [sys.executable, SCRIPT, "summary", "--index", index_path,
           "--results", os.path.join(d, "results.json"), "--markdown", os.path.join(d, "summary.md")]
    expected_path = None
    if expected is not None:
        expected_path = os.path.join(d, "expected.json")
        jdump(expected, expected_path)
        cmd += ["--expected", expected_path]
    cmd += list(extra)

    p = subprocess.run(cmd, capture_output=True, text=True)
    results_path, md_path = os.path.join(d, "results.json"), os.path.join(d, "summary.md")
    results = jload(results_path) if os.path.exists(results_path) else None
    md = read(md_path) if os.path.exists(md_path) else None
    return p.returncode, p.stdout + p.stderr, results, md, expected_path


class GateTests(unittest.TestCase):
    BASE = {"1.1.1": "OK/OK", "2.5": "FAILED/FAILED", "6.4.1": "FAILED/WRONG CODE", "7.1.5": "INFORMATIONAL/INFORMATIONAL"}

    def cases(self, **overrides):
        out = {c: tuple(v.split("/")) for c, v in self.BASE.items()}
        for c, v in overrides.items():
            out[c.replace("_", ".")] = tuple(v.split("/"))
        return out

    def test_same_as_baseline_passes(self):
        rc, out, _, md, _ = run_summary(self.cases(), self.BASE)
        self.assertEqual(rc, 0, out)
        self.assertIn("Gate: PASS", md)

    def test_known_failures_do_not_fail_the_run(self):
        rc, out, _, md, _ = run_summary(self.cases(), self.BASE)
        self.assertEqual(rc, 0)
        self.assertIn("2 failing", md)  # 2.5 and 6.4.1 are known failures

    def test_behavior_regression_fails(self):
        rc, out, _, md, _ = run_summary(self.cases(**{"1_1_1": "FAILED/OK"}), self.BASE)
        self.assertEqual(rc, 3, out)
        self.assertIn("regression: 1.1.1: OK/OK -> FAILED/OK", out)
        self.assertIn("Gate: FAIL", md)

    def test_close_only_regression_fails(self):
        rc, out, _, _, _ = run_summary(self.cases(**{"1_1_1": "OK/WRONG CODE"}), self.BASE)
        self.assertEqual(rc, 3, out)

    def test_unknown_status_counts_as_failing(self):
        rc, out, _, _, _ = run_summary(self.cases(**{"1_1_1": "OK/SOMETHING NEW"}), self.BASE)
        self.assertEqual(rc, 3, out)

    def test_informational_to_ok_is_not_a_regression(self):
        rc, out, _, _, _ = run_summary(self.cases(**{"7_1_5": "OK/OK"}), self.BASE)
        self.assertEqual(rc, 0, out)

    def test_non_strict_is_worse_than_ok(self):
        rc, out, _, _, _ = run_summary(self.cases(**{"1_1_1": "NON-STRICT/OK"}), self.BASE)
        self.assertEqual(rc, 3, out)

    def test_failing_case_changing_flavour_is_not_a_regression(self):
        rc, out, _, _, _ = run_summary(self.cases(**{"2_5": "UNIMPLEMENTED/UNIMPLEMENTED"}), self.BASE)
        self.assertEqual(rc, 0, out)

    def test_improvement_passes_and_is_reported(self):
        rc, out, _, md, _ = run_summary(self.cases(**{"2_5": "OK/OK"}), self.BASE)
        self.assertEqual(rc, 0, out)
        self.assertIn("1 improved", out)
        self.assertIn("#### Improvements", md)

    def test_new_failing_case_fails(self):
        rc, out, _, _, _ = run_summary(self.cases(**{"9_9_9": "FAILED/OK"}), self.BASE)
        self.assertEqual(rc, 3, out)
        self.assertIn("not in the baseline", out)

    def test_new_passing_case_is_fine(self):
        rc, out, _, _, _ = run_summary(self.cases(**{"9_9_9": "OK/OK"}), self.BASE)
        self.assertEqual(rc, 0, out)

    def test_missing_case_only_fails_when_complete_run_required(self):
        partial = self.cases()
        del partial["1.1.1"]
        rc, out, _, _, _ = run_summary(partial, self.BASE)
        self.assertEqual(rc, 0, out)
        rc, out, _, _, _ = run_summary(partial, self.BASE, ["--require-complete"])
        self.assertEqual(rc, 3, out)
        self.assertIn("missing from this run", out)

    def test_update_expected_rewrites_baseline_and_passes(self):
        rc, out, _, _, path = run_summary(self.cases(**{"1_1_1": "FAILED/FAILED"}), self.BASE, ["--update-expected"])
        self.assertEqual(rc, 0, out)
        self.assertEqual(jload(path)["1.1.1"], "FAILED/FAILED")

    def test_no_baseline_is_report_only(self):
        rc, out, _, md, _ = run_summary(self.cases(**{"1_1_1": "FAILED/FAILED"}))
        self.assertEqual(rc, 0, out)
        self.assertIn("Report only", md)

    def test_numeric_case_order(self):
        _, _, results, _, _ = run_summary({"10.1.1": ("OK", "OK"), "2.10": ("OK", "OK"), "2.2": ("OK", "OK")})
        self.assertEqual(list(results), ["2.2", "2.10", "10.1.1"])

    def test_empty_and_missing_reports_are_infrastructure_errors(self):
        d = tempfile.mkdtemp()
        base = [sys.executable, SCRIPT, "summary", "--results", d + "/r.json", "--markdown", d + "/s.md"]
        self.assertEqual(subprocess.run(base + ["--index", d + "/nope.json"], capture_output=True).returncode, 2)
        jdump({}, d + "/empty.json")
        self.assertEqual(subprocess.run(base + ["--index", d + "/empty.json"], capture_output=True).returncode, 2)


@unittest.skipUnless(os.path.exists(BASELINE), "committed baseline not present")
class CommittedBaselineTests(unittest.TestCase):
    def setUp(self):
        self.baseline = jload(BASELINE)

    def as_cases(self, mapping):
        return {c: tuple(v.split("/")) for c, v in mapping.items()}

    def test_baseline_is_well_formed(self):
        self.assertGreater(len(self.baseline), 200)
        for case, value in self.baseline.items():
            self.assertRegex(case, r"^\d+(\.\d+)*$")
            self.assertEqual(value.count("/"), 1, (case, value))

    def test_baseline_passes_against_itself(self):
        rc, out, _, _, _ = run_summary(self.as_cases(self.baseline), self.baseline, ["--require-complete"])
        self.assertEqual(rc, 0, out)

    def test_breaking_a_passing_case_is_caught(self):
        passing = next(c for c, v in self.baseline.items() if v == "OK/OK")
        mutated = dict(self.baseline)
        mutated[passing] = "FAILED/OK"
        rc, out, _, _, _ = run_summary(self.as_cases(mutated), self.baseline, ["--require-complete"])
        self.assertEqual(rc, 3, out)
        self.assertIn(f"regression: {passing}", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
