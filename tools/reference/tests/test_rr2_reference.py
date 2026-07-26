import json
import os
import sys
import tempfile
import unittest
from pathlib import Path


TOOL_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_DIRECTORY))

import rr2_reference as reference  # noqa: E402


class ManifestTests(unittest.TestCase):
    def test_manifest_is_stable_and_contains_no_absolute_root(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory) / "fixture"
            root.mkdir()
            (root / "A.txt").write_bytes(b"alpha \r\n")
            (root / "binary.bin").write_bytes(b"\x00\x01\x02")

            first = reference.build_manifest(
                root, "fixture", frozenset({".txt"}), exclude_patterns=[]
            )
            second = reference.build_manifest(
                root, "fixture", frozenset({".txt"}), exclude_patterns=[]
            )

            self.assertEqual(first, second)
            self.assertEqual(first["file_count"], 2)
            self.assertEqual(first["files"][0]["path"], "A.txt")
            self.assertIn("normalized_text_sha256", first["files"][0])
            self.assertNotIn(str(root), json.dumps(first))

    def test_manifest_diff_classifies_every_change_type(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            base = Path(temporary_directory)
            left_root = base / "left"
            right_root = base / "right"
            left_root.mkdir()
            right_root.mkdir()

            (left_root / "exact.txt").write_bytes(b"exact\n")
            (right_root / "exact.txt").write_bytes(b"exact\n")
            (left_root / "normalized.txt").write_bytes(b"alpha \r\n")
            (right_root / "normalized.txt").write_bytes(b"alpha\n")
            (left_root / "changed.cfg").write_bytes(b"one\n")
            (right_root / "changed.cfg").write_bytes(b"two\n")
            (left_root / "left.bin").write_bytes(b"left")
            (right_root / "right.bin").write_bytes(b"right")

            text_extensions = frozenset({".cfg", ".txt"})
            left = reference.build_manifest(left_root, "left", text_extensions, [])
            right = reference.build_manifest(right_root, "right", text_extensions, [])
            report = reference.build_manifest_diff(left, right)

            self.assertEqual(
                report["summary"],
                {
                    "case_changed": 0,
                    "changed": 1,
                    "common": 3,
                    "exact": 1,
                    "left_only": 1,
                    "normalized_equal": 1,
                    "right_only": 1,
                },
            )

    def test_manifest_rejects_output_inside_inventoried_root(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            with self.assertRaises(reference.ReferenceError):
                reference.ensure_output_outside_root(root, root / "manifest.json")

    def test_manifest_records_timestamp_changes_even_when_bytes_match(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            file_path = root / "same.bin"
            file_path.write_bytes(b"same bytes")
            before = reference.build_manifest(root, "fixture", frozenset(), [])

            next_timestamp = file_path.stat().st_mtime + 5
            os.utime(file_path, (next_timestamp, next_timestamp))
            after = reference.build_manifest(root, "fixture", frozenset(), [])

            self.assertNotEqual(before, after)
            diff = reference.build_manifest_diff(before, after)
            self.assertEqual(diff["summary"]["exact"], 1)
            self.assertNotEqual(before["files"][0]["mtime_ns"], after["files"][0]["mtime_ns"])

    def test_parity_ledger_ids_are_stable_and_exact_files_are_omitted(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            base = Path(temporary_directory)
            left_root = base / "left"
            right_root = base / "right"
            left_root.mkdir()
            right_root.mkdir()
            (left_root / "exact.txt").write_bytes(b"exact")
            (right_root / "exact.txt").write_bytes(b"exact")
            (left_root / "changed.txt").write_bytes(b"left")
            (right_root / "changed.txt").write_bytes(b"right")

            left = reference.build_manifest(left_root, "left", frozenset({".txt"}), [])
            right = reference.build_manifest(right_root, "right", frozenset({".txt"}), [])
            diff = reference.build_manifest_diff(left, right)
            first = reference.build_parity_ledger(diff)
            second = reference.build_parity_ledger(diff)

            self.assertEqual(first, second)
            self.assertEqual(first["entry_count"], 1)
            self.assertEqual(first["entries"][0]["path"], "changed.txt")
            self.assertRegex(first["entries"][0]["id"], r"^RP-[0-9A-F]{12}$")
            self.assertEqual(first["entries"][0]["classification"], "UNKNOWN")


class PeTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "test requires a Windows PE executable")
    def test_inspects_running_python_executable(self):
        report = reference.inspect_pe(Path(sys.executable), [])

        self.assertEqual(report["schema"], reference.PE_SCHEMA)
        self.assertIn(report["coff"]["machine_name"], {"I386", "AMD64", "ARM64"})
        self.assertGreater(report["coff"]["section_count"], 0)
        self.assertTrue(report["sections"])
        self.assertTrue(report["imports"])


if __name__ == "__main__":
    unittest.main()
