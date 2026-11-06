#!/usr/bin/env python3
"""Contract tests for rs_move_vectors.py and its retained file, without RS, git or a compiler."""

import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import rs_move_vectors as tool

TEXT = tool.VECTORS.read_text(encoding="utf-8")


def produced_from(retained):
    return [(kind, list(values), 0) for kind, values in retained.cases.values()]


def replaced(retained, name, column, new):
    rows = produced_from(retained)
    for kind, values, _ in rows:
        if values[0] == name:
            values[tool.COLUMNS[kind].index(column)] = new
    return rows


class RetainedFileTests(unittest.TestCase):
    def setUp(self):
        self.retained = tool.Retained(TEXT)

    def test_records_a_pinned_revision_and_compiled_rs_sources(self):
        self.assertRegex(self.retained.revision, r"^[0-9a-f]{40}$")
        self.assertEqual(self.retained.translation_units(), ["src/Axis.cpp", "src/Units.cpp"])
        self.assertIn("include/MotorControlRS/Axis.h", self.retained.sources)
        self.assertTrue(any(kind == "RS_MOVE_REFUSAL" for kind, _ in self.retained.cases.values()))

    def test_both_consumers_include_the_retained_file(self):
        test = (tool.ROOT / "test/move_common_test.cpp").read_text(encoding="utf-8")
        self.assertEqual(test.count('#include "vectors/rs_common_intent.inc"'), 2)
        producer = tool.PRODUCER.read_text(encoding="utf-8")
        self.assertEqual(producer.count('#include "rs_common_intent.inc"'), 2)

    def test_every_case_row_round_trips_through_the_producer_format(self):
        rows = [line.strip() for line in TEXT.splitlines()
                if line.startswith(("RS_MOVE_VECTOR(", "RS_MOVE_REFUSAL("))]
        self.assertEqual(len(rows), len(self.retained.cases))
        for line in rows:
            ((kind, values, _),) = tool.parse_rows(line, tool.COLUMNS)
            self.assertEqual(tool.format_row(kind, values), line)

    def test_an_identical_candidate_reproduces_the_file(self):
        closure = set(self.retained.sources)
        text = tool.candidate_text(self.retained, self.retained.revision, closure, dict(self.retained.sources),
                                   produced_from(self.retained))
        self.assertEqual(text.splitlines(), TEXT.splitlines())

    def test_malformed_files_are_rejected(self):
        revision = 'RS_MOVE_REVISION("' + self.retained.revision + '")'
        cases = (
            TEXT.replace(revision, 'RS_MOVE_REVISION("cd27b12")'),
            TEXT.replace(', 2500, 3500, 0)', ', 2500, 3500)'),
            TEXT.replace('RS_MOVE_VECTOR("5/2 steps"', 'RS_MOVE_VECTOR("7/2 steps"'),
            TEXT.replace('"servo -1/3 turn", "servo"', '"servo -1/3 turn", "gantry"'),
            TEXT.replace('RS_MOVE_AXIS("servo", 10000, 1, 1, 1, 0, 1, 0)', 'RS_MOVE_AXIS("servo", 10000, 0, 1, 1, 0, 1, 0)'),
            TEXT + 'RS_MOVE_UNKNOWN("x")\n',
        )
        for text in cases:
            self.assertNotEqual(text, TEXT)
            with self.assertRaises(ValueError):
                tool.Retained(text)


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.retained = tool.Retained(TEXT)

    def test_identical_rows_have_no_difference(self):
        self.assertEqual(tool.compare_cases(self.retained, produced_from(self.retained)), [])

    def test_output_and_input_changes_are_named(self):
        output = tool.compare_cases(self.retained, replaced(self.retained, "7/2 steps", "errorSixths", -3))
        self.assertEqual(output, ["'7/2 steps' errorSixths (RS output): retained 3, RS -3"])
        echo = tool.compare_cases(self.retained, replaced(self.retained, "7/2 steps", "witness", 1))
        self.assertEqual(echo, ["'7/2 steps' witness (input echo): retained 0, RS 1"])
        code = tool.compare_cases(self.retained, replaced(self.retained, "servo 1 full step", "code", "OK"))
        self.assertEqual(code, ["'servo 1 full step' code (RS output): retained 'INVALID_CONFIG', RS 'OK'"])

    def test_refused_missing_extra_and_error_rows_are_differences(self):
        rows = [row for row in produced_from(self.retained) if row[1][0] not in ("1 full step", "5/2 steps")]
        rows.append(("RS_MOVE_VECTOR_REFUSED", ["1 full step", "INVALID_CONFIG", 102], 0))
        rows.append(("RS_MOVE_ERROR", ["5/2 steps", "rounding error is not a whole number of sixths"], 0))
        rows.append(rows[0])
        rows.append(("RS_MOVE_VECTOR", ["new case"] + rows[0][1][1:], 0))
        found = tool.compare_cases(self.retained, rows)
        self.assertIn("'1 full step': retained as accepted, RS refuses it with INVALID_CONFIG detail 102", found)
        self.assertIn("'5/2 steps': rounding error is not a whole number of sixths", found)
        self.assertIn(f"{rows[0][1][0]!r}: the producer printed it twice", found)
        self.assertIn("'new case': the producer printed a case the file does not retain", found)
        self.assertEqual(len(found), 4)

    def test_source_closure_and_blob_drift_are_differences(self):
        recorded = {"src/Axis.cpp": "a" * 40, "include/MotorControlRS/Old.h": "b" * 40}
        blobs = {"src/Axis.cpp": "c" * 40, "include/MotorControlRS/New.h": "d" * 40}
        found = tool.compare_sources(recorded, {"src/Axis.cpp", "include/MotorControlRS/New.h"}, blobs)
        self.assertEqual(found, [
            "source include/MotorControlRS/New.h: compiled but not recorded (blob " + "d" * 40 + ")",
            "source include/MotorControlRS/Old.h: recorded but not compiled",
            "source src/Axis.cpp: recorded blob " + "a" * 40 + ", compiled " + "c" * 40,
        ])
        self.assertEqual(tool.compare_sources(recorded, set(recorded), recorded), [])

    def test_dependency_files_keep_only_rs_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "rs tree"
            outside = (Path(directory) / "build/rs_common_intent.inc").as_posix()
            text = ("unit1.o: " + (base / "src/Axis.cpp").as_posix().replace(" ", "\\ ") + " \\\r\n"
                    " " + (base / "include/MotorControlRS/Axis.h").as_posix().replace(" ", "\\ ") + " \\\n"
                    " " + outside + "\n")
            self.assertEqual(tool.dependencies(text, base), {"src/Axis.cpp", "include/MotorControlRS/Axis.h"})


class MainTests(unittest.TestCase):
    def run_main(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = tool.main(argv)
        return status, out.getvalue(), err.getvalue()

    def test_a_difference_is_reported_and_the_retained_file_is_unchanged(self):
        retained = tool.Retained(TEXT)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "rs/.git").mkdir(parents=True)
            vectors = root / "rs_common_intent.inc"
            vectors.write_bytes(TEXT.encode("utf-8"))
            rows = replaced(retained, "+1/3 motor turn", "effective", 1066)
            output = "\n".join(tool.format_row(kind, values) for kind, values, _ in rows) + "\n"
            blobs = dict(retained.sources)
            with mock.patch.object(tool, "git", return_value=retained.revision + "\n"), \
                    mock.patch.object(tool, "acquire_revision", return_value=(retained.revision, blobs)), \
                    mock.patch.object(tool, "build_and_run", return_value=(set(blobs), output)):
                status, out, _ = self.run_main(["--rs", str(root / "rs"), "--vectors", str(vectors)])
            self.assertEqual(status, 1)
            self.assertIn("AUDIT DIFFERENCE: '+1/3 motor turn' effective (RS output): retained 1067, RS 1066", out)
            self.assertEqual(vectors.read_bytes(), TEXT.encode("utf-8"))

    def test_setup_problems_are_not_audit_results(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            vectors = root / "rs_common_intent.inc"
            vectors.write_text(TEXT, encoding="utf-8")
            status, _, err = self.run_main(["--rs", str(root), "--vectors", str(vectors)])
            self.assertEqual(status, 2)
            self.assertIn("is not a MotorControl-RS Git checkout", err)
            status, _, err = self.run_main(["--rs", str(root), "--vectors", str(vectors), "--candidate", str(vectors)])
            self.assertEqual(status, 2)
            self.assertIn("must not replace the retained file", err)
            self.assertEqual(vectors.read_text(encoding="utf-8"), TEXT)


if __name__ == "__main__":
    unittest.main()
