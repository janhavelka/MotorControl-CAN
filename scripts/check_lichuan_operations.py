#!/usr/bin/env python3
"""Reconcile public operations with the single reviewed object ledger.

This is a source/registration consistency check, not a test runner or a device
qualification. CTest executes the cited behavioral tests. The small C++ scanner
understands this repository's free-function declarations; it is not a C++ parser.
It ignores comments/strings when checking calls and follows real function bodies
for indirectly exercised helpers. A descriptor never counts as a typed operation.
"""

import argparse
import ast
from collections import Counter, defaultdict
from contextlib import redirect_stderr
from functools import lru_cache
import io
import json
from pathlib import Path
import re
import sys
import types

from check_lichuan_ledger import ROOT, generation_disposition, validate_ledger


MAP_PATH = "docs/reference/lichuan_operations.json"
LEDGER_PATH = "docs/reference/lichuan_objects.json"
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
FUNCTION = re.compile(
    r"^[ \t]*(?:(?:inline|constexpr|static|explicit)\s+)*"
    r"(?:[\w:<>]+[ \t*&]+)+(?P<name>[A-Za-z_]\w*)\s*"
    r"\((?P<args>[^;{}]*?)\)\s*(?:const\s*)?(?:noexcept\s*)?(?P<end>[;{])", re.M)
CLI_FUNCTION = re.compile(FUNCTION.pattern.replace(
    "(?P<name>", r"(?P<owner>(?:[A-Za-z_]\w*::)*)(?P<name>"), re.M)
NON_CALLS = {"if", "while", "for", "switch", "sizeof", "alignof", "static_cast",
             "reinterpret_cast", "const_cast", "dynamic_cast", "decltype"}


def strip_comments(text):
    """Preserve quoted text while removing comments, including disabled claims."""
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: " " if m[0].startswith(("//", "/*")) else m[0], text)


def code_only(text):
    text = strip_comments(text)
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', '""', text)


def free_function_code(text):
    """Mask balanced class/struct bodies without depending on indentation."""
    code = code_only(text)
    masked = list(code)
    for match in re.finditer(r"\b(?:class|struct)\s+\w+[^;{}]*\{", code):
        depth, cursor = 1, match.end()
        while cursor < len(code) and depth:
            depth += (code[cursor] == "{") - (code[cursor] == "}")
            cursor += 1
        if depth == 0:
            masked[match.start():cursor] = ["\n" if char == "\n" else " "
                                          for char in code[match.start():cursor]]
    return "".join(masked)


@lru_cache(maxsize=128)
def public_functions(text):
    """Map ordinary namespace free-function names to overload counts."""
    return Counter(m["name"] for m in FUNCTION.finditer(free_function_code(text))
                   if m["name"] not in NON_CALLS and not m[0].lstrip().startswith("return "))


@lru_cache(maxsize=128)
def bodies(text, qualified=False):
    code = code_only(text)
    found = []
    for match in (CLI_FUNCTION if qualified else FUNCTION).finditer(code):
        if match["end"] != "{" or match["name"] in NON_CALLS:
            continue
        start, depth, cursor = match.end(), 1, match.end()
        while cursor < len(code) and depth:
            depth += (code[cursor] == "{") - (code[cursor] == "}")
            cursor += 1
        if depth == 0:
            owner = match["owner"] if qualified else ""
            found.append((owner + match["name"], code[start:cursor - 1]))
    return found


def call_graph(texts):
    graph = defaultdict(set)
    assertions = set()
    for text in texts:
        for name, body in bodies(text):
            graph[name].update(set(CALL.findall(body)) - NON_CALLS)
            # Existing action tests deliberately invoke each named scenario
            # through this simple diagnostic macro. Only its real invocations
            # in a reachable function count, not the macro definition.
            graph[name].update(re.findall(r"\bRUN\(\s*(\w+)\s*\)", body))
            if re.search(r"\b(?:CHECK|assert|REQUIRE|EXPECT_[A-Z_]+)\s*\(", body):
                assertions.add(name)
            elif re.search(r"\breturn\s+[1-9][0-9]*\s*;", body) and "if" in body:
                assertions.add(name)  # Installed consumer exits nonzero on failure.
            elif re.search(r"\breturn\b[\s\S]*?\?\s*0\s*:\s*1\s*;", body):
                assertions.add(name)
        # Constant-expression tests execute at compile time. Follow the actual
        # initialized value into a static assertion instead of accepting a
        # commented function name or an unused namespace-scope expression.
        code = code_only(text)
        for variable, expression in re.findall(r"\bconstexpr\s+[^;=]+?\b(\w+)\s*=\s*([^;]+);", code):
            if re.search(r"\bstatic_assert\s*\([^;]*\b" + re.escape(variable) + r"\b", code):
                graph["main"].update(CALL.findall(expression))
    return graph, assertions


def reachable(graph, starts):
    seen, pending = set(), list(starts)
    while pending:
        name = pending.pop()
        if name not in seen:
            seen.add(name)
            pending.extend(set(graph.get(name, ())) - seen)
    return seen


def command_inventory(text):
    """Read the existing initializer, retaining its actual gate field."""
    start = text.index("const ConsoleCommand commands[] = {")
    end = text.index("bool equal(", start)
    # Braces in grammar/prose literals are data. Find actual initializer rows by
    # depth outside quoted strings; the outer array initializer is depth one.
    rows, depth, row_start, quoted, escaped = [], 0, None, False, False
    source = strip_comments(text[start:end])
    for cursor, char in enumerate(source):
        if char == '"' and not escaped:
            quoted = not quoted
        if not quoted:
            if char == "{":
                depth += 1
                if depth == 2:
                    row_start = cursor + 1
            elif char == "}":
                if depth == 2 and row_start is not None:
                    rows.append(source[row_start:cursor])
                    row_start = None
                depth -= 1
        escaped = char == "\\" and not escaped
    result = {}
    # All command fields are string literals/enums/nullptr; concatenate adjacent
    # literals before splitting comma fields so commas inside prose stay quoted.
    for row in rows:
        fields, field, quoted, escaped = [], "", False, False
        for char in row:
            if char == '"' and not escaped:
                quoted = not quoted
            if char == "," and not quoted:
                fields.append(field)
                field = ""
            else:
                field += char
            escaped = char == "\\" and not escaped
        fields.append(field)
        if len(fields) != 9:
            continue
        words = re.findall(r'"((?:\\.|[^"\\])*)"', fields[0])
        if words:
            result["".join(words)] = {"working": fields[7].strip() == "nullptr",
                                      "api": "".join(re.findall(r'"((?:\\.|[^"\\])*)"', fields[2])),
                                      "grammar": fields[1]}
    return result


def cli_reaches_entry(texts, dispatch, entry):
    """Follow one named application bridge, restricted to its declared files.

    Qualified definitions are needed only here. Native test reachability keeps
    its existing namespace-free convention; this is not a C++ type resolver.
    """
    functions = defaultdict(list)
    for text in texts:
        for name, body in bodies(text, qualified=True):
            functions[name].append(body)
    if dispatch not in functions:
        return False
    pending, seen = [dispatch], set()
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        calls = {call for body in functions[name] for call in CALL.findall(body)} - NON_CALLS
        # Inspect outgoing calls: a getter bridge sharing its native name does
        # not count merely because it is the selected dispatch function.
        if entry in calls:
            return True
        pending.extend(candidate for candidate in functions
                       if candidate.rsplit("::", 1)[-1] in calls and candidate not in seen)
    return False


def binding_family(binding):
    return binding.get("family") if isinstance(binding, dict) else binding


def binding_errors(binding, role, families, label):
    """Validate API references, without copying any object or firmware facts."""
    if not isinstance(binding, dict) or binding.get("role") != role:
        return [label + ": descriptor/codec is not a typed " + role +
                " implementation; binding requires explicit public entry evidence"]
    errors = []
    refs = binding.get("entries", [])
    if not isinstance(refs, list) or not refs:
        return [label + ": typed " + role + " binding has no public entries"]
    names = []
    for reference in refs:
        owner, separator, name = reference.partition(".") if isinstance(reference, str) else ("", "", "")
        family = families.get(owner)
        if not separator or not family or name not in family["functions"]:
            errors.append(label + ": binding references an absent public entry " + str(reference))
        else:
            names.append(name)
    selected = families.get(binding.get("family"))
    if not selected:
        errors.append(label + ": binding has no public family")
    elif not any(reference.startswith(binding["family"] + ".") for reference in refs
                 if isinstance(reference, str)):
        errors.append(label + ": public entries do not belong to the selected family")
    if role in ("read", "setting") and (not any(n.startswith("prepare") for n in names) or
                                        not any(n.startswith("get") for n in names)):
        errors.append(label + ": descriptor/codec is not a typed " + role + " implementation")
    if role == "workflow" and selected:
        functions = selected["functions"]
        if not all(any(n.startswith(phase) for n in functions) for phase in ("prepare", "next", "advance")):
            errors.append(label + ": descriptor/codec is not a typed workflow implementation")
    return errors


def cmake_source(text):
    """Remove line/bracket comments while preserving quoted command arguments."""
    token = r'"(?:\\.|[^"\\])*"|#\[(?P<equals>=*)\[[\s\S]*?\](?P=equals)\]|#[^\n]*'
    return re.sub(token, lambda match: " " if match[0].startswith("#") else match[0], text)


def registered_tests(root):
    cmake = cmake_source((root / "test/CMakeLists.txt").read_text(encoding="utf-8"))
    # Registration and source consumption must both exist. The actual CTest JSON
    # commands are independently checked by check_repository.py.
    targets = {name: set(re.findall(r"[\w/]+\.cpp", arguments))
               for name, arguments in re.findall(r"^[ \t]*add_executable\((\w+)\s+([^)]*)\)", cmake, re.M)}
    tests = dict(re.findall(r"^[ \t]*add_test\(NAME\s+(\w+)\s+COMMAND\s+(\w+)\s*\)", cmake, re.M))
    return {name: targets.get(target, set()) for name, target in tests.items()}


def operation_facet_errors(row):
    """Keep conditional software workflow coverage separate from full semantics."""
    errors, key = [], str(row.get("key"))
    coverage = row.get("operation_coverage")
    if coverage is not None:
        fields = {"documented", "implemented", "host_tested", "hardware_qualified"}
        if not isinstance(coverage, dict) or set(coverage) != fields or any(
                type(coverage.get(field)) is not bool for field in fields):
            errors.append(key + ": operation_coverage must contain four boolean evidence fields")
        else:
            if coverage["implemented"] and not coverage["documented"]:
                errors.append(key + ": workflow implementation requires documentation")
            if coverage["host_tested"] and not coverage["implemented"]:
                errors.append(key + ": workflow host testing requires implementation")
            if coverage["hardware_qualified"] and not coverage["host_tested"]:
                errors.append(key + ": workflow hardware qualification requires host testing")
            if coverage["implemented"] and (not isinstance(row.get("operation_scope"), str) or
                                             not row["operation_scope"].strip()):
                errors.append(key + ": conditional workflow requires explicit operation_scope")
            if coverage["implemented"] and not coverage["host_tested"]:
                errors.append(key + ": claimed workflow needs completed behavioral tests")
            if coverage["hardware_qualified"] and not row.get("operation_qualification_evidence"):
                errors.append(key + ": workflow hardware qualification lacks retained device evidence")
            if coverage["hardware_qualified"]:
                errors.append(key + ": workflow hardware qualification is not admitted by the current evidence schema; "
                              "prompts 47/48 must establish retained qualification evidence before promotion")
    if row.get("coverage", {}).get("implemented") and row.get("review", {}).get("gate_ids"):
        errors.append(key + ": unresolved semantic gates cannot be silently promoted by operation coverage")
    return errors


def check(root=ROOT, mapping=None, ledger=None):
    """Return errors and an honest facet inventory; never mutate the repository."""
    root = Path(root)
    mapping = mapping if mapping is not None else json.loads((root / MAP_PATH).read_text(encoding="utf-8"))
    ledger = ledger if ledger is not None else json.loads((root / LEDGER_PATH).read_text(encoding="utf-8"))
    errors = []
    allowed = {"schema_version", "object_ledger", "purpose", "families", "ledger_bindings",
               "python_plans", "python_tests"}
    if set(mapping) - allowed:
        errors.append("operation map contains fields outside API/coverage references")
    if mapping.get("schema_version") != 2 or mapping.get("object_ledger") != LEDGER_PATH:
        errors.append("operation map requires API-evidence schema 2 and the single reviewed object ledger")
    # The ledger verifier retains original EDS accounting and all 53 manual
    # obligations. Removing an unimplemented row cannot improve coverage.
    errors.extend(validate_ledger(ledger, root))
    families = mapping.get("families", [])
    headers = {p.relative_to(root).as_posix() for p in (root / "include/MotorControlCAN").rglob("*.h")}
    expected_headers = {p for p in headers if public_functions((root / p).read_text(encoding="utf-8"))}
    mapped_headers = [f.get("header") for f in families]
    if set(mapped_headers) != expected_headers or len(mapped_headers) != len(set(mapped_headers)):
        errors.append("public header inventory differs: " + str(sorted(expected_headers ^ set(mapped_headers))))
    commands = command_inventory((root / "examples/common/ProbeConsole.cpp").read_text(encoding="utf-8"))
    registered = registered_tests(root)
    family_ids = set()
    for family in families:
        label = str(family.get("id"))
        allowed_family = {"id", "header", "functions", "implementation", "tests", "api_only_reason",
                          "console", "python_scenarios", "console_api_only_reason", "python_api_only_reason"}
        if set(family) - allowed_family:
            errors.append(label + ": operation map cannot duplicate device facts")
        if label in family_ids:
            errors.append(label + ": duplicate family")
        family_ids.add(label)
        header = family.get("header", "")
        if header not in expected_headers:
            errors.append(label + ": unknown public operation header")
            continue
        actual = public_functions((root / header).read_text(encoding="utf-8"))
        if dict(actual) != family.get("functions"):
            errors.append(label + ": public function/overload inventory differs")
        if not family.get("api_only_reason"):
            errors.append(label + ": sequencing/helper API-only rationale missing")
        implementation_texts, test_texts = [], []
        for path in family.get("implementation", []):
            if not path.startswith(("src/", "include/")) or not (root / path).is_file():
                errors.append(label + ": missing production implementation " + path)
            else:
                implementation_texts.append((root / path).read_text(encoding="utf-8"))
        for name, count in actual.items():
            defined = sum(n == name for text in implementation_texts for n, _ in bodies(text))
            if defined < count:
                errors.append(label + ": missing implementation for " + name)
        for test in family.get("tests", []):
            path, name = test.get("path", ""), test.get("ctest")
            if not (root / path).is_file() or path.removeprefix("test/") not in registered.get(name, set()):
                errors.append(label + ": missing registered behavioral test " + path)
                continue
            text = (root / path).read_text(encoding="utf-8")
            graph, assertions = call_graph([text])
            live = reachable(graph, {"main"})
            if not live.intersection(assertions):
                errors.append(label + ": test has no reachable assertions: " + path)
            test_texts.append(text)
        if not test_texts:
            errors.append(label + ": no behavioral test evidence")
        # Tests may exercise a validator through the preparation that calls it.
        # Only bodies reachable from main count; unused success fixtures do not.
        graph, assertions = call_graph(test_texts + implementation_texts)
        live = reachable(graph, {"main"})
        for name in actual:
            if name not in live:
                errors.append(label + ": public function has no reachable native test call: " + name)
        for route in family.get("console", []):
            command = route.get("command")
            if command not in commands or not commands[command]["working"]:
                errors.append(label + ": CLI route absent or gated: " + str(command))
            entry = route.get("entry")
            if entry not in actual:
                errors.append(label + ": CLI entry is not a typed public function")
            if command in commands and route.get("api") != commands[command]["api"]:
                errors.append(label + ": CLI API expression differs from the selected command")
            paths = route.get("implementation", [])
            bridge = [(root / p).read_text(encoding="utf-8") for p in paths if (root / p).is_file()]
            if (len(bridge) != len(paths) or not paths or
                    not cli_reaches_entry(bridge, route.get("dispatch"), entry)):
                errors.append(label + ": CLI bridge does not call public entry " + str(entry))
            console = route.get("console_dispatch", {})
            console_path = root / console.get("path", "")
            dispatch = route.get("dispatch", "")
            if (not console_path.is_file() or not dispatch or
                    (console.get("function") != dispatch and not cli_reaches_entry(
                        [console_path.read_text(encoding="utf-8")], console.get("function"),
                        dispatch.rsplit("::", 1)[-1]))):
                errors.append(label + ": console dispatch does not call its declared application bridge")
        if not family.get("console") and not family.get("console_api_only_reason"):
            errors.append(label + ": no CLI route or concrete API-only reason")
        if not family.get("python_scenarios") and not family.get("python_api_only_reason"):
            errors.append(label + ": no Python scenario or concrete API-only reason")
    rows = [e for obj in ledger.get("objects", []) for e in obj.get("entries", [])]
    bindings = mapping.get("ledger_bindings", {})
    family_map = {f["id"]: f for f in families}
    if set(bindings.get("manual", {})) != {r["key"] for r in ledger.get("manual_only", [])}:
        errors.append("manual API/pending inventory differs from the reviewed ledger")
    counts = Counter(read_hardware_qualified=0, setting_hardware_qualified=0,
                     operation_hardware_qualified=0, manual_operation_hardware_qualified=0)
    pending, inventory, manual_inventory = [], [], []
    for row in rows:
        key = row["key"]
        errors.extend(operation_facet_errors(row))
        operation = row.get("operation_coverage")
        operation = operation if isinstance(operation, dict) else {}
        item = {"key": key, "owner_prompt": row["owner_prompt"],
                "descriptor": generation_disposition(row), "facets": {}}
        for facet, owner_field, coverage_field in (("read", "read_owner_prompt", "read_coverage"),
                                                  ("setting", "owner_prompt", "setting_coverage")):
            coverage = row.get(coverage_field, {})
            binding = bindings.get(facet, {}).get(str(row.get(owner_field)))
            family = binding_family(binding)
            item["facets"][facet] = {"implemented": coverage.get("implemented", False),
                                      "owner_prompt": row.get(owner_field), "family": family}
            if facet == "read" and row.get(owner_field) is not None and not coverage.get("implemented"):
                pending.append({"key": key, "facet": facet, "owner_prompt": row[owner_field],
                                "status": "pending_software",
                                "reason": "Standalone typed read is not implemented; descriptor/action prerequisite is insufficient."})
            if coverage.get("implemented"):
                if family not in family_ids:
                    errors.append(key + ": implemented " + facet + " has no typed public family")
                else:
                    errors.extend(binding_errors(binding, facet, family_map, key))
                counts[facet + "_implemented"] += 1
            if coverage.get("hardware_qualified"):
                counts[facet + "_hardware_qualified"] += 1
        if (row.get("access_reviewed") in ("RW", "WO") and row.get("role") != "reserved"
                and not row.get("setting_coverage", {}).get("implemented")):
            pending.append({"key": key, "facet": "setting_or_workflow", "owner_prompt": row["owner_prompt"],
                            "status": "blocked" if row.get("review", {}).get("gate_ids") else "pending",
                            "gates": row.get("review", {}).get("gate_ids", []),
                            "reason": row.get("review", {}).get("next_evidence") or
                                      "No general typed setter is claimed; concrete operation-owned writes do not complete it."})
        item["facets"]["semantic"] = row.get("coverage", {})
        item["facets"]["operation"] = row.get("operation_coverage", {})
        if operation.get("implemented"):
            binding = bindings.get("operation", {}).get(str(row["owner_prompt"]))
            if binding_family(binding) not in family_ids:
                errors.append(key + ": implemented workflow has no public operation family")
            else:
                errors.extend(binding_errors(binding, "workflow", family_map, key))
            counts["operation_implemented"] += 1
        if operation.get("hardware_qualified"):
            counts["operation_hardware_qualified"] += 1
        if not row.get("coverage", {}).get("implemented") and row.get("role") != "reserved":
            pending.append({"key": key, "facet": "semantic", "owner_prompt": row["owner_prompt"],
                            "status": "blocked" if row.get("review", {}).get("gate_ids") else "pending",
                            "gates": row.get("review", {}).get("gate_ids", []),
                            "reason": row.get("review", {}).get("next_evidence") or row.get("effect")})
        inventory.append(item)
    for row in ledger.get("manual_only", []):
        errors.extend(operation_facet_errors(row))
        operation = row.get("operation_coverage")
        operation = operation if isinstance(operation, dict) else {}
        binding = bindings.get("manual", {}).get(row["key"])
        family = binding_family(binding)
        if family is not None and family not in family_ids:
            errors.append(row["key"] + ": manual operation family missing")
        if row.get("coverage", {}).get("implemented") and family is None:
            errors.append(row["key"] + ": implemented manual obligation has no public family")
        if operation.get("implemented"):
            if family not in family_ids:
                errors.append(row["key"] + ": implemented manual workflow has no public family")
            else:
                errors.extend(binding_errors(binding, "workflow", family_map, row["key"]))
            counts["manual_operation_implemented"] += 1
        elif isinstance(binding, dict) and binding.get("role") == "workflow" and binding.get("entries"):
            errors.append(row["key"] + ": public workflow entry evidence lost its implemented operation facet")
        if operation.get("hardware_qualified"):
            counts["manual_operation_hardware_qualified"] += 1
        manual_inventory.append({"key": row["key"], "owner_prompt": row["owner_prompt"],
                                 "family": family, "semantic": row["coverage"],
                                 "operation": operation, "operation_scope": row.get("operation_scope")})
        if not row.get("coverage", {}).get("implemented"):
            pending.append({"key": row["key"], "facet": "manual", "owner_prompt": row["owner_prompt"],
                            "status": "blocked" if row["review"]["gate_ids"] else "pending",
                            "gates": row["review"]["gate_ids"], "reason": row["review"]["decision"]})
    # Scenario names are derived from Python's actual literal catalogue. The
    # plan tests execute build_scenario and check its commands without serial I/O.
    tree = ast.parse((root / "scripts/bench_scenarios.py").read_text(encoding="utf-8"))
    scenarios = next((ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                      and any(isinstance(t, ast.Name) and t.id == "SCENARIOS" for t in n.targets)), {})
    claimed = {s for f in families for s in f.get("python_scenarios", [])}
    if claimed != set(scenarios):
        errors.append("Python scenario catalogue differs from mapped native families")
    plans = mapping.get("python_plans", {})
    if set(plans) != set(scenarios):
        errors.append("Python scenario offline plan inventory differs")
    module = types.ModuleType("_operations_scenarios")
    module.__file__ = str(root / "scripts/bench_scenarios.py")
    sys.modules[module.__name__] = module
    try:
        exec(compile((root / "scripts/bench_scenarios.py").read_text(encoding="utf-8"),
                     module.__file__, "exec"), module.__dict__)
        for name, arguments in plans.items():
            try:
                with redirect_stderr(io.StringIO()):
                    scenario = module.build_scenario(module.arguments(["--plan-only", "--scenario", name] + arguments))
                for command in scenario.commands:
                    route = commands.get(command.split()[0])
                    if route is None or not route["working"]:
                        errors.append(name + ": Python plan emits an absent/gated CLI command")
                emitted = {command.split()[0] for command in scenario.commands}
                for family in families:
                    if name in family.get("python_scenarios", []) and not emitted.intersection(
                            route["command"] for route in family.get("console", [])):
                        errors.append(family["id"] + ": Python scenario does not exercise a mapped family CLI route: " + name)
                if scenario.plan.get("hardware_qualified") is not False:
                    errors.append(name + ": an offline plan cannot claim hardware qualification")
            except (ValueError, SystemExit, TypeError) as error:
                errors.append(name + ": Python offline plan failed: " + str(error))
    finally:
        del sys.modules[module.__name__]
    for test in mapping.get("python_tests", []):
        name, path = test.get("ctest", ""), test.get("path", "")
        cmake = cmake_source((root / "test/CMakeLists.txt").read_text(encoding="utf-8"))
        if not (root / path).is_file() or not re.search(
                r"^[ \t]*add_test\(NAME\s+" + re.escape(name) + r"\s+COMMAND[^)]*" + re.escape(Path(path).name), cmake, re.M):
            errors.append("Python scenario test is missing/unregistered: " + path)
    if not mapping.get("python_tests"):
        errors.append("Python scenarios require registered offline behavioral tests")
    summary = {"eds_entries": len(rows), "manual_obligations": len(ledger.get("manual_only", [])),
               "public_families": len(families), "public_functions": sum(sum(f.get("functions", {}).values()) for f in families),
               "coverage": dict(counts), "entries": inventory, "manual_entries": manual_inventory, "pending": pending,
               "hardware_qualification_inferred": False}
    return errors, summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--json", action="store_true", help="emit every retained pending obligation")
    args = parser.parse_args(argv)
    try:
        errors, summary = check(args.root)
    except (OSError, ValueError, KeyError, TypeError, StopIteration) as error:
        print("Lichuan operations: " + str(error), file=sys.stderr)
        return 1
    for error in errors:
        print(error, file=sys.stderr)
    if args.json:
        print(json.dumps(summary, indent=2))
    elif not errors:
        print("Lichuan operations: {eds_entries} EDS entries, {manual_obligations} manual obligations, "
              "{public_functions} public overloads reconciled; source/hardware gates retained.".format(**summary))
    return int(bool(errors))


if __name__ == "__main__":
    sys.exit(main())
