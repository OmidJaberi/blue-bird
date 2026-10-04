#!/usr/bin/env python3
"""Helpers for tests/load/run_autobahn.sh.

  autobahn.py config  --template T --out O --url URL --cases A,B --exclude C,D
      Render the fuzzingclient config from the committed template.

  autobahn.py summary --index reports/servers/index.json --results R.json --markdown S.md
      Summarize Autobahn's index.json. Writes a flat {case: "behavior/behaviorClose"}
      results file and a markdown summary, prints the counts, and exits 2 if the
      report is missing, unreadable or empty (an infrastructure problem, as opposed
      to a protocol conformance failure, which is only reported).
"""

import argparse
import json
import sys

AGENT = "bluebird"
BAD = {"FAILED", "UNIMPLEMENTED"}
ORDER = ["OK", "NON-STRICT", "INFORMATIONAL", "FAILED", "UNIMPLEMENTED"]
MAX_LISTED = 60


def case_key(case_id):
    return [int(part) if part.isdigit() else 0 for part in case_id.split(".")]


def split(value):
    return [item.strip() for item in value.split(",") if item.strip()]


def cmd_config(args):
    with open(args.template) as f:
        config = json.load(f)

    config["servers"][0]["url"] = args.url
    config["cases"] = split(args.cases)
    config["exclude-cases"] = split(args.exclude)

    with open(args.out, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")


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
    counts = {name: 0 for name in ORDER}
    problems = []  # (case, behavior, behaviorClose)

    for case_id in sorted(cases, key=case_key):
        entry = cases[case_id]
        behavior = entry.get("behavior", "UNKNOWN")
        close = entry.get("behaviorClose", "UNKNOWN")
        results[case_id] = f"{behavior}/{close}"
        counts[behavior] = counts.get(behavior, 0) + 1
        if behavior in BAD or close in BAD or behavior == "NON-STRICT":
            problems.append((case_id, behavior, close))

    with open(args.results, "w") as f:
        json.dump(results, f, indent=2, sort_keys=False)
        f.write("\n")

    total = len(cases)
    lines = ["### Autobahn WebSocket conformance", ""]
    lines.append(f"{total} cases run. " + ", ".join(f"{counts.get(n, 0)} {n}" for n in ORDER if counts.get(n, 0)))
    lines.append("")

    hard = [p for p in problems if p[1] in BAD or p[2] in BAD]
    soft = [p for p in problems if p not in hard]

    if hard:
        lines += [f"**{len(hard)} failing or unimplemented**", ""]
        lines += ["| Case | Behavior | Close |", "|---|---|---|"]
        lines += [f"| {c} | {b} | {cl} |" for c, b, cl in hard[:MAX_LISTED]]
        if len(hard) > MAX_LISTED:
            lines.append(f"| … | {len(hard) - MAX_LISTED} more, see results.json | |")
        lines.append("")
    if soft:
        ids = ", ".join(c for c, _, _ in soft[:MAX_LISTED])
        more = f" (+{len(soft) - MAX_LISTED} more)" if len(soft) > MAX_LISTED else ""
        lines += [f"{len(soft)} non-strict: {ids}{more}", ""]
    if not problems:
        lines += ["All cases OK or informational.", ""]

    with open(args.markdown, "w") as f:
        f.write("\n".join(lines))

    print(f"Autobahn: {total} cases - " + ", ".join(f"{counts.get(n, 0)} {n}" for n in ORDER if counts.get(n, 0)))
    return 0


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
    s.set_defaults(func=cmd_summary)

    args = parser.parse_args()
    sys.exit(args.func(args) or 0)


if __name__ == "__main__":
    main()
