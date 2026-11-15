#!/usr/bin/env python3
"""Finite campaign intent, prerequisite refusals and no-replay orchestration.

Byte framing, native terminal interpretation and artifact failures have separate
Console/session tests. These fakes exercise ordinary campaign command ownership.
"""
from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import bench_probe as probe
import bench_motion as motion
import bench_repeat as repeat
import bench_scenarios as scenarios
import bench_session as session


POSITION = ["--target", "100", "--kind", "relative", "--rpm", "60",
            "--ramp-up-ms", "100", "--ramp-down-ms", "200", "--setup", "verify",
            "--stop-policy", "fast"]
REPEAT = ["--target", "100", "--rpm", "60", "--ramp-up-ms", "100",
          "--ramp-down-ms", "200", "--stop-policy", "fast"]


def fixture():
    return dict(protocol="canopen", profile="lichuan_c", model="CL86-C", firmware="specimen-1",
                dialect="selected specimen record", evidence="host test fixture, not qualification",
                capabilities=dict(position=True, velocity=True, homing=True, stop=True, settings=True),
                stop=dict(method="fast", evidence="fixture stop evidence"),
                command_scale=dict(evidence="fixture command scale"),
                reference=dict(evidence="fixture reference"),
                io=dict(method=17, home_inputs_permit=True, evidence="fixture I/O"),
                heartbeat=dict(evidence="fixture heartbeat"), pdo_map=dict(evidence="fixture map"),
                configuration=dict(evidence="fixture configuration"))


class CampaignTests(unittest.TestCase):
    def args(self, name="quick", options=()):
        return scenarios.arguments(["--plan-only", "--scenario", name] + list(options))

    def run_fake(self, scenario, *, fail_at=None, cleanup_ok=True, preflight_error=None):
        calls, record = [], {"fixture": fixture()}

        def checked(console, command, **kwargs):
            self.assertIs(kwargs["record"], record)
            calls.append(command)
            if command.split()[0] in ("move", "velocity", "home", "stop"):
                record["motion_handoff"] = True
            if command == fail_at:
                raise probe.BenchError("primary uncertain")
            return dict(ok=True, operation_id=len(calls), completed_us="9007199254740993")

        def cleanup(console, target, **kwargs):
            self.assertIs(target, record)
            calls.append("cleanup " + kwargs["stop"])
            record["cleanup_observed"] = cleanup_ok
            return cleanup_ok

        with patch.object(probe, "checked", side_effect=checked), \
                patch.object(probe, "cleanup_stop_attempts", side_effect=cleanup), \
                patch.object(probe, "require_prerequisites", side_effect=preflight_error):
            try:
                scenario.run(object(), record)
                error = None
            except BaseException as failure:
                error = failure
        return calls, record, error

    def test_default_quick_is_read_only_and_has_no_cleanup(self):
        scenario = scenarios.build_scenario(self.args())
        self.assertEqual(scenario.name, "quick")
        calls, record, error = self.run_fake(scenario)
        self.assertIsNone(error)
        self.assertEqual(calls, ["read identity 1000"])
        self.assertTrue(record["workload_verified"])
        self.assertEqual(scenario.plan["evidence_kind"], "planned")
        self.assertFalse(scenario.plan["hardware_qualified"])

    def test_all_six_classes_have_explicit_finite_public_paths(self):
        cases = (("position", POSITION, "move relative", "cleanup fast"),
                 ("stop", ["--stop-policy", "fast"], "stop fast", None),
                 ("velocity", ["--rpm", "60", "--duration-ms", "1000", "--stop-policy", "fast"], "velocity 60", None),
                 ("homing", ["--method", "17", "--search-rpm", "60", "--return-rpm", "30",
                             "--home-ramp-ms", "100", "--stop-policy", "fast"], "home 17", "cleanup fast"),
                 ("settings", ["--driver-settings", "60", "100", "200"], "driver set 60", None))
        for name, options, prefix, cleanup in cases:
            with self.subTest(name=name):
                scenario = scenarios.build_scenario(self.args(name, options))
                calls, record, error = self.run_fake(scenario)
                self.assertIsNone(error)
                self.assertTrue(any(call.startswith(prefix) for call in calls))
                self.assertEqual([call for call in calls if call.startswith("cleanup")], [cleanup] if cleanup else [])
                self.assertTrue(record["workload_verified"])
                self.assertFalse(any(call.startswith(("enable", "persistence", "recover", "control")) for call in calls))

    def test_uncertain_primary_is_never_replayed_or_rewritten_by_cleanup(self):
        scenario = scenarios.build_scenario(self.args("position", POSITION))
        move = scenario.commands[-1]
        calls, record, error = self.run_fake(scenario, fail_at=move)
        self.assertEqual(str(error), "primary uncertain")
        self.assertEqual(calls.count(move), 1)
        self.assertEqual(calls[-1], "cleanup fast")
        self.assertTrue(record["cleanup_observed"])
        self.assertFalse(record["workload_verified"])

    def test_cleanup_failure_does_not_mask_primary_and_fails_successful_work(self):
        scenario = scenarios.build_scenario(self.args("position", POSITION))
        _, record, error = self.run_fake(scenario, fail_at=scenario.commands[-1], cleanup_ok=False)
        self.assertEqual(str(error), "primary uncertain")
        _, record, error = self.run_fake(scenario, cleanup_ok=False)
        self.assertIn("planned stop", str(error))
        self.assertFalse(record["workload_verified"])

    def test_prerequisite_refusal_precedes_all_campaign_commands(self):
        scenario = scenarios.build_scenario(self.args("position", POSITION))
        calls, record, error = self.run_fake(scenario, preflight_error=probe.BenchError("wrong profile"))
        self.assertEqual(str(error), "wrong profile")
        self.assertEqual(calls, [])
        self.assertNotIn("workload_verified", record)

    def test_home_method_and_stop_policy_must_match_supplied_evidence(self):
        scenario = scenarios.build_scenario(self.args("homing", ["--method", "18", "--search-rpm", "60",
            "--return-rpm", "30", "--home-ramp-ms", "100", "--stop-policy", "fast"]))
        with self.assertRaisesRegex(ValueError, "method-specific"):
            scenario.validate_fixture(fixture())
        candidate = fixture()
        candidate["io"]["method"] = 18
        candidate["io"]["home_inputs_permit"] = False
        with self.assertRaisesRegex(ValueError, "home_inputs_permit"):
            scenario.validate_fixture(candidate)
        candidate["io"]["home_inputs_permit"] = True
        candidate["stop"]["method"] = "normal"
        with self.assertRaisesRegex(ValueError, "stop method"):
            scenario.validate_fixture(candidate)

    def test_repeat_is_bounded_write_verify_moves_with_one_cleanup(self):
        scenario = repeat.build_scenario(repeat.arguments(["--plan-only", "--count", "3"] + REPEAT))
        calls, record, error = self.run_fake(scenario)
        self.assertIsNone(error)
        self.assertEqual(sum(call.startswith("move ") for call in calls), 6)
        self.assertEqual(calls.count("cleanup fast"), 1)
        self.assertEqual(record["comparison"], dict(write=3, verify=3))
        self.assertEqual([trial["setup"] for trial in record["trials"]], ["write", "verify"] * 3)
        self.assertEqual(record["trials"][0]["terminal"]["completed_us"], "9007199254740993")
        self.assertFalse(any("restore" in call or "driver" in call or "persistence" in call for call in calls))

    def test_repeat_stops_on_first_failed_move_and_never_replays_it(self):
        scenario = repeat.build_scenario(repeat.arguments(["--plan-only", "--count", "20"] + REPEAT))
        move = scenario.commands[3]
        calls, record, error = self.run_fake(scenario, fail_at=move)
        self.assertEqual(str(error), "primary uncertain")
        self.assertEqual(sum(call.startswith("move ") for call in calls), 2)
        self.assertEqual(len(record["trials"]), 1)
        self.assertEqual(calls[-1], "cleanup fast")

    def test_large_integer_target_and_aggregate_remain_exact(self):
        options = REPEAT.copy()
        options[options.index("--target") + 1] = "9007199254740993"
        scenario = repeat.build_scenario(repeat.arguments(["--plan-only", "--count", "2"] + options))
        self.assertIn("9007199254740993", scenario.commands[2])
        self.assertEqual(scenario.inputs["total_requested_displacement"], "36028797018963972")

    def test_crossed_unknown_unbounded_and_unsafe_arguments_fail_offline(self):
        cases = ((scenarios.arguments, ["--plan-only", "--target", "1"]),
                 (scenarios.arguments, ["--plan-only", "--scenario", "settings", "--driver-settings", "60", "bad", "200"]),
                 (scenarios.arguments, ["--plan-only", "--scenario", "position"]),
                 (scenarios.arguments, ["--plan-only", "--timeout", "nan"]),
                 (motion.arguments, ["--list"]), (repeat.arguments, ["--list"]),
                 (repeat.arguments, ["--plan-only", "--scenario", "settings"] + REPEAT),
                 (repeat.arguments, ["--plan-only", "--count", "21"] + REPEAT),
                 (repeat.arguments, ["--plan-only", "--target", "1e3"] + REPEAT[2:]),
                 (repeat.arguments, ["--plan-only", "--target", "9223372036854775808"] + REPEAT[2:]))
        for parser, argv in cases:
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    parser(argv)

    def test_direct_plan_values_reject_boolean_target_and_unbounded_count(self):
        args = self.args("position", POSITION)
        args.target = True
        with self.assertRaisesRegex(ValueError, "integer range"):
            scenarios.build_scenario(args)
        args = repeat.arguments(["--plan-only"] + REPEAT)
        args.count = 0
        with self.assertRaisesRegex(ValueError, "count"):
            repeat.build_scenario(args)

    def test_plan_and_list_do_not_call_recorded_session_or_create_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = str(Path(directory) / "unused")
            for entry, argv in ((scenarios.main, ["--list"]),
                                (scenarios.main, ["--plan-only", "--out", destination]),
                                (motion.main, ["--plan-only", "--out", destination]),
                                (repeat.main, ["--plan-only", "--out", destination] + REPEAT)):
                with self.subTest(entry=entry.__module__), patch.object(session, "run_recorded") as execute, \
                        redirect_stdout(io.StringIO()):
                    self.assertEqual(entry(argv), 0)
                    execute.assert_not_called()
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_malformed_fixture_corpus_is_input_failure_before_artifacts_or_serial(self):
        corpus = {
            "deep_array": b"[" * 2000 + b"0" + b"]" * 2000,
            "deep_object": b'{"a":' * 1500 + b"0" + b"}" * 1500,
            "over_contract_depth": b"[" * 18 + b"0" + b"]" * 18,
            "duplicate": b'{"protocol":"canopen","protocol":"other"}',
            "nonfinite": b'{"value":1e999}',
            "non_ascii_encoding": b"\xff",
            "partial": b'{"protocol":',
            "oversize": b" " * (session.MAX_FIXTURE_BYTES + 1),
        }
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "fixture.json"
            destination = Path(directory) / "unused"
            for name, data in corpus.items():
                with self.subTest(fixture=name):
                    source.write_bytes(data)
                    stderr = io.StringIO()
                    with patch.object(session, "run_recorded") as execute, \
                            patch.object(probe, "open_port") as open_port, \
                            redirect_stdout(io.StringIO()), redirect_stderr(stderr):
                        self.assertEqual(session.main(["--plan-only", "--fixture", str(source),
                                                       "--out", str(destination)]), 2)
                    execute.assert_not_called()
                    open_port.assert_not_called()
                    self.assertTrue(stderr.getvalue().strip())
                    self.assertEqual(source.read_bytes(), data)
                    self.assertEqual(list(Path(directory).iterdir()), [source])


if __name__ == "__main__":
    unittest.main()
