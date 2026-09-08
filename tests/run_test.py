#!/usr/bin/env python3
"""Test driver for hybrid-inverse-kinematics-solver.

Two kinds of test:

  golden  Run the solver over an example and compare the resulting .sto against
          a committed reference, column by column, within a tolerance.

  error   Run the solver over a deliberately broken setup file and assert on the
          exit code and on a substring of its output.

The golden references are tied to the OpenSim version they were produced with
(4.4). After an OpenSim upgrade, differences within tolerance are fine; larger
ones need a look, and once understood the references are refreshed with
--regenerate. Never regenerate to make a red test go green without reading the
diff it was reporting.
"""
import argparse
import os
import shutil
import subprocess
import sys


def load_sto(path):
    """Return (column labels, rows) from an OpenSim .sto/.mot file."""
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        lines = fh.read().split("\n")
    i = next(k for k, l in enumerate(lines) if l.strip().lower() == "endheader") + 1
    while not lines[i].strip():          # .mot files carry a blank line here
        i += 1
    cols = lines[i].split()
    rows = [[float(x) for x in l.split()] for l in lines[i + 1:] if l.strip()]
    return cols, rows


def compare(produced, expected, tol):
    """Return a list of human-readable differences, empty if the files match."""
    pc, pr = load_sto(produced)
    ec, er = load_sto(expected)
    problems = []
    if pc != ec:
        only_p = [c for c in pc if c not in ec]
        only_e = [c for c in ec if c not in pc]
        problems.append("column labels differ (produced %d, expected %d)%s%s"
                        % (len(pc), len(ec),
                           "; only in produced: %s" % only_p[:5] if only_p else "",
                           "; only in expected: %s" % only_e[:5] if only_e else ""))
        return problems
    if len(pr) != len(er):
        problems.append("row count differs: produced %d, expected %d" % (len(pr), len(er)))
        return problems

    # Track the worst difference per column, not just overall: when a solve is
    # ill-conditioned it is usually a handful of related coordinates that move,
    # and knowing which ones is the difference between a real regression and a
    # known-unstable joint.
    per_column = [0.0] * len(pc)
    worst_row = [-1] * len(pc)
    for r, (a, b) in enumerate(zip(pr, er)):
        for j, (x, y) in enumerate(zip(a, b)):
            d = abs(x - y)
            if d > per_column[j]:
                per_column[j], worst_row[j] = d, r

    ranked = sorted(zip(per_column, pc, worst_row), reverse=True)
    worst, where, at_row = ranked[0]
    if worst > tol:
        problems.append("max |difference| = %.6e in column '%s' at row %d (tolerance %.1e)"
                        % (worst, where, at_row, tol))
        over = [t for t in ranked if t[0] > tol]
        problems.append("%d of %d columns exceed the tolerance; worst offenders:"
                        % (len(over), len(pc)))
        for d, name, row in over[:8]:
            problems.append("    %-28s %.6e (row %d)" % (name, d, row))
    return problems


def run_solver(exe, setup, opensim_bin=None):
    """Run the solver, putting the OpenSim we were built against first on PATH.

    Windows resolves DLLs by PATH order, so a machine with several OpenSim
    installations will happily load the wrong ones into this binary and kill it
    right after the banner. CMake knows the correct directory, so the tests
    prepend it rather than depending on the developer's environment.
    """
    env = os.environ.copy()
    if opensim_bin:
        env["PATH"] = opensim_bin + os.pathsep + env.get("PATH", "")
    proc = subprocess.run([exe, "-S", setup], capture_output=True, text=True, env=env)
    return proc.returncode, (proc.stdout or "") + (proc.stderr or "")


def do_golden(args):
    code, output = run_solver(args.exe, args.setup, args.opensim_bin)
    if code != 0:
        print("FAIL: solver exited %d" % code)
        print(output[-2000:])
        return 1
    if not os.path.exists(args.produced):
        print("FAIL: solver produced no output at %s" % args.produced)
        return 1

    if args.regenerate:
        os.makedirs(os.path.dirname(args.expected), exist_ok=True)
        shutil.copyfile(args.produced, args.expected)
        print("regenerated %s" % args.expected)
        return 0

    if not os.path.exists(args.expected):
        print("FAIL: no reference at %s (run with --regenerate to create it)" % args.expected)
        return 1

    problems = compare(args.produced, args.expected, args.tol)
    if problems:
        print("FAIL: %s" % args.setup)
        for p in problems:
            print("  " + p)
        return 1
    print("ok: matches %s within %.1e" % (os.path.basename(args.expected), args.tol))
    return 0


def do_error(args):
    code, output = run_solver(args.exe, args.setup, args.opensim_bin)
    failures = []
    if args.expect_code is not None and code != args.expect_code:
        failures.append("expected exit code %d, got %d" % (args.expect_code, code))
    if args.expect_text and args.expect_text not in output:
        failures.append("expected text not found: %r" % args.expect_text)
    if failures:
        print("FAIL: %s" % args.setup)
        for f in failures:
            print("  " + f)
        print("--- output ---")
        print(output[-2000:])
        return 1
    print("ok: %s" % os.path.basename(args.setup))
    return 0


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True, help="path to the solver executable")
    p.add_argument("--opensim-bin", default=None,
                   help="OpenSim bin directory to put first on PATH")
    sub = p.add_subparsers(dest="mode", required=True)

    g = sub.add_parser("golden", help="compare a solve against a committed reference")
    g.add_argument("--setup", required=True)
    g.add_argument("--produced", required=True, help="the .sto the setup file writes")
    g.add_argument("--expected", required=True, help="the committed reference .sto")
    g.add_argument("--tol", type=float, default=1e-6)
    g.add_argument("--regenerate", action="store_true",
                   help="overwrite the reference with the produced output")
    g.set_defaults(func=do_golden)

    e = sub.add_parser("error", help="assert on exit code and output of a bad setup")
    e.add_argument("--setup", required=True)
    e.add_argument("--expect-code", type=int, default=None)
    e.add_argument("--expect-text", default=None)
    e.set_defaults(func=do_error)

    args = p.parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
