#!/usr/bin/env python3
"""Parser, budget, baseline and skip regressions for check_stack.py with synthetic GCC output."""

import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import check_stack as checker


def node(title, frame=None, qualifier="static", location="src/a.cpp:1:1", name=None):
    label = (name or title + "()") + "\\n" + location
    if frame is None:
        return 'node: {{ title: "{}" label: "{}" shape : ellipse }}'.format(title, label)
    return 'node: {{ title: "{}" label: "{}\\n{} bytes ({})" }}'.format(title, label, frame, qualifier)


def edge(source, target, site="src/a.cpp:2:3"):
    suffix = ' label: "{}"'.format(site) if site else ""
    return 'edge: {{ sourcename: "{}" targetname: "{}"{} }}'.format(source, target, suffix)


def graph_of(*lines, su=""):
    text = 'graph: { title: "src/a.cpp"\n' + "\n".join(lines) + "\n}\n"
    nodes, edges = checker.parse_ci(text)
    return checker.build_graph([(checker.parse_su(su), nodes, edges)])


def chain_of(analysis, function):
    return next(row for row in analysis["chains"] if row["function"] == function + "()")


class ParserTests(unittest.TestCase):
    def test_su_row_keeps_drive_letter_and_signature(self):
        rows = checker.parse_su("C:/tool/x.h:12:3:void ns::{anonymous}::f(const char*)\t64\tdynamic,bounded\n")
        self.assertEqual(rows, [{"location": "C:/tool/x.h:12:3", "name": "void ns::{anonymous}::f(const char*)",
                                 "frame": 64, "qualifier": "dynamic,bounded"}])

    def test_unrecognized_rows_fail_instead_of_hiding_frames(self):
        with self.assertRaises(checker.StackCheckError):
            checker.parse_su("not a stack row\n")
        with self.assertRaises(checker.StackCheckError):
            checker.parse_ci('graph: { title: "a"\nvertex: { title: "b" }\n}\n')

    def test_ci_defined_external_and_edge_without_site(self):
        nodes, edges = checker.parse_ci("\n".join([
            'graph: { title: "a"', node("f", 96), node("memcpy"), edge("f", "memcpy", site=None), "}"]))
        self.assertEqual((nodes[0]["frame"], nodes[0]["qualifier"], nodes[0]["external"]), (96, "static", False))
        self.assertEqual((nodes[1]["frame"], nodes[1]["external"]), (None, True))
        self.assertEqual(edges, [{"source": "f", "target": "memcpy", "site": ""}])

    def test_clone_symbol_replaces_uninformative_printed_name(self):
        title = "src/a.cpp:_ZN15MotorControlCAN9LICHUAN_C12_GLOBAL__N_14prepERi$constprop$0"
        nodes, _ = checker.parse_ci(node(title, 64, name="constprop(int&)"))
        self.assertEqual(nodes[0]["printed"], "constprop(int&)")
        self.assertEqual(checker.short_name(nodes[0]["name"]), "LICHUAN_C::prep.constprop")

    def test_short_name_drops_return_type_and_parameters(self):
        self.assertEqual(checker.short_name("void MotorControlCAN::a::f<int>(const char*)"), "a::f<int>")

    def test_host_paths_normalize(self):
        self.assertEqual(checker.normalize_path("src\\a.cpp"), "src/a.cpp")
        self.assertEqual(checker.normalize_path((checker.ROOT / "src/a.cpp").as_posix()), "src/a.cpp")
        self.assertEqual(checker.normalize_path("C:/tool/include/x.h"), "<external>/x.h")


class BudgetTests(unittest.TestCase):
    def test_static_frame_at_limit_passes_and_over_limit_breaches(self):
        analysis = checker.analyze(graph_of(node("at", checker.FRAME_LIMIT), node("over", checker.FRAME_LIMIT + 1)))
        self.assertEqual([row["function"] for row in analysis["frame_breaches"]], ["over()"])

    def test_dynamic_frame_is_visible_and_chain_is_only_a_lower_bound(self):
        analysis = checker.analyze(graph_of(node("alloca", 64, "dynamic"), node("caller", 32),
                                            edge("caller", "alloca")))
        self.assertEqual([row["function"] for row in analysis["dynamic_frames"]], ["alloca()"])
        self.assertEqual(analysis["frame_breaches"], [])
        row = chain_of(analysis, "caller")
        self.assertEqual((row["bytes"], row["bound"], row["dynamic_frames"]), (96, "lower_bound", ["alloca"]))

    def test_bounded_dynamic_frame_is_also_not_called_static(self):
        analysis = checker.analyze(graph_of(node("vla", 64, "dynamic,bounded")))
        self.assertEqual(analysis["dynamic_frames"][0]["qualifier"], "dynamic,bounded")

    def test_chain_through_known_edges_takes_the_largest_path(self):
        limit = checker.CHAIN_LIMIT
        analysis = checker.analyze(graph_of(
            node("root", 1000), node("small", 100), node("big", limit - 1000), node("leaf", 1),
            edge("root", "small"), edge("root", "big"), edge("big", "leaf")))
        root = chain_of(analysis, "root")
        self.assertEqual(root["bytes"], limit + 1)
        self.assertEqual(root["bound"], "complete")
        self.assertEqual([step["function"] for step in root["path"]], ["root", "big", "leaf"])
        self.assertEqual([row["function"] for row in analysis["chain_breaches"]], ["root()"])
        self.assertEqual(chain_of(analysis, "big")["bytes"], limit - 999)

    def test_chain_at_limit_passes(self):
        analysis = checker.analyze(graph_of(node("root", checker.CHAIN_LIMIT - 8), node("leaf", 8),
                                            edge("root", "leaf")))
        self.assertEqual(analysis["chain_breaches"], [])

    def test_chain_merges_across_translation_units(self):
        first = checker.parse_ci("\n".join([node("root", 64), node("shared"), edge("root", "shared")]))
        second = checker.parse_ci(node("shared", 128))
        analysis = checker.analyze(checker.build_graph([([], *first), ([], *second)]))
        self.assertEqual(chain_of(analysis, "root")["bytes"], 192)
        self.assertEqual(analysis["unknown_edges"], [])

    def test_constructor_alias_resolves_to_its_definition(self):
        analysis = checker.analyze(graph_of(
            node("root", 8), node("src/a.cpp:_ZN1AC2Ev", 16, name="A::A()"), node("_ZN1BD2Ev", 32, name="B::~B()"),
            edge("root", "src/a.cpp:_ZN1AC1Ev"), edge("src/a.cpp:_ZN1AC2Ev", "_ZN1BD1Ev")))
        row = chain_of(analysis, "root")
        self.assertEqual((row["bytes"], row["bound"], row["unknown_edges"]), (56, "complete", []))
        self.assertEqual(analysis["unknown_edges"], [])

    def test_constructor_alias_without_a_definition_stays_unknown(self):
        analysis = checker.analyze(graph_of(node("root", 8), edge("root", "_ZN1AC1Ev")))
        self.assertEqual(chain_of(analysis, "root")["unknown_edges"], ["_ZN1AC1Ev"])

    def test_link_once_copies_keep_the_largest_frame(self):
        first = checker.parse_ci(node("inlineFn", 64, location="include/x.h:3:1"))
        second = checker.parse_ci(node("inlineFn", 96, location="include/x.h:3:1"))
        analysis = checker.analyze(checker.build_graph([([], *first), ([], *second)]))
        self.assertEqual(analysis["frames"][0]["bytes"], 96)

    def test_recursion_is_reported_and_counts_each_member_once(self):
        analysis = checker.analyze(graph_of(
            node("root", 10), node("ping", 20), node("pong", 30), node("self", 40),
            edge("root", "ping"), edge("ping", "pong"), edge("pong", "ping"), edge("self", "self")))
        self.assertEqual(sorted(sorted(group) for group in analysis["recursion"]), [["ping", "pong"], ["self"]])
        root = chain_of(analysis, "root")
        self.assertEqual((root["bytes"], root["bound"]), (60, "lower_bound"))
        self.assertEqual(root["recursion"], ["ping", "pong"])
        self.assertEqual(chain_of(analysis, "self")["bytes"], 40)

    def test_indirect_edge_makes_chain_a_named_lower_bound(self):
        analysis = checker.analyze(graph_of(
            node("root", 8), node("dispatch", 8), node(checker.INDIRECT), edge("root", "dispatch"),
            edge("dispatch", checker.INDIRECT)))
        self.assertEqual(analysis["indirect_calls"], [{"function": "dispatch", "sites": 1}])
        row = chain_of(analysis, "root")
        self.assertEqual((row["bound"], row["indirect_callers"]), ("lower_bound", ["dispatch"]))
        self.assertEqual(checker.graph_gaps(analysis)["indirect_call_sites"], 1)

    def test_unlocated_indirect_record_stays_visible_and_separate(self):
        analysis = checker.analyze(graph_of(node("copier", 8), edge("copier", checker.INDIRECT, site=None)))
        self.assertEqual(analysis["indirect_calls"], [])
        self.assertEqual(analysis["unlocated_indirect_records"], 1)
        row = chain_of(analysis, "copier")
        self.assertEqual((row["bound"], row["indirect_callers"]), ("lower_bound", ["copier [unlocated record]"]))

    def test_undefined_target_is_named_even_without_a_node_record(self):
        analysis = checker.analyze(graph_of(
            node("root", 8), node("sdkCall", name="int sdkCall(int)"), edge("root", "sdkCall"),
            edge("root", "neverDeclared")))
        row = chain_of(analysis, "root")
        self.assertEqual((row["bytes"], row["bound"]), (8, "lower_bound"))
        self.assertEqual(row["unknown_edges"], ["neverDeclared", "sdkCall"])
        self.assertEqual([item["target"] for item in analysis["unknown_edges"]], ["neverDeclared", "sdkCall"])

    def test_edge_from_missing_source_node_is_recorded(self):
        analysis = checker.analyze(graph_of(node("leaf", 8), edge("ghost", "leaf")))
        self.assertEqual(analysis["edge_sources_without_node"], ["ghost"])

    def test_su_frame_without_graph_node_still_counts(self):
        over = checker.FRAME_LIMIT + 1
        analysis = checker.analyze(graph_of(su="src/b.cpp:9:1:void hidden()\t{}\tstatic\n".format(over)))
        self.assertEqual([row["function"] for row in analysis["frame_breaches"]], ["void hidden()"])
        self.assertEqual(len(analysis["frames_without_graph_node"]), 1)
        self.assertEqual(analysis["chains"][0]["bound"], "lower_bound")

    def test_su_qualifier_marks_a_frame_the_graph_called_static(self):
        analysis = checker.analyze(graph_of(node("f", 16), su="src/a.cpp:1:1:f()\t16\tdynamic\n"))
        self.assertEqual(analysis["frames"][0]["qualifier"], "dynamic")


def breach(kind="frame", function="f()", size=3000, target="esp32s3", opt="-Og"):
    return {"target": target, "opt": opt, "kind": kind, "function": function, "file": "src/a.cpp", "bytes": size}


def entry(kind="frame", function="f()", size=3000, targets=("esp32s3",), opt="-Og", reason="known"):
    return {"function": function, "file": "src/a.cpp", "opt": opt, "kind": kind,
            "targets": list(targets), "bytes": size, "reason": reason}


MEASURED = {("esp32s3", "-Og"), ("esp32s3", "-Os")}


class BaselineTests(unittest.TestCase):
    def test_unlisted_breach_fails(self):
        result = checker.evaluate_baseline([breach()], [], MEASURED)
        self.assertTrue(result["failed"])
        self.assertEqual(len(result["new"]), 1)

    def test_identity_includes_kind_level_and_target(self):
        for other in (entry(kind="chain"), entry(opt="-Os"), entry(targets=("esp32s2",)), entry(function="g()")):
            self.assertTrue(checker.evaluate_baseline([breach()], [other], MEASURED)["failed"], other)

    def test_listed_breach_passes(self):
        result = checker.evaluate_baseline([breach()], [entry()], MEASURED)
        self.assertFalse(result["failed"])
        self.assertEqual(len(result["matched"]), 1)

    def test_growth_past_listed_size_fails(self):
        result = checker.evaluate_baseline([breach(size=3001)], [entry()], MEASURED)
        self.assertTrue(result["failed"])
        self.assertEqual(result["grown"][0]["baseline_bytes"], 3000)

    def test_smaller_breach_is_reported_and_passes(self):
        result = checker.evaluate_baseline([breach(size=2500)], [entry()], MEASURED)
        self.assertFalse(result["failed"])
        self.assertEqual(result["reduced"][0]["baseline_bytes"], 3000)

    def test_vanished_breach_is_reported_and_passes(self):
        result = checker.evaluate_baseline([], [entry()], MEASURED)
        self.assertFalse(result["failed"])
        self.assertEqual(result["vanished"][0]["function"], "f()")

    def test_unmeasured_target_is_not_evidence_of_vanishing(self):
        result = checker.evaluate_baseline([], [entry(targets=("esp32s2",))], MEASURED)
        self.assertEqual(result["vanished"], [])

    def test_strict_fails_on_any_entry_even_when_it_vanished(self):
        self.assertTrue(checker.evaluate_baseline([], [entry()], MEASURED, strict=True)["failed"])
        self.assertFalse(checker.evaluate_baseline([], [], MEASURED, strict=True)["failed"])

    def load(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "baseline.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            return checker.load_baseline(path)

    def test_load_rejects_entries_that_cannot_justify_a_breach(self):
        for bad in (entry(reason=" "), entry(kind="total"), entry(opt="-O2"), entry(targets=()),
                    entry(targets=("esp32c3",)), entry(size=0), entry(size=True)):
            with self.assertRaises(checker.StackCheckError, msg=bad):
                self.load({"entries": [bad]})
        with self.assertRaises(checker.StackCheckError):
            self.load({"entries": [entry(), entry()]})
        with self.assertRaises(checker.StackCheckError):
            self.load([])
        self.assertEqual(len(self.load({"entries": [entry()]})), 1)

    def test_generated_entries_group_targets_and_keep_written_reasons(self):
        items = [dict(breach(target=target), largest_frame={"function": "f", "bytes": 1})
                 for target in ("esp32s2", "esp32s3")]
        grouped = checker.baseline_entries(items, [entry(reason="reviewed reason")])
        self.assertEqual(len(grouped), 1)
        self.assertEqual(grouped[0]["targets"], ["esp32s2", "esp32s3"])
        self.assertEqual(grouped[0]["reason"], "reviewed reason")
        different = checker.baseline_entries([breach(target="esp32s2", size=1), breach(target="esp32s3", size=2)])
        self.assertEqual(len(different), 2)

    def test_written_baseline_round_trips(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nested/baseline.json"
            checker.write_baseline(path, [breach()])
            loaded = checker.load_baseline(path)
        result = checker.evaluate_baseline([breach()], loaded, MEASURED)
        self.assertFalse(result["failed"])


class DriftTests(unittest.TestCase):
    SOURCE = "src/a.cpp"

    def database(self, directory, flags):
        path = Path(directory) / "compile_commands.json"
        path.write_text(json.dumps([{"file": "C:\\repo\\src\\a.cpp", "directory": "C:/b",
                                     "command": "g++ -DX=1 @\"gone/cxxflags\" " + " ".join(flags) + " -o -c a.cpp"}]),
                        encoding="utf-8")
        return path

    def real_flags(self):
        hidden = checker.RESPONSE_FLAGS + ["-fno-rtti"]
        return [flag for flag in checker.compile_flags(self.SOURCE, "-Og", [])
                if not flag.startswith("-D") and flag not in hidden]

    def test_matching_real_build_has_no_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            database = self.database(directory, self.real_flags() + ["-fdiagnostics-color=always", "-Wall"])
            self.assertEqual(checker.flag_drift(database, self.SOURCE), [])

    def test_added_removed_and_standard_changes_are_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            database = self.database(directory, self.real_flags() + ["-fstack-protector"])
            self.assertIn("real build adds -fstack-protector", checker.flag_drift(database, self.SOURCE))
            database = self.database(directory, [f for f in self.real_flags() if f != "-fno-jump-tables"])
            self.assertIn("real build lacks -fno-jump-tables", checker.flag_drift(database, self.SOURCE))
            database = self.database(directory, [f for f in self.real_flags() if f != "-std=gnu++2b"])
            differences = checker.flag_drift(database, self.SOURCE)
            self.assertTrue(any("effective standard" in item for item in differences), differences)

    def test_missing_sample_is_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            database = self.database(directory, self.real_flags())
            self.assertTrue(checker.flag_drift(database, "src/other.cpp"))


def run_quiet(argv):
    output = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        code = checker.main(argv)
    return code, output.getvalue()


class MainTests(unittest.TestCase):
    """Exit codes and report status with the compiler boundary replaced by fixtures."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.report = self.base / "report.json"
        self.frame = 64
        patches = [
            mock.patch.object(checker, "repository_state", return_value={"revision": "x", "dirty": False}),
            mock.patch.object(checker, "source_inventory", return_value=(["src/a.cpp"], {"sdk.cpp": "SDK"}, [])),
            mock.patch.object(checker, "source_hashes", return_value={"src/a.cpp": "0"}),
            mock.patch.object(checker, "compile_unit", side_effect=self.compile_unit),
            mock.patch.object(checker, "context_sizes", return_value={"ActionContext": 1}),
        ]
        for patch in patches:
            patch.start()
            self.addCleanup(patch.stop)

    def compile_unit(self, compiler, source, opt, include_dirs, output_dir, number):
        su = output_dir / "unit.su"
        ci = output_dir / "unit.ci"
        su.write_text("src/a.cpp:1:1:f()\t{}\tstatic\n".format(self.frame), encoding="utf-8")
        ci.write_text('graph: { title: "a"\n' + node("f", self.frame) + "\n}\n", encoding="utf-8")
        return su, ci

    def toolchain(self, *missing):
        def finder(target, explicit=None):
            if target in missing:
                return None, target + " not found"
            return checker.Compiler(target, "g++", "gcc", {"version": checker.PINNED_COMPILER}), None
        return mock.patch.object(checker, "find_compiler", side_effect=finder)

    def run_main(self, *extra):
        code, output = run_quiet(["--build-dir", str(self.base / "build"), "--report", str(self.report), *extra])
        return code, output, json.loads(self.report.read_text(encoding="utf-8"))

    def baseline(self, *entries):
        path = self.base / "baseline.json"
        path.write_text(json.dumps({"entries": list(entries)}), encoding="utf-8")
        return str(path)

    def test_pass_within_budget(self):
        with self.toolchain():
            code, output, report = self.run_main()
        self.assertEqual((code, report["status"], report["exit_code"]), (checker.EXIT_PASSED, "passed", 0))
        self.assertIn("sdk.cpp", output)
        self.assertEqual(report["not_measured_sources"], {"sdk.cpp": "SDK"})
        self.assertIn("not measured", report["unmeasured_remainder"])
        self.assertEqual(set(report["measurements"]), set(checker.TARGETS))

    def test_missing_toolchain_is_a_visible_skip_and_not_a_pass(self):
        with self.toolchain(*checker.TARGETS):
            code, output, report = self.run_main()
        self.assertEqual((code, report["status"]), (checker.EXIT_SKIPPED, "skipped"))
        self.assertNotEqual(code, checker.EXIT_PASSED)
        self.assertIn("SKIP, NOT A PASS", output)
        self.assertNotIn("Passed", output)
        self.assertEqual(report["measurements"], {})

    def test_required_toolchain_fails_when_any_compiler_is_missing(self):
        with self.toolchain("esp32s3"):
            code, output, report = self.run_main("--require-toolchain")
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "unavailable"))
        self.assertIn("required compiler unavailable", output)

    def test_partial_toolchain_measures_the_rest_and_still_skips(self):
        with self.toolchain("esp32s3"):
            code, _, report = self.run_main()
        self.assertEqual((code, report["status"]), (checker.EXIT_SKIPPED, "skipped"))
        self.assertEqual(set(report["measurements"]), {"esp32s2"})

    def test_breach_outranks_partial_skip(self):
        self.frame = checker.FRAME_LIMIT + 1
        with self.toolchain("esp32s3"):
            code, _, report = self.run_main()
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))

    def test_breach_fails_without_allow_file(self):
        self.frame = checker.FRAME_LIMIT + 1
        with self.toolchain():
            code, output, report = self.run_main()
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))
        self.assertIn("NEW breach", output)
        self.assertEqual(len(report["baseline"]["new"]), 4)

    def listed(self, size=None):
        size = size or checker.FRAME_LIMIT + 1
        return [entry(size=size, targets=checker.TARGETS, opt=opt) for opt in checker.OPT_FLAGS]

    def test_listed_breach_passes_but_says_budgets_are_not_met(self):
        self.frame = checker.FRAME_LIMIT + 1
        with self.toolchain():
            code, output, report = self.run_main("--allow-file", self.baseline(*self.listed()))
        self.assertEqual((code, report["status"]), (checker.EXIT_PASSED, "passed"))
        self.assertIn("Budgets are not yet met", output)

    def test_strict_fails_on_listed_breach(self):
        self.frame = checker.FRAME_LIMIT + 1
        with self.toolchain():
            code, _, report = self.run_main("--allow-file", self.baseline(*self.listed()), "--strict")
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))

    def test_shrunken_breach_is_reported(self):
        self.frame = checker.FRAME_LIMIT + 1
        with self.toolchain():
            code, output, _ = self.run_main("--allow-file", self.baseline(*self.listed(checker.FRAME_LIMIT + 9)))
        self.assertEqual(code, checker.EXIT_PASSED)
        self.assertIn("smaller than baseline", output)

    def test_vanished_breach_is_reported(self):
        with self.toolchain():
            code, output, _ = self.run_main("--allow-file", self.baseline(*self.listed()))
        self.assertEqual(code, checker.EXIT_PASSED)
        self.assertIn("no longer breaches", output)

    def test_invalid_baseline_fails(self):
        with self.toolchain():
            code, _, report = self.run_main("--allow-file", str(self.base / "absent.json"))
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))

    def test_write_baseline_then_check_passes(self):
        self.frame = checker.FRAME_LIMIT + 1
        path = self.base / "written.json"
        with self.toolchain():
            code, _, _ = self.run_main("--write-baseline", str(path))
            self.assertEqual(code, checker.EXIT_PASSED)
            self.assertEqual(len(checker.load_baseline(path)), 2)
            code, _, _ = self.run_main("--allow-file", str(path))
        self.assertEqual(code, checker.EXIT_PASSED)

    def test_write_baseline_refuses_partial_measurement(self):
        path = self.base / "partial.json"
        with self.toolchain("esp32s3"):
            code, _, report = self.run_main("--write-baseline", str(path))
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))
        self.assertFalse(path.exists())
        with self.toolchain():
            code, _, _ = self.run_main("--write-baseline", str(path), "--opt=-Og")
        self.assertEqual(code, checker.EXIT_FAILED)
        self.assertFalse(path.exists())

    def test_compile_failure_is_a_failure_not_a_skip(self):
        with self.toolchain(), mock.patch.object(checker, "compile_unit",
                                                 side_effect=checker.StackCheckError("compile failed")):
            code, _, report = self.run_main()
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))
        self.assertIn("compile failed", report["error"])

    def test_empty_measurement_fails(self):
        def empty(compiler, source, opt, include_dirs, output_dir, number):
            su = output_dir / "unit.su"
            ci = output_dir / "unit.ci"
            su.write_text("", encoding="utf-8")
            ci.write_text("", encoding="utf-8")
            return su, ci
        with self.toolchain(), mock.patch.object(checker, "compile_unit", side_effect=empty):
            code, _, report = self.run_main()
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))

    def test_real_build_option_drift_fails_before_measuring(self):
        database = self.base / "compile_commands.json"
        database.write_text(json.dumps([]), encoding="utf-8")
        with self.toolchain():
            code, _, report = self.run_main("--compile-commands", "esp32s3=" + str(database))
        self.assertEqual((code, report["status"]), (checker.EXIT_FAILED, "failed"))
        self.assertTrue(report["flag_drift"])
        self.assertNotIn("measurements", report)

    def test_unmeasured_remainder_uses_the_real_stack_size(self):
        self.assertIn("6144 bytes", checker.remainder_text(12288))
        self.assertIn("unknown", checker.remainder_text(None))


class CompilerDiscoveryTests(unittest.TestCase):
    def fake_compiler(self, directory, target, version):
        """A runnable stand-in; never invokes a real toolchain."""
        directory = Path(directory)
        script = directory / "fake_compiler.py"
        script.write_text("import sys\nprint({!r} if '--version' in sys.argv else '')\n".format(version),
                          encoding="utf-8")
        for name in ("g++", "gcc"):
            stem = directory / "xtensa-{}-elf-{}".format(target, name)
            if sys.platform == "win32":
                path = stem.with_name(stem.name + ".bat")
                path.write_text('@"{}" "{}" %*\n'.format(sys.executable, script), encoding="utf-8")
            else:
                path = stem
                path.write_text('#!/bin/sh\nexec "{}" "{}" "$@"\n'.format(sys.executable, script), encoding="utf-8")
                path.chmod(0o755)

    def test_pinned_version_is_accepted_and_hashed(self):
        with tempfile.TemporaryDirectory() as directory:
            self.fake_compiler(directory, "esp32s3", "g++ (crosstool-NG {}) 14.2.0".format(checker.PINNED_COMPILER))
            with mock.patch.object(checker, "toolchain_directories", return_value=[directory]):
                compiler, reason = checker.find_compiler("esp32s3", directory)
        self.assertIsNone(reason)
        self.assertEqual(len(compiler.evidence["sha256"]), 64)
        self.assertIn(checker.PINNED_COMPILER, compiler.evidence["version"])

    def test_other_version_is_rejected_because_frames_would_differ(self):
        with tempfile.TemporaryDirectory() as directory:
            self.fake_compiler(directory, "esp32s3", "g++ (crosstool-NG esp-2021r2-patch5) 8.4.0")
            with mock.patch.object(checker, "toolchain_directories", return_value=[directory]):
                compiler, reason = checker.find_compiler("esp32s3", directory)
        self.assertIsNone(compiler)
        self.assertIn("rejected", reason)

    def test_absent_compiler_gives_a_reason(self):
        with tempfile.TemporaryDirectory() as directory:
            compiler, reason = checker.find_compiler("esp32s2", directory)
        self.assertIsNone(compiler)
        self.assertIn("not found", reason)

    def test_explicit_directory_never_falls_back(self):
        self.assertEqual(checker.toolchain_directories("/only/here"), ["/only/here"])


class InventoryTests(unittest.TestCase):
    def test_real_inventory_measures_core_and_application_and_names_sdk_sources(self):
        measured, not_measured, include_dirs = checker.source_inventory()
        for path in ("src/profiles/lichuan_c/Actions.cpp", "adapters/owner/CanBusOwner.cpp",
                     "adapters/canopennode/SdoClient.cpp", "adapters/twai/TwaiAdapter.cpp",
                     "examples/common/ProbeApp.cpp", "examples/common/DeviceEvidence.cpp"):
            self.assertIn(path, measured)
        self.assertEqual(set(not_measured), set(checker.SDK_ONLY))
        self.assertFalse(set(measured) & set(checker.SDK_ONLY))
        self.assertIn("include", include_dirs)

    def test_firmware_source_outside_the_measured_set_fails(self):
        inventory = (["examples/common/Unlisted.cc"], [])
        with mock.patch.object(checker, "firmware_inventory", return_value=inventory):
            with self.assertRaises(checker.StackCheckError):
                checker.source_inventory()

    def test_real_flags_keep_the_frame_relevant_options(self):
        flags = checker.compile_flags("src/a.cpp", "-Og", [])
        for flag in ("-mlongcalls", "-fno-rtti", "-fno-exceptions", "-Og", "-fno-shrink-wrap"):
            self.assertIn(flag, flags)
        self.assertEqual([f for f in flags if f.startswith("-std=")], ["-std=c++11", "-std=gnu++2b"])
        self.assertIn("-freorder-blocks", checker.compile_flags("src/a.cpp", "-Os", []))
        c_flags = checker.compile_flags("src/a.c", "-Os", [])
        self.assertNotIn("-fno-rtti", c_flags)
        self.assertEqual([f for f in c_flags if f.startswith("-std=")], ["-std=gnu99", "-std=gnu17"])


if __name__ == "__main__":
    unittest.main()
