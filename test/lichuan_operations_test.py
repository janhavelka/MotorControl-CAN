#!/usr/bin/env python3
"""Adversarial source/registration/coverage mutations; no serial or motor I/O."""

from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import check_lichuan_operations as checker


class OperationsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mapping = json.loads((ROOT / checker.MAP_PATH).read_text(encoding="utf-8"))
        cls.ledger = json.loads((ROOT / checker.LEDGER_PATH).read_text(encoding="utf-8"))

    def check(self, mapping=None, ledger=None, edits=None):
        original = Path.read_text

        def read(path, *args, **kwargs):
            relative = path.relative_to(ROOT).as_posix() if path.is_relative_to(ROOT) else str(path)
            if edits and relative in edits:
                return edits[relative]
            return original(path, *args, **kwargs)

        with patch.object(Path, "read_text", read):
            return checker.check(ROOT, self.mapping if mapping is None else mapping,
                                 self.ledger if ledger is None else ledger)

    def assertRejected(self, fragment, **kwargs):
        errors, _ = self.check(**kwargs)
        self.assertTrue(any(fragment in error for error in errors), errors)

    def family(self, mapping, identity):
        return next(f for f in mapping["families"] if f["id"] == identity)

    def test_complete_current_inventory_keeps_unimplemented_and_unqualified_separate(self):
        errors, summary = self.check()
        self.assertEqual([], errors)
        self.assertEqual(202, summary["eds_entries"])
        self.assertEqual(53, summary["manual_obligations"])
        self.assertEqual(202, len(summary["entries"]))
        self.assertEqual(192, summary["coverage"]["read_implemented"])
        self.assertEqual(15, summary["coverage"]["setting_implemented"])
        self.assertFalse(summary["hardware_qualification_inferred"])
        missing = {p["key"] for p in summary["pending"] if p["facet"] == "read"}
        self.assertEqual({"0x605D:00", "0x2301:00", "0x2302:00", "0x607A:00"}, missing)
        for row in summary["entries"]:
            self.assertFalse(row["facets"]["semantic"]["hardware_qualified"])

    def test_new_public_function_is_not_silently_unmapped(self):
        path = "include/MotorControlCAN/profiles/lichuan_c/Reads.h"
        self.assertRejected("function/overload inventory", edits={path: (ROOT / path).read_text() +
                            "\nStatus inventedTypedRead(ReadContext& output);\n"})

    def test_cli_inventory_ignores_braces_and_commas_inside_literals(self):
        source = '''const ConsoleCommand commands[] = {
            {"move", "move {absolute|relative POS} [OPTS]", "bridge", "caps", E::DEVICE_ACTION,
             C::ASYNCHRONOUS, "prose {braces}, and \\"quotes\\"", nullptr, 52},
            {"blocked", "blocked {thing}", "bridge", "caps", E::DEVICE_ACTION,
             C::ASYNCHRONOUS, "still gated", "Q25", 52}
        };
        bool equal(const char*, const char*);
        '''
        commands = checker.command_inventory(source)
        self.assertEqual({"move", "blocked"}, set(commands))
        self.assertTrue(commands["move"]["working"])
        self.assertIn("{absolute|relative POS}", commands["move"]["grammar"])
        self.assertFalse(commands["blocked"]["working"])

    def test_indented_namespace_free_function_is_not_silently_unmapped(self):
        path = "include/MotorControlCAN/profiles/lichuan_c/Reads.h"
        self.assertRejected("function/overload inventory", edits={path: (ROOT / path).read_text() +
                            "\nnamespace Added {\n  Status inventedTypedRead(ReadContext& output);\n}\n"})

    def test_family_only_legacy_schema_cannot_claim_new_entry_reconciliation(self):
        mapping = deepcopy(self.mapping)
        mapping["schema_version"] = 1
        self.assertRejected("requires API-evidence schema 2", mapping=mapping)

    def test_new_overload_is_not_silently_unmapped(self):
        path = "include/MotorControlCAN/profiles/lichuan_c/Reads.h"
        self.assertRejected("function/overload inventory", edits={path: (ROOT / path).read_text() +
                            "\nStatus prepareIdentity(ReadContext& output);\n"})

    def test_removed_public_declaration_invalidates_map(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "Reads")["functions"]["prepareIdentity"] = 2
        self.assertRejected("function/overload inventory", mapping=mapping)

    def test_removed_implementation_fails_even_if_header_and_tests_remain(self):
        path = "src/profiles/lichuan_c/DriverReads.cpp"
        source = (ROOT / path).read_text().replace("Status prepareDriverRead(", "Status staleDriverRead(")
        self.assertRejected("missing implementation for prepareDriverRead", edits={path: source})

    def test_missing_registered_native_tests_rejects_completion_claim(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "DriverSettings")["tests"] = []
        self.assertRejected("no behavioral test evidence", mapping=mapping)

    def test_removed_registration_is_not_replaced_by_a_test_filename(self):
        path = "test/CMakeLists.txt"
        source = (ROOT / path).read_text().replace("add_test(NAME driver_settings COMMAND driver_settings_test)", "")
        self.assertRejected("missing registered behavioral test", edits={path: source})

    def test_commented_registration_is_not_an_executable_test(self):
        path = "test/CMakeLists.txt"
        original = (ROOT / path).read_text()
        command = "add_test(NAME driver_settings COMMAND driver_settings_test)"
        for replacement in ("# " + command, "#[=[\n" + command + "\n]=]"):
            with self.subTest(replacement=replacement):
                self.assertRejected("missing registered behavioral test", edits={
                    path: original.replace(command, replacement)})

    def test_commented_python_registration_is_not_a_campaign_test(self):
        path = "test/CMakeLists.txt"
        source = (ROOT / path).read_text().replace("add_test(NAME bench_campaign_plans ",
                                                  "# add_test(NAME bench_campaign_plans ")
        self.assertRejected("Python scenario test is missing/unregistered", edits={path: source})

    def test_commented_executable_cannot_supply_a_registered_test_source(self):
        path = "test/CMakeLists.txt"
        source = (ROOT / path).read_text().replace(
            "add_executable(driver_settings_test driver_settings_test.cpp)",
            "# add_executable(driver_settings_test driver_settings_test.cpp)")
        self.assertRejected("missing registered behavioral test", edits={path: source})

    def test_cmake_comment_scanner_preserves_hash_in_quoted_arguments(self):
        source = 'set(label "keep # this") # remove this\n#[=[\nadd_test(NAME fake COMMAND fake)\n]=]\n'
        cleaned = checker.cmake_source(source)
        self.assertIn('"keep # this"', cleaned)
        self.assertNotIn("remove this", cleaned)
        self.assertNotIn("add_test", cleaned)

    def test_test_success_stub_has_no_reachable_asserted_behavior(self):
        self.assertRejected("no reachable assertions", edits={
            "test/driver_settings_test.cpp": "int main() { return 0; }\n"})

    def test_comments_and_strings_do_not_prove_calls(self):
        path = "test/driver_settings_test.cpp"
        self.assertRejected("no reachable native test call: prepareDriverRead", edits={path:
            '// prepareDriverRead(context);\nint main() { CHECK(true); const char* label = "prepareDriverRead()"; }\n'})

    def test_unused_asserted_fixture_does_not_prove_executed_call(self):
        path = "test/driver_settings_test.cpp"
        self.assertRejected("no reachable native test call: prepareDriverRead", edits={path:
            "void unused() { CHECK(prepareDriverRead(context)); }\nint main() { CHECK(true); }\n"})

    def test_missing_console_command_is_rejected(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "Actions")["console"][0]["command"] = "stop-missing"
        self.assertRejected("CLI route absent or gated", mapping=mapping)

    def test_actual_console_gate_invalidates_claimed_route(self):
        path = "examples/common/ProbeConsole.cpp"
        source = (ROOT / path).read_text()
        start = source.index('{"stop", "stop normal|fast | stop ID|all [normal|fast]"')
        end = source.index('},', start)
        source = source[:start] + source[start:end].replace('nullptr', '"blocked"') + source[end:]
        self.assertRejected("CLI route absent or gated: stop", edits={path: source})

    def test_inventory_description_does_not_replace_native_bridge(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "Actions")["console"][0]["implementation"] = ["examples/common/ProbeConsole.cpp"]
        self.assertRejected("CLI bridge does not call public entry prepareStop", mapping=mapping)

    def test_descriptor_does_not_complete_typed_read(self):
        mapping = deepcopy(self.mapping)
        mapping["ledger_bindings"]["read"]["19"] = "ObjectDescriptors"
        self.assertRejected("descriptor/codec is not a typed read implementation", mapping=mapping)

    def test_binding_cannot_borrow_unrelated_family_entry_evidence(self):
        mapping = deepcopy(self.mapping)
        mapping["ledger_bindings"]["read"]["19"]["family"] = "DriverSettings"
        self.assertRejected("public entries do not belong to the selected family", mapping=mapping)

    def test_descriptor_does_not_complete_manual_workflow(self):
        mapping = deepcopy(self.mapping)
        mapping["ledger_bindings"]["manual"]["enable_transition"] = {
            "family": "ObjectDescriptors", "role": "workflow",
            "entries": ["ObjectDescriptors.readDescriptor"]}
        self.assertRejected("descriptor/codec is not a typed workflow implementation", mapping=mapping)

    def test_binding_requires_real_public_entry_in_its_named_family(self):
        mapping = deepcopy(self.mapping)
        mapping["ledger_bindings"]["manual"]["enable_transition"]["entries"] = ["Actions.prepareMissing"]
        self.assertRejected("absent public entry Actions.prepareMissing", mapping=mapping)

    def test_manual_workflow_entry_evidence_retains_implemented_facet(self):
        ledger = deepcopy(self.ledger)
        row = next(row for row in ledger["manual_only"] if row["key"] == "enable_transition")
        del row["operation_coverage"]
        self.assertRejected("lost its implemented operation facet", ledger=ledger)

    def test_operation_hardware_qualification_cannot_use_an_arbitrary_evidence_label(self):
        ledger = deepcopy(self.ledger)
        row = next(row for row in ledger["manual_only"] if row["key"] == "enable_transition")
        row["operation_coverage"]["hardware_qualified"] = True
        row["operation_qualification_evidence"] = "invented"
        self.assertRejected("prompts 47/48 must establish retained qualification evidence", ledger=ledger)

    def test_working_but_unrelated_command_is_not_a_cli_route(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "Actions")["console"][0]["command"] = "profile"
        self.assertRejected("CLI API expression differs", mapping=mapping)

    def test_unrelated_call_in_bridge_file_does_not_prove_dispatch_reachability(self):
        mapping = deepcopy(self.mapping)
        route = self.family(mapping, "Axis")["console"][0]
        route["entry"] = "configureAxis"
        # ProbeAppLocal.cpp contains this call only in configureHostAxis, which
        # the console's offline executeHostAxis path does not call.
        self.assertRejected("CLI bridge does not call public entry configureAxis", mapping=mapping)

    def test_application_bridge_must_be_called_by_the_actual_console_dispatch(self):
        path = "examples/common/ProbeConsole.cpp"
        source = (ROOT / path).read_text().replace("app_.requestOrdinaryRead(", "app_.staleOrdinaryRead(")
        self.assertRejected("console dispatch does not call its declared application bridge", edits={path: source})

    def test_getter_bridge_must_call_the_native_getter_despite_sharing_its_name(self):
        source = "bool ProbeApp::getIo() { return true; }\n"
        self.assertFalse(checker.cli_reaches_entry([source], "ProbeApp::getIo", "getIo"))
        source = "bool ProbeApp::getIo() { return L::getIo(context); }\n"
        self.assertTrue(checker.cli_reaches_entry([source], "ProbeApp::getIo", "getIo"))

    def test_original_eds_obligation_cannot_be_deleted(self):
        ledger = deepcopy(self.ledger)
        ledger["objects"][-1]["entries"].pop()
        self.assertRejected("EDS typed entries not accounted exactly once", ledger=ledger)

    def test_original_manual_obligation_cannot_be_deleted(self):
        ledger = deepcopy(self.ledger)
        ledger["manual_only"].pop()
        self.assertRejected("manual obligation inventory incomplete", ledger=ledger)

    def test_pending_manual_mapping_cannot_be_deleted(self):
        mapping = deepcopy(self.mapping)
        del mapping["ledger_bindings"]["manual"]["termination_and_cabling"]
        self.assertRejected("manual API/pending inventory differs", mapping=mapping)

    def test_implemented_manual_workflow_requires_actual_public_family(self):
        ledger, mapping = deepcopy(self.ledger), deepcopy(self.mapping)
        next(row for row in ledger["manual_only"] if row["key"] == "enable_transition")["operation_coverage"] = {
            "documented": True, "implemented": True, "host_tested": True, "hardware_qualified": False}
        mapping["ledger_bindings"]["manual"]["enable_transition"] = None
        self.assertRejected("implemented manual workflow has no public family", mapping=mapping, ledger=ledger)

    def test_conditional_workflow_cannot_silently_promote_gated_semantics(self):
        ledger = deepcopy(self.ledger)
        row = next(r for r in ledger["manual_only"] if r["key"] == "enable_transition")
        row["coverage"]["implemented"] = row["coverage"]["host_tested"] = True
        self.assertRejected("unresolved semantic gates cannot be silently promoted", ledger=ledger)

    def test_operation_facet_rejects_nonboolean_or_missing_evidence_fields(self):
        for change in ({"implemented": "true"}, {"documented": True, "implemented": True,
                                               "host_tested": True}):
            with self.subTest(change=change):
                self.assertTrue(checker.operation_facet_errors({"key": "fixture", "operation_coverage": change}))

    def test_malformed_operation_facet_reports_error_without_crashing_reconciliation(self):
        ledger = deepcopy(self.ledger)
        ledger["manual_only"][0]["operation_coverage"] = True
        self.assertRejected("four boolean evidence fields", ledger=ledger)

    def test_operation_facet_requires_scope_and_consistent_evidence(self):
        base = {"key": "fixture", "operation_coverage": {"documented": True, "implemented": True,
                 "host_tested": True, "hardware_qualified": False}}
        self.assertIn("explicit operation_scope", " ".join(checker.operation_facet_errors(base)))
        base["operation_scope"] = "Conditional public workflow; source and hardware gates remain open."
        self.assertEqual([], checker.operation_facet_errors(base))
        base["operation_coverage"]["hardware_qualified"] = True
        self.assertIn("retained device evidence", " ".join(checker.operation_facet_errors(base)))
        base["operation_coverage"]["host_tested"] = False
        self.assertIn("requires host testing", " ".join(checker.operation_facet_errors(base)))

    def test_operation_map_cannot_become_a_second_device_dictionary(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "Reads")["access_reviewed"] = "RW"
        self.assertRejected("cannot duplicate device facts", mapping=mapping)

    def test_missing_api_only_rationale_is_rejected(self):
        mapping = deepcopy(self.mapping)
        del self.family(mapping, "Native")["console_api_only_reason"]
        self.assertRejected("no CLI route or concrete API-only reason", mapping=mapping)

    def test_missing_python_scenario_is_rejected(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "Velocity")["python_scenarios"] = []
        self.assertRejected("Python scenario catalogue differs", mapping=mapping)

    def test_python_family_claim_requires_its_ordinary_cli_route(self):
        mapping = deepcopy(self.mapping)
        self.family(mapping, "Reads")["python_scenarios"] = []
        self.family(mapping, "Reads")["python_api_only_reason"] = "Use the native read API."
        self.family(mapping, "Capabilities")["python_scenarios"] = ["quick"]
        self.assertRejected("Python scenario does not exercise a mapped family CLI route", mapping=mapping)

    def test_working_python_command_cannot_replace_the_claimed_family_plan(self):
        path = "scripts/bench_scenarios.py"
        original = (ROOT / path).read_text()
        self.assertIn('f"read identity ', original)
        source = original.replace('f"read identity ', 'f"profile list ')
        self.assertRejected("Python scenario does not exercise a mapped family CLI route", edits={path: source})

    def test_python_plan_is_built_and_rejects_absent_required_parameters(self):
        mapping = deepcopy(self.mapping)
        mapping["python_plans"]["velocity"] = []
        self.assertRejected("Python offline plan failed", mapping=mapping)

    def test_python_campaign_missing_tests_is_rejected(self):
        mapping = deepcopy(self.mapping)
        mapping["python_tests"] = []
        self.assertRejected("require registered offline behavioral tests", mapping=mapping)

    def test_function_scanner_does_not_count_generated_data(self):
        declarations = checker.public_functions('''
constexpr ObjectDescriptor OBJECT_1234_00 = {0x1234, 0, ScalarType::U16};
// Status invented();
const char* words = "prepareHome()";
inline bool readDescriptor(uint16_t index) { return index != 0; }
Status prepareHome(HomeContext& output);
Status prepareHome(HomeContext& output, int method);
''')
        self.assertEqual({"readDescriptor": 1, "prepareHome": 2}, dict(declarations))

    def test_free_function_scanner_excludes_members_independently_of_indentation(self):
        declarations = checker.public_functions('''
struct Value {
Status member();
struct Nested { bool nested() { return helper(); } };
};
class Other {
public:
    Status another() { return helper(); }
};
namespace API {
  Status prepareHome();
  inline Status checked() { return helper(); }
}
''')
        self.assertEqual({"prepareHome": 1, "checked": 1}, dict(declarations))


if __name__ == "__main__":
    unittest.main()
