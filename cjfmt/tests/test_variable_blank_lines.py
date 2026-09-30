# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

"""Run after building cjfmt and sourcing the SDK's envsetup.sh:

    python3 -m unittest discover -s tests

CJFMT_BINARY may override the default build/build/bin/cjfmt executable.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get("CJFMT_BINARY", ROOT / "build/build/bin/cjfmt"))


class VariableBlankLinesTest(unittest.TestCase):
    def format_source(self, source, region=None):
        with tempfile.TemporaryDirectory(prefix="cjfmt-variable-spacing-") as directory:
            source_file = Path(directory) / "input.cj"
            output_file = Path(directory) / "output.cj"
            source_file.write_text(source)
            command = [str(BINARY), "-f", str(source_file), "-o", str(output_file),
                       "-c", str(ROOT / "config/cangjie-format.toml")]
            if region:
                command.extend(["-l", region])
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotRegex(result.stdout + result.stderr, r"(?i)error|fail")
            return output_file.read_text()

    def assert_format(self, source, expected, region=None):
        actual = self.format_source(source, region)
        self.assertEqual(actual, expected)
        self.assertEqual(self.format_source(actual, region), actual)

    def test_variable_gaps(self):
        for container in (None, "class C", "struct S"):
            for blanks in (0, 1, 3):
                with self.subTest(container=container, blanks=blanks):
                    source = ("\n" * (blanks + 1)).join(
                        ["var a=0", "var b=0", "let c=1", "let d=2"]
                    ) + "\n"
                    separator = "\n" if blanks == 0 else "\n\n"
                    expected = separator.join(
                        ["var a = 0", "var b = 0", "let c = 1", "let d = 2"]
                    ) + "\n"
                    if container:
                        source = container + " {\n" + source + "}\n"
                        expected = container + " {\n" + textwrap.indent(expected, "    ") + "}\n"
                    self.assert_format(source, expected)

    def test_multiline_initializer(self):
        for blanks in (0, 1, 3):
            with self.subTest(blanks=blanks):
                source = "let a=[\n1,\n2\n]" + "\n" * (blanks + 1) + "let b=3\n"
                expected = "let a = [\n    1,\n    2\n]" + ("\n\n" if blanks else "\n") + "let b = 3\n"
                self.assert_format(source, expected)

    def test_pattern_variables(self):
        for blanks in (0, 1, 3):
            with self.subTest(blanks=blanks):
                source = ("\n" * (blanks + 1)).join(
                    ["let first=0", "let (a,b)=(1,2)", "var (c,d)=(3,4)", "var last=5"]
                ) + "\n"
                expected = ("\n\n" if blanks else "\n").join(
                    ["let first = 0", "let (a, b) = (1, 2)", "var (c, d) = (3, 4)", "var last = 5"]
                ) + "\n"
                self.assert_format(source, expected)

    def test_whitespace_only_lines(self):
        self.assert_format("var a=0\n  \n\t\nlet b=1\n", "var a = 0\n\nlet b = 1\n")

    def test_annotations_and_modifiers(self):
        source = ('let a=0\n@Deprecated["old", strict: true]\npublic\nlet b=1\n\n\n'
                  '@Deprecated["old", strict: true] let c=2\nlet d=3\n')
        expected = ('let a = 0\n@Deprecated["old", strict: true]\npublic let b = 1\n\n'
                    '@Deprecated["old", strict: true]\nlet c = 2\nlet d = 3\n')
        self.assert_format(source, expected)

    def test_comments(self):
        for comment in (" // trailing", " /* trailing */", "\n// leading", "\n/* leading */"):
            for blanks in (0, 1, 3):
                with self.subTest(comment=comment, blanks=blanks):
                    source = "var a=0" + comment + "\n" * (blanks + 1) + "let b=1\n"
                    expected = "var a = 0" + comment + ("\n\n" if blanks else "\n") + "let b = 1\n"
                    self.assert_format(source, expected)

    def test_trailing_and_leading_comments(self):
        for container in (None, "class C", "struct S"):
            for before in (0, 1, 3):
                for after in (0, 1, 3):
                    with self.subTest(container=container, before=before, after=after):
                        source = ("var a=0 // trailing" + "\n" * (before + 1)
                                  + "// leading" + "\n" * (after + 1) + "let b=1\n")
                        expected = ("var a = 0 // trailing" + ("\n\n" if before else "\n")
                                    + "// leading" + ("\n\n" if after else "\n") + "let b = 1\n")
                        if container:
                            source = container + " {\n" + source + "}\n"
                            expected = container + " {\n" + textwrap.indent(expected, "    ") + "}\n"
                        self.assert_format(source, expected)

    def test_non_variable_declarations(self):
        source = "var a=0\nfunc f() {}\nlet b=1\nclass C {}\nlet c=2\n"
        expected = "var a = 0\n\nfunc f() {}\n\nlet b = 1\n\nclass C {}\n\nlet c = 2\n"
        self.assert_format(source, expected)

    def test_multiline_comments(self):
        for prefix in ("\n", " ", " // trailing\n"):
            for blanks in (0, 1, 3):
                with self.subTest(prefix=prefix, blanks=blanks):
                    source = ("var a=0" + prefix + "/* first\n * second */"
                              + "\n" * (blanks + 1) + "let b=1\n")
                    expected = ("var a = 0" + prefix + "/* first\n * second */"
                                + ("\n\n" if blanks else "\n") + "let b = 1\n")
                    self.assert_format(source, expected)

    def test_foreign_block(self):
        for blanks in (0, 1, 3):
            with self.subTest(blanks=blanks):
                source = ("foreign {\nvar a:Int64" + "\n" * (blanks + 1)
                          + "var b:Int64\n}\n\n\nvar c=0\n")
                expected = ("foreign {\n    var a: Int64" + ("\n\n" if blanks else "\n")
                            + "    var b: Int64\n}\n\nvar c = 0\n")
                self.assert_format(source, expected)

    def test_shared_annotation_block(self):
        source = ('@When[os == "Linux"] foreign {\nvar a:Int64\n\n\nvar b:Int64\nvar c:Int64\n}\nlet d=3\n')
        expected = ('@When[os == "Linux"]\nforeign {\n    var a: Int64\n\n    var b: Int64\n'
                    '    var c: Int64\n}\n\nlet d = 3\n')
        self.assert_format(source, expected)

    def test_region(self):
        source = "var a=0\n\n\nlet b=1\nvar c=2\n"
        self.assert_format(source, "var a = 0\n\nlet b = 1\nvar c = 2\n", "1:5")

    def test_region_limits(self):
        source = "var before=9\nvar a=0\n\n\nlet b=1\nvar after=9\n"
        expected = "var before=9\nvar a = 0\n\nlet b = 1\nvar after=9\n"
        self.assertEqual(self.format_source(source, "2:5"), expected)
        # The same declarations occupy one fewer line after their blank lines are collapsed.
        self.assertEqual(self.format_source(expected, "2:4"), expected)


if __name__ == "__main__":
    unittest.main()
