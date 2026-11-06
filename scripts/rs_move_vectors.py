#!/usr/bin/env python3
"""Check the retained MotorControl-RS common-intent vectors against RS source.

Optional audit tool for test/vectors/rs_common_intent.inc. Normal builds, tests and
package consumers never run it. It reads MotorControl-RS read-only: by default the
retained RS_MOVE_REVISION through `git archive` (no checkout), with --revision another
commit, or with --worktree the current files. It compiles scripts/rs_move_vectors.cpp
against that source, runs it on the retained inputs, and compares the inputs, RS's
outputs and the compiled RS source closure (paths and Git blob IDs) with the file.

Any difference is an audit difference: it is printed and the exit status is 1. The
retained file is never rewritten. --candidate writes a complete replacement to another
path for review. Exit status 2 means the check could not run (no checkout, git or
compiler, or a build failure).
"""

import argparse
import io
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VECTORS = ROOT / "test/vectors/rs_common_intent.inc"
PRODUCER = ROOT / "scripts/rs_move_vectors.cpp"
ROW = re.compile(r"^(RS_MOVE_[A-Z_]+)\((.*)\)\s*$")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
IDENT = re.compile(r"^[A-Z][A-Z_]*$|^true$|^false$")
INT = re.compile(r"^-?[0-9]+$")
COLUMNS = {
    "RS_MOVE_REVISION": ("revision",),
    "RS_MOVE_SOURCE": ("path", "blob"),
    "RS_MOVE_AXIS": ("name", "steps", "stepsDenominator", "gear", "gearDenominator",
                     "fullSteps", "fullStepsDenominator", "origin"),
    "RS_MOVE_VECTOR": ("name", "axis", "numerator", "denominator", "unit", "frame", "relative",
                       "rounding", "allowanceHalves", "witness", "effective", "endpoint",
                       "errorSixths"),
    "RS_MOVE_REFUSAL": ("name", "axis", "numerator", "denominator", "unit", "frame", "relative",
                        "rounding", "allowanceHalves", "witness", "code", "detail"),
}
# Rows only the producer prints; the comparison names them instead of accepting them.
PRODUCED_ONLY = {"RS_MOVE_VECTOR_REFUSED": ("name", "code", "detail"), "RS_MOVE_ERROR": ("name", "message")}
INPUTS = 10  # Leading VECTOR/REFUSAL columns that are inputs, echoed back by the producer.


class ToolError(Exception):
    """The check could not run; distinct from an audit difference."""


def split_arguments(text):
    """Split one macro argument list on commas outside string literals."""
    tokens, current, quoted = [], "", False
    for ch in text:
        if ch == '"':
            quoted = not quoted
        if ch == "," and not quoted:
            tokens.append(current.strip())
            current = ""
        else:
            current += ch
    if quoted:
        raise ValueError("unterminated string")
    tokens.append(current.strip())
    return tokens


def value(token):
    if len(token) >= 2 and token[0] == token[-1] == '"':
        return token[1:-1]
    if INT.match(token):
        return int(token)
    if IDENT.match(token):
        return token
    raise ValueError(f"unexpected token {token!r}")


def parse_rows(text, kinds):
    """Return (kind, values, line number) for every row line; comments and directives are skipped."""
    rows = []
    for number, line in enumerate(text.splitlines(), 1):
        match = ROW.match(line.strip())
        if not match:
            continue
        kind = match.group(1)
        if kind not in kinds:
            raise ValueError(f"line {number}: unknown row {kind}")
        try:
            values = [value(token) for token in split_arguments(match.group(2))]
        except ValueError as error:
            raise ValueError(f"line {number}: {error}") from None
        if len(values) != len(kinds[kind]):
            raise ValueError(f"line {number}: {kind} needs {len(kinds[kind])} columns, has {len(values)}")
        rows.append((kind, values, number))
    return rows


class Retained:
    """The retained file, validated for shape only; its values are what the audit compares."""

    def __init__(self, text):
        self.text = text
        self.revision = None
        self.sources, self.axes, self.cases = {}, {}, {}
        for kind, values, number in parse_rows(text, COLUMNS):
            if kind == "RS_MOVE_REVISION":
                if self.revision or not isinstance(values[0], str) or not HEX40.match(values[0]):
                    raise ValueError(f"line {number}: one full 40-digit RS_MOVE_REVISION is required")
                self.revision = values[0]
            elif kind == "RS_MOVE_SOURCE":
                path, blob = values
                if path in self.sources or not isinstance(blob, str) or not HEX40.match(blob):
                    raise ValueError(f"line {number}: duplicate source or invalid blob")
                self.sources[path] = blob
            elif kind == "RS_MOVE_AXIS":
                if values[0] in self.axes or not all(isinstance(v, int) for v in values[1:]):
                    raise ValueError(f"line {number}: duplicate or malformed axis")
                if values[2] <= 0 or values[4] <= 0 or values[6] <= 0:
                    raise ValueError(f"line {number}: denominators must be positive")
                self.axes[values[0]] = values
            else:
                name = values[0]
                if name in self.cases:
                    raise ValueError(f"line {number}: duplicate case {name!r}")
                if not isinstance(values[2], int) or not isinstance(values[3], int) or values[3] <= 0:
                    raise ValueError(f"line {number}: the value needs an integer numerator and positive denominator")
                self.cases[name] = (kind, values)
        if not self.revision or not self.sources or not self.axes or not self.cases:
            raise ValueError("the retained file needs a revision, sources, axes and cases")
        for name, (_, values) in self.cases.items():
            if values[1] not in self.axes:
                raise ValueError(f"case {name!r} names unknown axis {values[1]!r}")
        if not any(path.endswith(".cpp") for path in self.sources):
            raise ValueError("no RS translation unit is recorded")

    def translation_units(self):
        return sorted(path for path in self.sources if path.endswith(".cpp"))


def compare_cases(retained, produced):
    """Compare producer rows with the retained cases; return audit differences."""
    differences, seen = [], set()
    for kind, values, _ in produced:
        name = values[0]
        if name in seen:
            differences.append(f"{name!r}: the producer printed it twice")
            continue
        seen.add(name)
        if name not in retained.cases:
            differences.append(f"{name!r}: the producer printed a case the file does not retain")
            continue
        expected_kind, expected = retained.cases[name]
        if kind == "RS_MOVE_ERROR":
            differences.append(f"{name!r}: {values[1]}")
        elif kind == "RS_MOVE_VECTOR_REFUSED":
            differences.append(f"{name!r}: retained as accepted, RS refuses it with {values[1]} detail {values[2]}")
        elif kind != expected_kind:
            differences.append(f"{name!r}: retained as {expected_kind}, produced as {kind}")
        else:
            for column, old, new in zip(COLUMNS[kind], expected, values):
                if old != new:
                    role = "input echo" if COLUMNS[kind].index(column) < INPUTS else "RS output"
                    differences.append(f"{name!r} {column} ({role}): retained {old!r}, RS {new!r}")
    for name in retained.cases:
        if name not in seen:
            differences.append(f"{name!r}: the producer printed no row")
    return differences


def compare_sources(recorded, closure, blobs):
    """Compare the recorded RS source closure with the compiled one; return audit differences."""
    differences = []
    for path in sorted(set(recorded) | set(closure)):
        if path not in closure:
            differences.append(f"source {path}: recorded but not compiled")
        elif path not in recorded:
            differences.append(f"source {path}: compiled but not recorded (blob {blobs.get(path, 'unknown')})")
        elif blobs.get(path) != recorded[path]:
            differences.append(f"source {path}: recorded blob {recorded[path]}, compiled {blobs.get(path, 'missing')}")
    return differences


def dependencies(text, base):
    """RS paths, relative to base, named by one GCC -MMD file."""
    body = text.replace("\\\r\n", " ").replace("\\\n", " ").replace("\\ ", "\0")
    _, _, items = body.partition(": ")
    paths = set()
    for item in items.split():
        path = Path(item.replace("\0", " "))
        if not path.is_absolute():
            path = (base / path)
        try:
            paths.add(path.resolve().relative_to(base.resolve()).as_posix())
        except ValueError:
            continue
    return paths


def candidate_text(retained, revision, closure, blobs, produced):
    """A complete replacement for review. Every row is replaced; comments are kept verbatim."""
    lines, sources_written = [], False
    by_name = {values[0]: (kind, values) for kind, values, _ in produced}
    for line in retained.text.splitlines():
        match = ROW.match(line.strip())
        if not match:
            lines.append(line)
            continue
        kind = match.group(1)
        if kind == "RS_MOVE_REVISION":
            lines.append(f'RS_MOVE_REVISION("{revision}")')
        elif kind == "RS_MOVE_SOURCE":
            if not sources_written:
                lines.extend(f'RS_MOVE_SOURCE("{path}", "{blobs[path]}")' for path in sorted(closure))
                sources_written = True
        elif kind == "RS_MOVE_AXIS":
            lines.append(line)
        else:
            name = split_arguments(match.group(2))[0][1:-1]
            produced_kind, values = by_name[name]
            lines.append(format_row(produced_kind, values))
    return "\n".join(lines) + "\n"


def format_row(kind, values):
    """One VECTOR or REFUSAL row: name and axis quoted, numbers and enumerators bare."""
    texts = [f'"{v}"' if column < 2 else str(v) for column, v in enumerate(values)]
    return f"{kind}({', '.join(texts)})"


def git(rs, *arguments, binary=False):
    try:
        result = subprocess.run(["git", "-C", str(rs), "--no-optional-locks", *arguments],
                                capture_output=True, check=False)
    except OSError as error:
        raise ToolError(f"git is unavailable: {error}") from None
    if result.returncode:
        raise ToolError(f"git {' '.join(arguments)} failed: {result.stderr.decode(errors='replace').strip()}")
    return result.stdout if binary else result.stdout.decode()


def safe_extract(archive, destination):
    with tarfile.open(fileobj=archive, mode="r:") as tar:
        for member in tar.getmembers():
            name = Path(member.name)
            if name.is_absolute() or ".." in name.parts or not (member.isfile() or member.isdir()):
                raise ToolError(f"unexpected archive member {member.name}")
        tar.extractall(destination, **({"filter": "data"} if hasattr(tarfile, "data_filter") else {}))


def acquire_revision(rs, revision, destination):
    """Extract include/ and src/ at a commit; return (commit, blob map). Nothing in RS changes."""
    commit = git(rs, "rev-parse", "--verify", f"{revision}^{{commit}}").strip()
    safe_extract(io.BytesIO(git(rs, "archive", "--format=tar", commit, "--", "include", "src", binary=True)),
                 destination)
    blobs = {}
    for line in git(rs, "ls-tree", "-r", "--full-tree", commit, "--", "include", "src").splitlines():
        meta, _, path = line.partition("\t")
        blobs[path] = meta.split()[2]
    return commit, blobs


def acquire_worktree(rs, destination):
    """Copy the current include/ and src/; return (HEAD, blob map, dirty lines). Nothing in RS changes."""
    head = git(rs, "rev-parse", "HEAD").strip()
    dirty = git(rs, "status", "--porcelain=v1", "--untracked-files=all", "--", "include", "src").splitlines()
    for part in ("include", "src"):
        shutil.copytree(rs / part, destination / part)
    files = sorted(p.relative_to(destination).as_posix() for p in destination.rglob("*") if p.is_file())
    hashes = git(rs, "hash-object", "--", *files).split() if files else []
    return head, dict(zip(files, hashes)), dirty


def build_and_run(compiler, source, retained, build):
    """Compile each recorded RS translation unit and the producer; return (closure, stdout)."""
    # The producer includes exactly the checked text. POSIX spellings keep GCC's -MMD output
    # parseable on Windows.
    (build / VECTORS.name).write_text(retained.text, encoding="utf-8")
    flags = ["-std=c++11", "-O1", "-Wall", "-Wextra", "-I", (source / "include").as_posix(),
             "-I", (source / "src").as_posix(), "-I", build.as_posix()]
    units = [PRODUCER] + [source / path for path in retained.translation_units()]
    objects, closure = [], set()
    for index, unit in enumerate(units):
        if not unit.is_file():
            raise ToolError(f"missing translation unit {unit}")
        obj, dep = build / f"unit{index}.o", build / f"unit{index}.d"
        run([compiler, *flags, "-MMD", "-MF", dep.as_posix(), "-c", unit.as_posix(), "-o", obj.as_posix()],
            "compile " + unit.name)
        closure |= dependencies(dep.read_text(encoding="utf-8", errors="replace"), source)
        objects.append(str(obj))
    executable = build / ("rs_move_vectors" + (".exe" if os.name == "nt" else ""))
    run([compiler, *objects, "-o", str(executable)], "link")
    result = subprocess.run([str(executable)], capture_output=True, text=True, check=False)
    if result.returncode not in (0, 3):
        raise ToolError(f"producer exited {result.returncode}: {result.stderr.strip()}")
    return closure, result.stdout


def run(command, what):
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=False)
    except OSError as error:
        raise ToolError(f"{what}: {error}") from None
    if result.returncode:
        raise ToolError(f"{what} failed:\n{result.stdout}{result.stderr}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rs", required=True, type=Path, help="MotorControl-RS Git checkout, read only")
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--revision", help="RS commit to compare with (default: the retained RS_MOVE_REVISION)")
    where.add_argument("--worktree", action="store_true", help="compare with the checkout's current files")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"), help="C++11 compiler (default: $CXX or g++)")
    parser.add_argument("--vectors", type=Path, default=VECTORS, help=argparse.SUPPRESS)
    parser.add_argument("--candidate", type=Path, help="write a reviewed replacement here; never the retained file")
    parser.add_argument("--keep", type=Path, help="keep the extracted source and build in this new directory")
    args = parser.parse_args(argv)
    try:
        retained = Retained(args.vectors.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, ValueError) as error:
        print(f"rs_move_vectors: cannot read {args.vectors}: {error}", file=sys.stderr)
        return 2
    if args.candidate and args.candidate.resolve() == args.vectors.resolve():
        print("rs_move_vectors: --candidate must not replace the retained file; review and replace it by hand",
              file=sys.stderr)
        return 2
    if not (args.rs / ".git").exists():
        print(f"rs_move_vectors: {args.rs} is not a MotorControl-RS Git checkout", file=sys.stderr)
        return 2
    if args.keep and args.keep.exists():
        print(f"rs_move_vectors: --keep {args.keep} already exists", file=sys.stderr)
        return 2
    work = Path(tempfile.mkdtemp(prefix="rs_move_vectors_")) if not args.keep else args.keep
    try:
        source, build = work / "rs", work / "build"
        source.mkdir(parents=True)
        build.mkdir()
        head = git(args.rs, "rev-parse", "HEAD").strip()
        dirty = []
        if args.worktree:
            commit, blobs, dirty = acquire_worktree(args.rs, source)
            label = f"worktree at {commit}" + (" with local changes" if dirty else ", clean")
        else:
            commit, blobs = acquire_revision(args.rs, args.revision or retained.revision, source)
            label = f"commit {commit}"
        closure, output = build_and_run(args.compiler, source, retained, build)
        produced = parse_rows(output, {**COLUMNS, **PRODUCED_ONLY})
    except (ToolError, OSError, ValueError) as error:
        print(f"rs_move_vectors: {error}", file=sys.stderr)
        return 2
    finally:
        if not args.keep:
            shutil.rmtree(work, ignore_errors=True)
    print(f"RS checkout {args.rs} HEAD {head}")
    print(f"retained RS_MOVE_REVISION {retained.revision}; compared with {label}")
    for line in dirty:
        print(f"RS local change: {line}")
    differences = compare_sources(retained.sources, closure, blobs) + compare_cases(retained, produced)
    if dirty:
        differences.append("the RS worktree has local changes under include/ or src/")
    if args.candidate:
        if dirty or any(kind in PRODUCED_ONLY or kind != retained.cases[values[0]][0]
                        for kind, values, _ in produced if values[0] in retained.cases):
            print("rs_move_vectors: no candidate; a dirty source or a changed row kind needs a manual decision",
                  file=sys.stderr)
        else:
            args.candidate.write_text(candidate_text(retained, commit, closure, blobs, produced), encoding="utf-8")
            print(f"candidate written to {args.candidate}; update its History comment before any replacement")
    for difference in differences:
        print(f"AUDIT DIFFERENCE: {difference}")
    if differences:
        print(f"{len(differences)} audit difference(s); {args.vectors} is unchanged")
        return 1
    print(f"{len(retained.cases)} cases and {len(retained.sources)} RS sources match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
