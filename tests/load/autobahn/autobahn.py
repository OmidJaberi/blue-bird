#!/usr/bin/env python3
"""Helpers for tests/load/run_autobahn.sh.

  autobahn.py config  --template T --out O --url URL --cases A,B --exclude C,D
      Render the fuzzingclient config from the committed template.

  autobahn.py summary --index reports/servers/index.json --results R.json --markdown S.md
                      [--expected expected-results.json [--require-complete] [--update-expected]]
      Summarize Autobahn's index.json: write a flat {case: "behavior/behaviorClose"}
      results file and a markdown summary, and print the counts.

      With --expected, the run is gated against that baseline: a case that got
      WORSE than its baseline status is a regression. Known failures do not fail the
      run, and improvements are reported (refresh the baseline with --update-expected).

Exit status of `summary`: 0 ok, 2 report missing/unreadable/empty (infrastructure
problem), 3 the gate failed (regressions against the baseline).
"""

import argparse
import json
import sys

AGENT = "bluebird"
MAX_LISTED = 60

GROUP_NAMES = {
    "1": "framing",
    "2": "ping/pong",
    "3": "reserved bits",
    "4": "opcodes",
    "5": "fragmentation",
    "6": "UTF-8",
    "7": "close handling",
    "9": "limits / performance",
    "10": "misc",
    "12": "compression",
    "13": "compression",
}


def rank(status):
    """0 passing, 1 non-strict, 2 failing. Anything unknown counts as failing."""
    if status in ("OK", "INFORMATIONAL"):
        return 0
    if status == "NON-STRICT":
        return 1
    return 2


def case_key(case_id):
    return [int(part) if part.isdigit() else 0 for part in case_id.split(".")]


def split_csv(value):
    return [item.strip() for item in value.split(",") if item.strip()]


def split_status(value):
    behavior, _, close = value.partition("/")
    return behavior, close


def case_rank(value):
    behavior, close = split_status(value)
    return max(rank(behavior), rank(close))


def compare(results, expected, require_complete):
    """Return (regressions, improvements, new_passing).

    regressions:  [(case, expected_or_None, now_or_None, reason)]
    improvements: [(case, expected, now)]
    new_passing:  [(case, now)]  cases the baseline doesn't know that pass
    """
    regressions, improvements, new_passing = [], [], []

    for case_id in sorted(results, key=case_key):
        now = results[case_id]
        exp = expected.get(case_id)

        if exp is None:
            if case_rank(now) >= 2:
                regressions.append((case_id, None, now, "new case failing (not in the baseline)"))
            else:
                new_passing.append((case_id, now))
            continue

        nb, nc = split_status(now)
        eb, ec = split_status(exp)
        worse = rank(nb) > rank(eb) or rank(nc) > rank(ec)
        better = rank(nb) < rank(eb) or rank(nc) < rank(ec)
        if worse:
            regressions.append((case_id, exp, now, "got worse"))
        elif better:
            improvements.append((case_id, exp, now))

    if require_complete:
        for case_id in sorted(expected, key=case_key):
            if case_id not in results:
                regressions.append((case_id, expected[case_id], None, "missing from this run"))

    return regressions, improvements, new_passing


def group_of(case_id):
    return case_id.split(".")[0]


def cmd_config(args):
    with open(args.template) as f:
        config = json.load(f)

    config["servers"][0]["url"] = args.url
    config["cases"] = split_csv(args.cases)
    config["exclude-cases"] = split_csv(args.exclude)

    with open(args.out, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")


def write_results(path, results):
    with open(path, "w") as f:
        json.dump({k: results[k] for k in sorted(results, key=case_key)}, f, indent=2)
        f.write("\n")


def table(header, rows, limit=MAX_LISTED):
    columns = header.count("|") - 1
    lines = [header, "|" + "---|" * columns]
    lines += rows[:limit]
    if len(rows) > limit:
        cells = [f"{len(rows) - limit} more, see results.json"] + [""] * (columns - 2)
        lines.append("| … | " + " | ".join(cells) + " |")
    return lines


def cmd_summary(args):
    try:
        with open(args.index) as f:
            index = json.load(f)
    except (OSError, ValueError) as err:
        print(f"error: cannot read Autobahn report {args.index}: {err}", file=sys.stderr)
        return 2

    cases = index.get(AGENT)
    if not isinstance(cases, dict) or not cases:
        print(f"error: Autobahn report has no results for agent '{AGENT}'", file=sys.stderr)
        return 2

    results = {}
    behavior_counts = {}
    for case_id, entry in cases.items():
        behavior = entry.get("behavior", "UNKNOWN")
        close = entry.get("behaviorClose", "UNKNOWN")
        results[case_id] = f"{behavior}/{close}"
        behavior_counts[behavior] = behavior_counts.get(behavior, 0) + 1

    write_results(args.results, results)

    total = len(results)
    ranks = [case_rank(v) for v in results.values()]
    passing, nonstrict, failing = ranks.count(0), ranks.count(1), ranks.count(2)

    gate = None  # None: no baseline, True: pass, False: fail
    regressions, improvements, new_passing = [], [], []
    if args.expected:
        try:
            with open(args.expected) as f:
                expected = json.load(f)
        except (OSError, ValueError) as err:
            print(f"error: cannot read expected results {args.expected}: {err}", file=sys.stderr)
            return 2

        if args.update_expected:
            write_results(args.expected, results)
            expected = dict(results)
            print(f"Updated {args.expected} from this run ({total} cases).")

        regressions, improvements, new_passing = compare(results, expected, args.require_complete)
        gate = not regressions

    # ------------------------------------------------------------ markdown
    lines = ["### Autobahn WebSocket conformance", ""]
    summary = f"{total} cases run: {passing} passing, {failing} failing"
    if nonstrict:
        summary += f", {nonstrict} non-strict"
    breakdown = ", ".join(f"{n} {b}" for b, n in sorted(behavior_counts.items(), key=lambda kv: -kv[1]))
    lines += [summary + f" (behavior: {breakdown}).", ""]

    if gate is True:
        lines += [f"**Gate: PASS** against the committed baseline "
                  f"({len(improvements)} improved, {len(new_passing)} new passing).", ""]
    elif gate is False:
        lines += [f"**Gate: FAIL**: {len(regressions)} regression(s) against the committed baseline.", ""]
    else:
        lines += ["Report only: no baseline was used.", ""]

    if regressions:
        rows = [f"| {c} | {e or 'n/a'} | {n or 'missing'} | {why} |" for c, e, n, why in regressions]
        lines += ["#### Regressions", ""] + table("| Case | Baseline | Now | Why |", rows) + [""]

    if improvements or new_passing:
        rows = [f"| {c} | {e} | {n} |" for c, e, n in improvements]
        rows += [f"| {c} | not in baseline | {n} |" for c, n in new_passing]
        lines += ["#### Improvements (refresh the baseline with `BB_AUTOBAHN_UPDATE_EXPECTED=1`)", ""]
        lines += table("| Case | Baseline | Now |", rows) + [""]

    groups = {}
    for case_id, value in results.items():
        g = groups.setdefault(group_of(case_id), [0, 0])
        g[1] += 1
        if case_rank(value) >= 2:
            g[0] += 1
    if any(g[0] for g in groups.values()):
        rows = []
        for g in sorted(groups, key=lambda x: int(x) if x.isdigit() else 0):
            bad, tot = groups[g]
            if bad:
                rows.append(f"| {g}.* {GROUP_NAMES.get(g, '')} | {bad} / {tot} |")
        lines += ["#### Failing cases by group", "", "| Group | Failing / total |", "|---|---|"] + rows + [""]

    if gate is None:
        bad_rows = [f"| {c} | {v} |" for c, v in sorted(results.items(), key=lambda kv: case_key(kv[0]))
                    if case_rank(v) >= 2]
        if bad_rows:
            lines += ["#### Failing cases", ""] + table("| Case | Behavior / close |", bad_rows) + [""]

    with open(args.markdown, "w") as f:
        f.write("\n".join(lines))

    gate_text = {None: "no baseline", True: "gate PASS", False: f"gate FAIL ({len(regressions)} regressions)"}[gate]
    print(f"Autobahn: {total} cases - {passing} passing, {failing} failing"
          + (f", {nonstrict} non-strict" if nonstrict else "") + f" | {gate_text}")
    for c, e, n, why in regressions[:20]:
        print(f"  regression: {c}: {e or 'n/a'} -> {n or 'missing'} ({why})")
    if len(regressions) > 20:
        print(f"  ... and {len(regressions) - 20} more")
    if improvements or new_passing:
        print(f"  {len(improvements)} improved, {len(new_passing)} new passing: refresh the baseline with "
              "BB_AUTOBAHN_UPDATE_EXPECTED=1")

    return 3 if gate is False else 0


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    c = sub.add_parser("config")
    c.add_argument("--template", required=True)
    c.add_argument("--out", required=True)
    c.add_argument("--url", required=True)
    c.add_argument("--cases", default="*")
    c.add_argument("--exclude", default="")
    c.set_defaults(func=cmd_config)

    s = sub.add_parser("summary")
    s.add_argument("--index", required=True)
    s.add_argument("--results", required=True)
    s.add_argument("--markdown", required=True)
    s.add_argument("--expected")
    s.add_argument("--require-complete", action="store_true")
    s.add_argument("--update-expected", action="store_true")
    s.set_defaults(func=cmd_summary)

    args = parser.parse_args()
    sys.exit(args.func(args) or 0)


if __name__ == "__main__":
    main()
