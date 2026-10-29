#!/usr/bin/env python3
"""Prove the real strict Doxygen configuration rejects public documentation errors."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import doxygen_filter  # noqa: E402
from verify import DOXYGEN_VERSION, doxygen_matches  # noqa: E402

HEADER = """/** @file fixture.h
 * @brief Temporary public API used to test the documentation gate.
 */
#pragma once
/** @brief Temporary validation scope. */
namespace Fixture {
/** @brief Return a validated value.
 * @param[in] value Caller-owned nonnegative integer, borrowed during the call.
 * @return The supplied value; no I/O, retained reference or shared state.
 */
int validate(int value);
}
"""


class DocumentationGateTests(unittest.TestCase):
    doxygen = "doxygen"

    def run_doxygen(self, header: str) -> tuple[int, str, str]:
        # The path exercises spaces and a "test" segment without mutating any
        # repository input. Broad exclusion patterns must not hide this header.
        with tempfile.TemporaryDirectory(prefix="motorcontrol docs ") as temporary:
            workspace = pathlib.Path(temporary) / "test" / "MotorControl-CAN"
            workspace.mkdir(parents=True)
            (workspace / "fixture.h").write_text(header, encoding="utf-8")
            configuration = (ROOT / "Doxyfile").read_text(encoding="utf-8")
            configuration += """
INPUT = fixture.h
USE_MDFILE_AS_MAINPAGE =
OUTPUT_DIRECTORY = output
GENERATE_HTML = NO
WARN_LOGFILE =
"""
            (workspace / "Doxyfile").write_text(configuration, encoding="utf-8")
            command = [self.doxygen, "Doxyfile"]
            result = subprocess.run(command, cwd=workspace, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, timeout=60,
                                    encoding="utf-8", errors="replace", check=False)
            print(f"Doxygen {DOXYGEN_VERSION}: command={command!r} "
                  f"cwd={workspace} status={result.returncode}")
            if result.stdout:
                print(result.stdout, end="")
            index = workspace / "output/xml/index.xml"
            xml = index.read_text(encoding="utf-8") if index.is_file() else ""
            return result.returncode, result.stdout, xml

    def test_documented_public_function_is_extracted(self) -> None:
        status, diagnostic, xml = self.run_doxygen(HEADER)
        self.assertEqual(status, 0, diagnostic)
        self.assertIn("<name>validate</name>", xml)

    def test_missing_parameter_documentation_fails(self) -> None:
        header = HEADER.replace(
            " * @param[in] value Caller-owned nonnegative integer, borrowed during the call.\n", "")
        status, diagnostic, _ = self.run_doxygen(header)
        self.assertNotEqual(status, 0, "Missing parameter documentation passed")
        self.assertIn("Fixture::validate", diagnostic)
        self.assertIn("not documented", diagnostic)

    def test_unresolved_public_reference_fails(self) -> None:
        header = HEADER.replace("Return a validated value.",
                                "Return a validated value. See @ref MissingPublicContract.")
        status, diagnostic, _ = self.run_doxygen(header)
        self.assertNotEqual(status, 0, "Unresolved public reference passed")
        self.assertIn("MissingPublicContract", diagnostic)
        self.assertIn("resolve", diagnostic)

    def test_undocumented_public_member_fails(self) -> None:
        header = HEADER.replace("int validate(int value);", """int validate(int value);
/** @brief Temporary public value. */
struct Value {
    int undocumented;
};""")
        status, diagnostic, _ = self.run_doxygen(header)
        self.assertNotEqual(status, 0, "Undocumented public member passed")
        self.assertIn("undocumented", diagnostic)
        self.assertIn("not documented", diagnostic)


class MarkdownFilterTests(unittest.TestCase):
    def test_repository_link_and_anchor_render_as_source_link(self) -> None:
        text = "[contracts](../docs/prompts/CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence)\n"
        rendered = doxygen_filter.render_markdown(text, ROOT / "docs/verification.md")
        self.assertIn("https://github.com/janhavelka/MotorControl-CAN/blob/"
                      "promptset/motorcontrol-can/docs/prompts/CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence",
                      rendered)
        self.assertEqual(text.count("\n"), rendered.count("\n"))

    def test_external_links_anchors_and_explicit_refs_are_unchanged(self) -> None:
        text = ("[site](https://example.com/contract.md) [anchor](#local)\n"
                "See @ref MissingPublicContract.\n")
        self.assertEqual(doxygen_filter.render_markdown(text, ROOT / "README.md"), text)

    def test_link_label_matching_target_is_preserved(self) -> None:
        rendered = doxygen_filter.render_markdown("[README.md](README.md)", ROOT / "README.md")
        self.assertTrue(rendered.startswith("[README.md](https://"), rendered)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--doxygen", default="doxygen", help="Doxygen 1.13.2 executable")
    arguments = parser.parse_args()
    try:
        version = subprocess.run([arguments.doxygen, "--version"], capture_output=True,
                                 text=True, timeout=10, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        parser.exit(2, f"Documentation gate unavailable: {error}\n")
    actual = version.stdout.strip()
    if version.returncode or not doxygen_matches(actual):
        parser.exit(2, f"Documentation gate requires Doxygen {DOXYGEN_VERSION}; found {actual!r}\n")
    DocumentationGateTests.doxygen = arguments.doxygen
    suite = unittest.TestSuite([
        unittest.defaultTestLoader.loadTestsFromTestCase(DocumentationGateTests),
        unittest.defaultTestLoader.loadTestsFromTestCase(MarkdownFilterTests),
    ])
    return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
