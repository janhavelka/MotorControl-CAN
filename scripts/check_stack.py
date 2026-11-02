#!/usr/bin/env python3
"""Measure target stack frames and known call chains; never flashes or opens a port.

Compiles the portable core and the application sources that need no SDK with the
pinned Xtensa compilers, at both optimization levels the firmware can use, and
parses GCC's -fstack-usage and -fcallgraph-info=su output. The limits below are
project limits. A chain total is a lower bound whenever its reachable graph holds
an unknown edge, indirect call, dynamic frame or recursion; it is reported that way.

Exit codes: 0 passed, 1 failed or a required compiler is unavailable, 2 usage,
77 skipped because a compiler is missing locally. A skip is not a pass.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FRAME_LIMIT = 2048
CHAIN_LIMIT = 6144
EXIT_PASSED = 0
EXIT_FAILED = 1
EXIT_SKIPPED = 77
TARGETS = ("esp32s2", "esp32s3")
PINNED_COMPILER = "esp-14.2.0_20260121"  # tools/tools.json of the reviewed ESP-IDF 5.5.5.
DEFAULT_BUILD = ROOT / ".build/stack"
INDIRECT = "__indirect_call"
TOP_ROWS = 10
REPORT_ROWS = 25

# Options from the real IDF compile database and toolchain response files of the
# probe_idf and s2_bench projects. IDF appends -std=gnu++2b after the component's
# -std=c++11, so gnu++2b is the effective language mode of the firmware.
DEFINES = ["-DESP_PLATFORM", "-D_GLIBCXX_HAVE_POSIX_SEMAPHORE", "-D_GLIBCXX_USE_POSIX_SEMAPHORE",
           "-D_GNU_SOURCE", "-D_POSIX_READER_WRITER_LOCKS"]
RESPONSE_FLAGS = ["-mlongcalls", "-fno-builtin-memcpy", "-fno-builtin-memset", "-fno-builtin-bzero"]
SECTION_FLAGS = ["-ffunction-sections", "-fdata-sections", "-gdwarf-4", "-ggdb"]
TAIL_FLAGS = ["-fstrict-volatile-bitfields", "-fno-jump-tables", "-fno-tree-switch-conversion"]
OPT_FLAGS = {"-Os": ["-Os", "-freorder-blocks"], "-Og": ["-Og", "-fno-shrink-wrap"]}
MEASURE_FLAGS = ["-fstack-usage", "-fcallgraph-info=su"]
DRIFT_SAMPLES = ("src/profiles/lichuan_c/Actions.cpp",
                 "experiments/canopen_service_fit/vendor/CANopenNode/301/CO_SDOclient.c")
DRIFT_IGNORED = ("-fdiagnostics-color", "-fmacro-prefix-map", "-fstack-usage", "-fcallgraph-info")

SOURCE_GLOBS = ("src/**/*.cpp", "adapters/owner/*.cpp", "adapters/canopennode/*.cpp",
                "adapters/twai/*.cpp", "examples/common/*.cpp")
HEADER_GLOBS = ("include/**/*.h", "adapters/**/*.h", "examples/common/*.h",
                "experiments/canopen_service_fit/vendor/CANopenNode/**/*.h")
SDK_ONLY = {
    "adapters/twai/IdfTwaiDriver.cpp": "includes ESP-IDF TWAI driver headers",
    "examples/common/EspProbeMain.cpp": "includes FreeRTOS and esp_attr headers",
    "examples/common/EspProbePlatform.cpp": "includes FreeRTOS, esp_timer and console driver headers",
}
CONTEXT_TYPES = (
    ("ActionContext", "MotorControlCAN/profiles/lichuan_c/Actions.h", "MotorControlCAN::LICHUAN_C::ActionContext"),
    ("ReadContext", "MotorControlCAN/profiles/lichuan_c/Reads.h", "MotorControlCAN::LICHUAN_C::ReadContext"),
    ("BestEffortStop", "MotorControlCAN/profiles/lichuan_c/Actions.h", "MotorControlCAN::LICHUAN_C::BestEffortStop"),
    ("SdoCompletion", "MotorControlCAN/canopen/SdoTransfer.h", "MotorControlCAN::CANOPEN::SdoCompletion"),
    ("AxisSession", "AxisSession.h", "MotorControlCANExample::AxisSession"),
    ("ProbeApp", "ProbeApp.h", "MotorControlCANExample::ProbeApp"),
)

STRING = r'((?:[^"\\]|\\.)*)'
SU_ROW = re.compile(r"^(.*?):(\d+):(\d+):(.*)\t(\d+)\t([a-z,]+)\s*$")
CI_NODE = re.compile(r'^node:\s*\{\s*title:\s*"' + STRING + r'"\s+label:\s*"' + STRING + r'"(.*)\}\s*$')
CI_EDGE = re.compile(r'^edge:\s*\{\s*sourcename:\s*"' + STRING + r'"\s+targetname:\s*"' + STRING +
                     r'"(?:\s+label:\s*"' + STRING + r'")?\s*\}\s*$')
CI_FRAME = re.compile(r"^(\d+) bytes \(([a-z,]+)\)$")


class StackCheckError(RuntimeError):
    """A measurement or input problem; the run fails and never reports a pass."""


class Node:
    """One function in the merged call graph. A frame of None means no known definition."""

    def __init__(self, title, name=None, location=""):
        self.title = title
        self.name = name or title
        self.location = location
        self.frame = None
        self.qualifier = None
        self.described = name is not None

    @property
    def file(self):
        return normalize_path(self.location.rsplit(":", 2)[0]) if self.location else ""

    def add_frame(self, frame, qualifier):
        # Clones and link-once copies share one identity; keep the largest and any dynamic mark.
        self.frame = frame if self.frame is None else max(self.frame, frame)
        if self.qualifier is None or qualifier != "static":
            self.qualifier = qualifier


class Graph:
    """Call graph merged across translation units for one target and optimization level."""

    def __init__(self):
        self.nodes = {}
        self.callees = {}
        self.indirect_sites = {}
        self.unlocated_indirect = {}
        self.unmatched_frames = []
        self.missing_sources = set()

    def node(self, title, name=None, location=""):
        node = self.nodes.get(title)
        if node is None:
            node = self.nodes[title] = Node(title, name, location)
        elif name is not None and not node.described:
            node.name, node.location, node.described = name, location, True
        return node


def normalize_path(path):
    """Make a location independent of the host: repository-relative or marked external."""
    text = path.replace("\\", "/")
    if text.startswith("<"):
        return text
    if re.match(r"^(?:[A-Za-z]:)?/", text):
        try:
            return Path(text).resolve().relative_to(ROOT).as_posix()
        except (ValueError, OSError):
            return "<external>/" + text.rsplit("/", 1)[-1]
    return text


def clone_short(symbol):
    """Readable form of a GCC clone symbol such as _ZN...E$part$0; only names are decoded."""
    base, _, suffix = symbol.partition("$")
    parts = []
    position = re.match(r"_Z(?:N[KVR]*|L)?", base)
    position = position.end() if position else 0
    while True:
        digits = re.match(r"\d+", base[position:])
        if not digits:
            break
        start = position + digits.end()
        parts.append(base[start:start + int(digits.group())])
        position = start + int(digits.group())
    name = "::".join(part for part in parts if not part.startswith("_GLOBAL__N")) or base
    return name.replace("MotorControlCAN::", "").replace("MotorControlCANExample::", "") + "." + suffix.split("$")[0]


def short_name(name):
    """Drop the return type and parameters from a printed signature for narrow tables."""
    if "$" in name:
        return clone_short(name)
    depth = 0
    start = 0
    for index, char in enumerate(name):
        if char in "<{":
            depth += 1
        elif char in ">}":
            depth -= 1
        elif char == " " and depth == 0:
            start = index + 1
        elif char == "(" and depth == 0:
            if name[start:index].endswith("operator"):
                continue
            return name[start:index].replace("MotorControlCAN::", "").replace("MotorControlCANExample::", "")
    return name


def parse_su(text, origin="<su>"):
    """Return frame rows; an unrecognized line fails rather than hiding a frame."""
    rows = []
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        match = SU_ROW.match(line)
        if not match:
            raise StackCheckError("{}:{}: unrecognized stack usage row: {}".format(origin, number, line[:160]))
        path, row, column, name, frame, qualifier = match.groups()
        rows.append({"location": "{}:{}:{}".format(path.replace("\\", "/"), row, column), "name": name,
                     "frame": int(frame), "qualifier": qualifier})
    return rows


def parse_ci(text, origin="<ci>"):
    """Return call graph nodes and edges from GCC's VCG output."""
    nodes = []
    edges = []
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped == "}" or stripped.startswith("graph:"):
            continue
        match = CI_NODE.match(stripped)
        if match:
            title, label, rest = match.groups()
            parts = label.split("\\n")
            # GCC prints some clones with only "constprop" as a name. The symbol tells them apart.
            symbol = title.rsplit(":", 1)[-1]
            node = {"title": title, "printed": parts[0], "name": symbol if "$" in symbol else parts[0],
                    "location": parts[1].replace("\\\\", "/").replace("\\", "/") if len(parts) > 1 else "",
                    "external": "ellipse" in rest, "frame": None, "qualifier": None}
            for part in parts[2:]:
                frame = CI_FRAME.match(part)
                if frame:
                    node["frame"], node["qualifier"] = int(frame.group(1)), frame.group(2)
            nodes.append(node)
            continue
        match = CI_EDGE.match(stripped)
        if match:
            edges.append({"source": match.group(1), "target": match.group(2), "site": match.group(3) or ""})
            continue
        raise StackCheckError("{}:{}: unrecognized call graph row: {}".format(origin, number, stripped[:160]))
    return nodes, edges


def build_graph(units):
    """Merge parsed (su_rows, nodes, edges) units; link-once functions unify by title."""
    graph = Graph()
    for su_rows, nodes, edges in units:
        located = {}
        for item in nodes:
            if item["title"] == INDIRECT:
                continue
            if item["external"] or item["frame"] is None:
                # An external declaration names the target without defining it.
                graph.node(item["title"], item["name"], item["location"])
                continue
            node = graph.node(item["title"], item["name"], item["location"])
            node.name, node.location, node.described = item["name"], item["location"], True
            node.add_frame(item["frame"], item["qualifier"])
            located.setdefault((item["location"], item["printed"]), node)
        for row in su_rows:
            node = located.get((row["location"], row["name"]))
            if node is not None:
                node.add_frame(row["frame"], row["qualifier"])
                continue
            # The frame still counts against the budget, but no edges are known for it.
            title = "su:{}:{}".format(row["location"], row["name"])
            graph.node(title, row["name"], row["location"]).add_frame(row["frame"], row["qualifier"])
            graph.unmatched_frames.append(title)
        for edge in edges:
            if edge["source"] not in graph.nodes:
                graph.node(edge["source"])
                graph.missing_sources.add(edge["source"])
            if edge["target"] == INDIRECT:
                # Located records matched callx8 one for one in generated assembly. -Os also
                # emits unlocated records with no callx8 behind them; keep those visible apart.
                sites = graph.indirect_sites if edge["site"] else graph.unlocated_indirect
                sites.setdefault(edge["source"], []).append(edge["site"])
            else:
                graph.callees.setdefault(edge["source"], set()).add(edge["target"])
    return graph


def strongly_connected(titles, adjacency):
    """Iterative Tarjan; components come out callees first."""
    index = {}
    low = {}
    on_stack = set()
    stack = []
    components = []
    counter = 0
    for root in titles:
        if root in index:
            continue
        work = [(root, iter(adjacency.get(root, ())))]
        index[root] = low[root] = counter
        counter += 1
        stack.append(root)
        on_stack.add(root)
        while work:
            title, children = work[-1]
            advanced = False
            for child in children:
                if child not in index:
                    index[child] = low[child] = counter
                    counter += 1
                    stack.append(child)
                    on_stack.add(child)
                    work.append((child, iter(adjacency.get(child, ()))))
                    advanced = True
                    break
                if child in on_stack:
                    low[title] = min(low[title], index[child])
            if advanced:
                continue
            work.pop()
            if work:
                parent = work[-1][0]
                low[parent] = min(low[parent], low[title])
            if low[title] == index[title]:
                component = []
                while True:
                    member = stack.pop()
                    on_stack.discard(member)
                    component.append(member)
                    if member == title:
                        break
                components.append(sorted(component))
    return components


def resolve_alias(target, defined):
    """Find a target's definition. GCC defines C2/D2 and calls the C1/D1 alias of the same code."""
    if target in defined:
        return target
    for match in re.finditer(r"([CD])1E", target):
        candidate = target[:match.start()] + match.group(1) + "2E" + target[match.end():]
        if candidate in defined:
            return candidate
    return None


def analyze(graph, frame_limit=FRAME_LIMIT, chain_limit=CHAIN_LIMIT):
    """Check frames and known chains; any incompleteness stays attached to the chain it affects."""
    defined = {title: node for title, node in graph.nodes.items() if node.frame is not None}
    adjacency = {title: set() for title in defined}
    unknown_by_source = {}
    unknown_callers = {}
    for source, targets in graph.callees.items():
        for target in targets:
            resolved = resolve_alias(target, defined)
            if resolved is not None:
                if source in adjacency:
                    adjacency[source].add(resolved)
                continue
            node = graph.nodes.get(target)
            label = short_name(node.name) if node is not None and node.described else target
            unknown_by_source.setdefault(source, set()).add(label)
            unknown_callers.setdefault(label, set()).add(source)
    adjacency = {title: sorted(adjacency[title]) for title in sorted(adjacency)}

    components = strongly_connected(sorted(defined), adjacency)
    owner = {title: number for number, component in enumerate(components) for title in component}
    totals = []
    following = []
    reach_unknown = []
    reach_indirect = []
    reach_dynamic = []
    reach_recursion = []
    recursion = []
    for number, component in enumerate(components):
        cyclic = len(component) > 1 or component[0] in adjacency[component[0]]
        if cyclic:
            recursion.append([short_name(defined[t].name) for t in component])
        best = None
        unknown = set()
        indirect = set()
        dynamic = set()
        cycles = set()
        if cyclic:
            cycles.update(short_name(defined[t].name) for t in component)
        for title in component:
            unknown |= unknown_by_source.get(title, set())
            if title in graph.indirect_sites:
                indirect.add(short_name(defined[title].name))
            elif title in graph.unlocated_indirect:
                indirect.add(short_name(defined[title].name) + " [unlocated record]")
            if defined[title].qualifier != "static":
                dynamic.add(short_name(defined[title].name))
            for target in adjacency[title]:
                child = owner[target]
                if child == number:
                    continue
                if best is None or totals[child] > totals[best]:
                    best = child
                unknown |= reach_unknown[child]
                indirect |= reach_indirect[child]
                dynamic |= reach_dynamic[child]
                cycles |= reach_recursion[child]
        totals.append(sum(defined[t].frame for t in component) + (totals[best] if best is not None else 0))
        following.append(best)
        reach_unknown.append(unknown)
        reach_indirect.append(indirect)
        reach_dynamic.append(dynamic)
        reach_recursion.append(cycles)

    def frame_row(node):
        return {"function": node.name, "file": node.file, "location": node.location,
                "bytes": node.frame, "qualifier": node.qualifier}

    def chain_row(title):
        number = owner[title]
        path = [title] + [t for t in components[number] if t != title]
        step = following[number]
        while step is not None:
            path.extend(components[step])
            step = following[step]
        complete = not (reach_unknown[number] or reach_indirect[number] or
                        reach_dynamic[number] or reach_recursion[number] or title in graph.unmatched_frames)
        return {"function": defined[title].name, "file": defined[title].file,
                "location": defined[title].location, "bytes": totals[number],
                "bound": "complete" if complete else "lower_bound",
                "path": [{"function": short_name(defined[t].name), "bytes": defined[t].frame} for t in path],
                "unknown_edges": sorted(reach_unknown[number]),
                "indirect_callers": sorted(reach_indirect[number]),
                "dynamic_frames": sorted(reach_dynamic[number]),
                "recursion": sorted(reach_recursion[number])}

    by_frame = sorted(defined.values(), key=lambda n: (-n.frame, n.name, n.location))
    by_chain = sorted(defined, key=lambda t: (-totals[owner[t]], defined[t].name, t))
    chain_rows = [chain_row(title) for title in by_chain]
    distinct = []
    seen_paths = []
    for row in chain_rows:
        # A callee's chain repeats the tail of its caller's; tables keep the longest form.
        names = [step["function"] for step in row["path"]]
        if not any(names == earlier[-len(names):] for earlier in seen_paths):
            distinct.append(row)
            seen_paths.append(names)
    return {
        "functions": len(defined),
        "known_edges": sum(len(targets) for targets in adjacency.values()),
        "frames": [frame_row(node) for node in by_frame],
        "chains": chain_rows,
        "distinct_chains": distinct,
        "frame_breaches": [frame_row(node) for node in by_frame if node.frame > frame_limit],
        "chain_breaches": [row for row in chain_rows if row["bytes"] > chain_limit],
        "dynamic_frames": [frame_row(node) for node in by_frame if node.qualifier != "static"],
        "recursion": recursion,
        "indirect_calls": sorted(({"function": short_name(graph.nodes[source].name), "sites": len(sites)}
                                  for source, sites in graph.indirect_sites.items()),
                                 key=lambda item: (-item["sites"], item["function"])),
        "unlocated_indirect_records": sum(len(sites) for sites in graph.unlocated_indirect.values()),
        "unknown_edges": sorted(({"target": label, "callers": len(callers)}
                                 for label, callers in unknown_callers.items()),
                                key=lambda item: (-item["callers"], item["target"])),
        "frames_without_graph_node": sorted(graph.unmatched_frames),
        "edge_sources_without_node": sorted(graph.missing_sources),
    }


def graph_gaps(analysis):
    """Name what prevents every chain total from being a complete bound."""
    return {"unknown_targets": len(analysis["unknown_edges"]),
            "indirect_call_sites": sum(item["sites"] for item in analysis["indirect_calls"]),
            "unlocated_indirect_records": analysis["unlocated_indirect_records"],
            "recursion_groups": len(analysis["recursion"]),
            "dynamic_frames": len(analysis["dynamic_frames"]),
            "frames_without_graph_node": len(analysis["frames_without_graph_node"]),
            "edge_sources_without_node": len(analysis["edge_sources_without_node"])}


def collect_breaches(measurements):
    """Flatten per-target breaches into the identities the baseline names."""
    breaches = {}
    for (target, opt), analysis in sorted(measurements.items()):
        for kind, rows in (("frame", analysis["frame_breaches"]), ("chain", analysis["chain_breaches"])):
            for row in rows:
                breach = {"target": target, "opt": opt, "kind": kind, "function": row["function"],
                          "file": row["file"], "bytes": row["bytes"]}
                if kind == "chain":
                    breach["bound"] = row["bound"]
                    breach["largest_frame"] = max(row["path"], key=lambda step: step["bytes"])
                # Rows are sorted largest first, so a repeated identity keeps the larger size.
                breaches.setdefault(breach_key(breach), breach)
    return list(breaches.values())


def breach_key(item, target=None):
    return (target if target is not None else item["target"], item["opt"], item["kind"],
            item["file"], item["function"])


def load_baseline(path):
    """Validate a baseline; an entry without a reason cannot silence a breach."""
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise StackCheckError("Cannot read stack baseline {}: {}".format(path, error))
    if not isinstance(data, dict) or not isinstance(data.get("entries"), list):
        raise StackCheckError("Stack baseline {} requires an entries list".format(path))
    keys = set()
    for number, entry in enumerate(data["entries"]):
        where = "{} entry {}".format(path, number)
        if not isinstance(entry, dict):
            raise StackCheckError(where + ": not an object")
        for field in ("function", "file", "reason"):
            if not isinstance(entry.get(field), str) or not entry[field].strip():
                raise StackCheckError("{}: {} must be a nonempty string".format(where, field))
        if entry.get("kind") not in ("frame", "chain"):
            raise StackCheckError(where + ": kind must be frame or chain")
        if entry.get("opt") not in OPT_FLAGS:
            raise StackCheckError(where + ": opt must be one of " + ", ".join(sorted(OPT_FLAGS)))
        targets = entry.get("targets")
        if not isinstance(targets, list) or not targets or any(t not in TARGETS for t in targets):
            raise StackCheckError(where + ": targets must list known targets")
        size = entry.get("bytes")
        if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
            raise StackCheckError(where + ": bytes must be a positive integer")
        for target in targets:
            key = breach_key(entry, target)
            if key in keys:
                raise StackCheckError("{}: duplicates {}".format(where, key))
            keys.add(key)
    return data["entries"]


def evaluate_baseline(breaches, entries, measured, strict=False):
    """Compare breaches to the allow list. Unlisted or grown breaches fail."""
    listed = {}
    for entry in entries:
        for target in entry["targets"]:
            listed[breach_key(entry, target)] = entry
    new = []
    grown = []
    reduced = []
    matched = []
    present = set()
    for breach in breaches:
        key = breach_key(breach)
        present.add(key)
        entry = listed.get(key)
        if entry is None:
            new.append(breach)
        elif breach["bytes"] > entry["bytes"]:
            grown.append(dict(breach, baseline_bytes=entry["bytes"]))
        elif breach["bytes"] < entry["bytes"]:
            reduced.append(dict(breach, baseline_bytes=entry["bytes"]))
        else:
            matched.append(breach)
    # A target or level that was not measured gives no evidence that an entry vanished.
    vanished = [{"target": key[0], "opt": key[1], "kind": key[2], "file": key[3], "function": key[4],
                 "baseline_bytes": entry["bytes"]}
                for key, entry in sorted(listed.items())
                if key not in present and (key[0], key[1]) in measured]
    strict_entries = list(entries) if strict else []
    return {"new": new, "grown": grown, "reduced": reduced, "vanished": vanished, "matched": matched,
            "strict_entries": strict_entries, "failed": bool(new or grown or strict_entries)}


def default_reason(breach):
    if breach["kind"] == "frame":
        return "Frame over the project limit, measured before the packet 8 refactor; packet 8 removes this entry."
    largest = breach["largest_frame"]
    return ("Known chain over the project limit, measured before the packet 8 refactor; largest frame on the "
            "path is {} at {} bytes. Packet 8 removes this entry.".format(largest["function"], largest["bytes"]))


def baseline_entries(breaches, previous=()):
    """Group identical per-target breaches and keep any reason a person already wrote."""
    reasons = {}
    for entry in previous:
        for target in entry["targets"]:
            reasons[breach_key(entry, target)] = entry["reason"]
    grouped = {}
    for breach in breaches:
        identity = (breach["opt"], breach["kind"], breach["file"], breach["function"], breach["bytes"])
        entry = grouped.setdefault(identity, {
            "function": breach["function"], "file": breach["file"], "opt": breach["opt"],
            "kind": breach["kind"], "targets": [], "bytes": breach["bytes"],
            "reason": default_reason(breach)})
        entry["targets"].append(breach["target"])
        if breach_key(breach) in reasons:
            entry["reason"] = reasons[breach_key(breach)]
    return sorted(grouped.values(), key=lambda e: (e["kind"] != "frame", -e["bytes"], e["function"], e["opt"]))


def write_baseline(path, breaches, previous=()):
    document = {
        "description": "Known stack budget breaches allowed by scripts/check_stack.py --allow-file. "
                       "Packet 8 drives this list to empty; --strict fails on any entry.",
        "limits": {"frame_bytes": FRAME_LIMIT, "chain_bytes": CHAIN_LIMIT},
        "entries": baseline_entries(breaches, previous)}
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8", newline="\n")
    return document["entries"]


class Compiler:
    """One pinned target compiler; evidence holds the identity recorded in the report."""

    def __init__(self, target, cxx, cc, evidence):
        self.target = target
        self.cxx = cxx
        self.cc = cc
        self.evidence = evidence


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def tool_output(command):
    result = subprocess.run([str(part) for part in command], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors="replace", timeout=60, check=False, stdin=subprocess.DEVNULL)
    if result.returncode != 0:
        raise OSError("{} returned {}".format(command[0], result.returncode))
    return result.stdout


def toolchain_directories(explicit=None):
    """An explicit directory alone, else PATH, then the usual PlatformIO package locations."""
    if explicit:
        return [str(explicit)]
    directories = [None]
    relative = "packages/toolchain-xtensa-esp-elf/bin"
    if os.environ.get("PLATFORMIO_CORE_DIR"):
        directories.append(str(Path(os.environ["PLATFORMIO_CORE_DIR"]) / relative))
    directories.append(str(Path.home() / ".platformio" / relative))
    if os.name == "nt":
        directories.append("C:/pio/" + relative)
    return directories


def find_compiler(target, explicit=None):
    """Return (compiler, reason). Only the pinned version counts; other versions give other frames."""
    rejected = []
    for directory in toolchain_directories(explicit):
        cxx = shutil.which("xtensa-{}-elf-g++".format(target), path=directory)
        cc = shutil.which("xtensa-{}-elf-gcc".format(target), path=directory)
        if not cxx or not cc:
            continue
        try:
            version = tool_output([cxx, "--version"]).splitlines()[0].strip()
            if PINNED_COMPILER not in version:
                rejected.append("{} is {}".format(cxx, version))
                continue
            evidence = {"path": str(cxx), "version": version, "sha256": sha256_file(cxx)}
            for program in ("cc1plus", "cc1"):
                located = tool_output([cxx, "-print-prog-name=" + program]).strip()
                if located and Path(located).is_file():
                    evidence[program + "_sha256"] = sha256_file(located)
            return Compiler(target, cxx, cc, evidence), None
        except (OSError, IndexError, subprocess.SubprocessError) as error:
            rejected.append("{}: {}".format(cxx, error))
    reason = "xtensa-{}-elf-g++ with {} not found".format(target, PINNED_COMPILER)
    if rejected:
        reason += "; rejected " + "; ".join(rejected)
    return None, reason


def firmware_inventory(root=ROOT):
    data = json.loads((root / "examples/common/firmware_sources.json").read_text(encoding="utf-8"))
    return data["sources"], data["include_dirs"]


def source_inventory(root=ROOT):
    """Return measured sources and SDK-bound sources; a firmware input cannot silently drop out."""
    inventory, include_dirs = firmware_inventory(root)
    sources = set()
    for pattern in SOURCE_GLOBS:
        sources.update(path.relative_to(root).as_posix() for path in root.glob(pattern))
    sources.update(path for path in inventory if path.endswith(".c"))
    not_measured = {path: reason for path, reason in SDK_ONLY.items() if path in sources}
    measured = sorted(sources - set(not_measured))
    missing = set(inventory) - set(measured) - set(not_measured)
    if missing:
        raise StackCheckError("Firmware inventory sources are not measured: " + ", ".join(sorted(missing)))
    return measured, not_measured, ["include"] + include_dirs


def compile_flags(source, opt, include_dirs):
    is_c = source.endswith(".c")
    language = (["-std=gnu99"] if is_c else ["-fno-rtti", "-std=c++11"])
    tail = (["-std=gnu17"] if is_c else ["-std=gnu++2b", "-fno-exceptions", "-fuse-cxa-atexit"])
    return (DEFINES + ["-I" + path for path in include_dirs] + RESPONSE_FLAGS + language + SECTION_FLAGS +
            OPT_FLAGS[opt] + TAIL_FLAGS + tail)


def compile_unit(compiler, source, opt, include_dirs, output_dir, number):
    """Compile one source from the repository root so titles stay host-independent."""
    stem = output_dir / "{:03d}_{}".format(number, Path(source).stem)
    program = compiler.cc if source.endswith(".c") else compiler.cxx
    command = [str(program)] + compile_flags(source, opt, include_dirs) + MEASURE_FLAGS + [
        "-c", source, "-o", str(stem) + ".o"]
    result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors="replace", timeout=600, check=False, stdin=subprocess.DEVNULL)
    if result.stdout.strip():
        Path(str(stem) + ".log").write_text(result.stdout, encoding="utf-8")
    if result.returncode != 0:
        raise StackCheckError("{} failed for {} {} {}:\n{}".format(
            Path(program).name, compiler.target, opt, source, result.stdout[-2000:]))
    su = Path(str(stem) + ".su")
    ci = Path(str(stem) + ".ci")
    if not su.is_file() or not ci.is_file():
        raise StackCheckError("Compiler wrote no stack output for {} {} {}".format(compiler.target, opt, source))
    return su, ci


def measure(compiler, opt, sources, include_dirs, build_dir, jobs):
    """Compile and parse every source for one target and level."""
    output_dir = Path(build_dir) / compiler.target / opt.lstrip("-")
    output_dir.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(compile_unit, compiler, source, opt, include_dirs, output_dir, number)
                   for number, source in enumerate(sources)]
        outputs = [future.result() for future in futures]
    units = []
    for su, ci in outputs:
        nodes, edges = parse_ci(ci.read_text(encoding="utf-8", errors="replace"), ci.name)
        units.append((parse_su(su.read_text(encoding="utf-8", errors="replace"), su.name), nodes, edges))
    graph = build_graph(units)
    analysis = analyze(graph)
    if not analysis["functions"]:
        raise StackCheckError("No functions measured for {} {}".format(compiler.target, opt))
    return analysis


def context_sizes(compiler, include_dirs, build_dir):
    """Read sizeof from generated assembly; a type another stage renamed reports as unavailable."""
    output_dir = Path(build_dir) / compiler.target / "sizes"
    output_dir.mkdir(parents=True, exist_ok=True)
    sizes = {}
    for label, header, qualified in CONTEXT_TYPES:
        source = output_dir / (label + ".cpp")
        source.write_text('#include "{}"\nextern const unsigned mccan_size;\n'
                          'const unsigned mccan_size = sizeof({});\n'.format(header, qualified), encoding="utf-8")
        assembly = output_dir / (label + ".s")
        command = [str(compiler.cxx)] + compile_flags("sizes.cpp", "-Os", include_dirs) + [
            "-S", str(source), "-o", str(assembly)]
        result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                errors="replace", timeout=300, check=False, stdin=subprocess.DEVNULL)
        match = None
        if result.returncode == 0:
            match = re.search(r"mccan_size:\s*\n\s*\.word\s+(\d+)", assembly.read_text(encoding="utf-8", errors="replace"))
        sizes[label] = int(match.group(1)) if match else None
    return sizes


def effective_flags(command):
    """Frame-relevant options from one database command, with response files expanded if still present."""
    tokens = []
    for token in command.replace('"', " ").split():
        if token.startswith("@"):
            try:
                tokens.extend(Path(token[1:]).read_text(encoding="utf-8").split())
            except OSError:
                pass
        else:
            tokens.append(token)
    return [token for token in tokens
            if token.startswith(("-f", "-O", "-std=", "-m", "-g")) and not token.startswith(DRIFT_IGNORED)]


def flag_drift(database, source, opt="-Og"):
    """Compare this script's options with one real compile; returns differences, empty when equal."""
    entries = json.loads(Path(database).read_text(encoding="utf-8"))
    rows = [item for item in entries if item.get("file", "").replace("\\", "/").endswith("/" + source)]
    if not rows:
        return ["{} has no entry for {}".format(database, source)]
    real = effective_flags(rows[0]["command"])
    ours = [token for token in compile_flags(source, opt, []) if not token.startswith("-D")]
    response_visible = "-mlongcalls" in real
    if not response_visible:
        # The response file sits in a deleted temporary build; those flags cannot be compared here.
        ours = [token for token in ours if token not in RESPONSE_FLAGS + ["-fno-rtti"]]
    differences = ["real build adds " + token for token in sorted(set(real) - set(ours))]
    differences += ["real build lacks " + token for token in sorted(set(ours) - set(real))]
    standard = [token for token in real if token.startswith("-std=")]
    expected = [token for token in ours if token.startswith("-std=")]
    if standard[-1:] != expected[-1:]:
        differences.append("effective standard {} differs from {}".format(standard[-1:], expected[-1:]))
    return differences


def source_hashes(sources, root=ROOT):
    paths = set(sources)
    for pattern in HEADER_GLOBS:
        paths.update(path.relative_to(root).as_posix() for path in root.glob(pattern))
    return {path: sha256_file(root / path) for path in sorted(paths)}


def repository_state():
    try:
        revision = tool_output(["git", "-C", ROOT, "rev-parse", "HEAD"]).strip()
        dirty = bool(tool_output(["git", "-C", ROOT, "status", "--porcelain"]).strip())
        return {"revision": revision, "dirty": dirty}
    except (OSError, subprocess.SubprocessError):
        return {"revision": None, "dirty": None}


def task_stack_bytes(root=ROOT):
    try:
        text = (root / "examples/common/EspProbeMain.h").read_text(encoding="utf-8")
    except OSError:
        return None
    match = re.search(r"FIRMWARE_STACK_BYTES\s*=\s*(\d+)", text)
    return int(match.group(1)) if match else None


def remainder_text(task_bytes, limit=CHAIN_LIMIT):
    """State the unmeasured share of the task stack; this is accounting, not a measurement."""
    if task_bytes is None:
        return "FIRMWARE_STACK_BYTES was not found; the unmeasured remainder is unknown."
    return ("Task stack {} bytes minus the {} byte chain limit leaves {} bytes. That remainder is not measured "
            "here. It must cover unknown and indirect calls, libc, SDK and FreeRTOS frames, the task entry path, "
            "and interrupt frames saved on the task stack. Only the firmware high-water diagnostic measures "
            "real use.".format(task_bytes, limit, task_bytes - limit))


def print_measurement(target, opt, analysis):
    print("\n{} {}: {} functions, {} known edges".format(target, opt, analysis["functions"], analysis["known_edges"]))
    print("  {:>6}  {:<17} {:<32} {}".format("frame", "kind", "location", "function"))
    for row in analysis["frames"][:TOP_ROWS]:
        mark = " OVER" if row["bytes"] > FRAME_LIMIT else ""
        print("  {:>6}  {:<17} {:<32} {}{}".format(row["bytes"], row["qualifier"], row["location"][-32:],
                                                  short_name(row["function"]), mark))
    print("  {:>6}  {:<17} {}".format("chain", "bound", "path"))
    for row in analysis["distinct_chains"][:TOP_ROWS]:
        path = " > ".join("{}({})".format(step["function"].split("::")[-1], step["bytes"]) for step in row["path"][:6])
        if len(row["path"]) > 6:
            path += " > ..."
        mark = " OVER" if row["bytes"] > CHAIN_LIMIT else ""
        print("  {:>6}  {:<17} {}{}".format(row["bytes"], row["bound"], path, mark))
    gaps = graph_gaps(analysis)
    print("  graph incomplete: {unknown_targets} unknown targets, {indirect_call_sites} indirect call sites, "
          "{unlocated_indirect_records} unlocated indirect records, {recursion_groups} recursion groups, {dynamic_frames} dynamic frames, "
          "{frames_without_graph_node} frames without a graph node. Totals marked lower_bound exclude these."
          .format(**gaps))
    if analysis["unknown_edges"]:
        print("  unknown targets: " + ", ".join(
            "{}({})".format(item["target"], item["callers"]) for item in analysis["unknown_edges"][:8]))
    if analysis["indirect_calls"]:
        print("  indirect callers: " + ", ".join(
            "{}({})".format(item["function"], item["sites"]) for item in analysis["indirect_calls"][:8]))
    if analysis["recursion"]:
        print("  recursion: " + "; ".join(" <> ".join(group) for group in analysis["recursion"]))
    if analysis["dynamic_frames"]:
        print("  dynamic frames: " + ", ".join(short_name(row["function"]) for row in analysis["dynamic_frames"]))
    print("  breaches: {} frames over {}, {} chains over {}".format(
        len(analysis["frame_breaches"]), FRAME_LIMIT, len(analysis["chain_breaches"]), CHAIN_LIMIT))


def print_baseline(result, allow_file):
    if allow_file is None:
        print("\nNo --allow-file given: every breach fails.")
    labels = (("new", "NEW breach"), ("grown", "GREW past baseline"), ("reduced", "smaller than baseline"),
              ("vanished", "no longer breaches; remove from baseline"))
    for key, label in labels:
        for item in result[key]:
            size = item.get("bytes", "-")
            print("  {}: {} {} {} {} {} bytes (baseline {})".format(
                label, item["target"], item["opt"], item["kind"], short_name(item["function"]),
                size, item.get("baseline_bytes", "none")))
    print("baseline: {} matched, {} new, {} grown, {} reduced, {} vanished, {} entries under --strict".format(
        len(result["matched"]), len(result["new"]), len(result["grown"]), len(result["reduced"]),
        len(result["vanished"]), len(result["strict_entries"])))


def measurement_report(analysis):
    """Keep every breach and gap; only passing rows are trimmed."""
    report = {key: analysis[key] for key in (
        "functions", "known_edges", "frame_breaches", "chain_breaches", "dynamic_frames", "recursion",
        "indirect_calls", "unlocated_indirect_records", "unknown_edges", "frames_without_graph_node",
        "edge_sources_without_node")}
    report["graph_gaps"] = graph_gaps(analysis)
    report["top_frames"] = analysis["frames"][:REPORT_ROWS]
    report["top_chains"] = analysis["distinct_chains"][:REPORT_ROWS]
    return report


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD, help="Prefer a short path on Windows")
    parser.add_argument("--toolchain-dir", type=Path, help="Directory holding xtensa-<target>-elf-g++")
    parser.add_argument("--require-toolchain", action="store_true", help="A missing compiler fails instead of skipping")
    parser.add_argument("--allow-file", type=Path, help="Baseline JSON of known breaches, usually "
                                                        "scripts/check_stack_baseline.json")
    parser.add_argument("--strict", action="store_true", help="Fail on any entry in the --allow-file baseline")
    parser.add_argument("--write-baseline", type=Path, help="Rewrite a baseline from this measurement, keeping "
                                                            "existing reasons; needs every target and level")
    parser.add_argument("--report", type=Path, help="Write the JSON report here")
    parser.add_argument("--target", action="append", choices=TARGETS, help="Measure one target; default both")
    parser.add_argument("--opt", action="append", choices=sorted(OPT_FLAGS), help="Measure one level, written --opt=-Og; default both")
    parser.add_argument("--compile-commands", action="append", default=[], metavar="TARGET=PATH",
                        help="Real compile database; options differing from this script fail")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    return parser.parse_args(argv)


def check_drift(pairs, requested):
    problems = []
    for pair in pairs:
        target, separator, path = pair.partition("=")
        if not separator or target not in requested:
            raise StackCheckError("--compile-commands needs TARGET=PATH with a measured target: " + pair)
        for source in DRIFT_SAMPLES:
            problems.extend("{} {}: {}".format(target, source, item) for item in flag_drift(path, source))
    return problems


def run(args, report):
    """Fill the report and return the exit code."""
    requested = tuple(target for target in TARGETS if not args.target or target in args.target)
    levels = tuple(opt for opt in ("-Os", "-Og") if not args.opt or opt in args.opt)
    report["limits"] = {"frame_bytes": FRAME_LIMIT, "chain_bytes": CHAIN_LIMIT}
    report["requested"] = {"targets": requested, "opts": levels}
    report["repository"] = repository_state()
    task_bytes = task_stack_bytes()
    report["task_stack_bytes"] = task_bytes
    report["unmeasured_remainder"] = remainder_text(task_bytes)

    compilers = {}
    missing = {}
    for target in requested:
        compiler, reason = find_compiler(target, args.toolchain_dir)
        if compiler is None:
            missing[target] = reason
        else:
            compilers[target] = compiler
    report["compilers"] = {target: compiler.evidence for target, compiler in compilers.items()}
    report["missing_compilers"] = missing
    if missing and args.require_toolchain:
        report["status"] = "unavailable"
        print("FAILED: required compiler unavailable: " + "; ".join(missing.values()), file=sys.stderr)
        return EXIT_FAILED
    if args.write_baseline and (missing or set(requested) != set(TARGETS) or set(levels) != set(OPT_FLAGS)):
        raise StackCheckError("--write-baseline needs both targets and both levels measured")

    sources, not_measured, include_dirs = source_inventory()
    report["not_measured_sources"] = not_measured
    report["sources"] = source_hashes(sources + sorted(not_measured))
    report["flags"] = {opt: {"c++": compile_flags("a.cpp", opt, []), "c": compile_flags("a.c", opt, [])}
                       for opt in levels}
    report["flags"]["measure"] = MEASURE_FLAGS

    drift = check_drift(args.compile_commands, requested)
    report["flag_drift"] = drift
    if drift:
        report["status"] = "failed"
        print("FAILED: options differ from the real build:\n  " + "\n  ".join(drift), file=sys.stderr)
        return EXIT_FAILED

    measurements = {}
    report["measurements"] = {}
    report["context_sizes"] = {}
    for target, compiler in compilers.items():
        report["measurements"][target] = {}
        for opt in levels:
            analysis = measure(compiler, opt, sources, include_dirs, args.build_dir, args.jobs)
            measurements[(target, opt)] = analysis
            report["measurements"][target][opt] = measurement_report(analysis)
            print_measurement(target, opt, analysis)
        report["context_sizes"][target] = context_sizes(compiler, include_dirs, args.build_dir)
        print("\n{} sizes: {}".format(target, ", ".join(
            "{}={}".format(name, size if size is not None else "unavailable")
            for name, size in report["context_sizes"][target].items())))

    if not_measured:
        print("\nNot measured (SDK headers required): " + ", ".join(sorted(not_measured)))
    print(report["unmeasured_remainder"])

    breaches = collect_breaches(measurements)
    entries = load_baseline(args.allow_file) if args.allow_file else []
    if args.write_baseline:
        previous = load_baseline(args.write_baseline) if Path(args.write_baseline).is_file() else entries
        entries = write_baseline(args.write_baseline, breaches, previous)
        print("Wrote {} baseline entries to {}".format(len(entries), args.write_baseline))
    result = evaluate_baseline(breaches, entries, set(measurements), args.strict)
    report["baseline"] = dict(result, path=str(args.allow_file or args.write_baseline or ""), entries=len(entries))
    print_baseline(result, args.allow_file or args.write_baseline)
    if result["failed"]:
        report["status"] = "failed"
        print("FAILED: stack budget breaches outside the baseline.", file=sys.stderr)
        return EXIT_FAILED
    if missing:
        report["status"] = "skipped"
        print("SKIP, NOT A PASS: compiler missing: " + "; ".join(missing.values()))
        return EXIT_SKIPPED
    report["status"] = "passed"
    allowed = len(result["matched"]) + len(result["reduced"])
    if allowed:
        print("Passed with {} known breaches allowed by the baseline. Budgets are not yet met.".format(allowed))
    else:
        print("Passed: no breach of the project stack limits. Chains marked lower_bound stay incomplete.")
    return EXIT_PASSED


def main(argv=None):
    args = parse_arguments(argv)
    report = {"schema": 1, "status": "running"}
    try:
        code = run(args, report)
    except (StackCheckError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        report["status"] = "failed"
        report["error"] = str(error)
        print("FAILED: {}".format(error), file=sys.stderr)
        code = EXIT_FAILED
    report["exit_code"] = code
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
