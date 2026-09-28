"""Regression test suite for the BOTF combat ship asset pipeline tooling.

Tests:
1. SafeArchiveValidator: path traversal defenses, sibling-prefix escape prevention, entry sanitization.
2. MS3DParser: binary parsing integrity, static mesh verification, corrupted header rejection.
3. UnrealImportCandidates: path sanitization, status-based filtering (Ready vs Experimental), hash matching.
4. Manifest & Asset Integrity: GLB existence, SHA-256 checksums, geometry parity.
"""
import unittest
import tempfile
import zipfile
import shutil
import json
import hashlib
from pathlib import Path

# Paths
TOOLING_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = TOOLING_DIR.parents[1]
CANDIDATES_DIR = PROJECT_ROOT / "game" / "ArtSource" / "CombatCandidates"

# Import tooling under test
import sys
sys.path.insert(0, str(TOOLING_DIR))
from archive_validator import SafeArchiveValidator, is_safe_entry_name
from ms3d_parser import MS3DParser
import unreal_import_candidates


class TestArchiveValidator(unittest.TestCase):
    """Verifies archive safety, path traversal protection, and containment."""

    def test_is_safe_entry_name(self):
        self.assertTrue(is_safe_entry_name("ship.ms3d"))
        self.assertTrue(is_safe_entry_name("Completed/ranger/franger.tga"))
        self.assertTrue(is_safe_entry_name("sub/dir/file.txt"))

        # Unsafe patterns
        self.assertFalse(is_safe_entry_name("../escaped.txt"))
        self.assertFalse(is_safe_entry_name("a/../../b.txt"))
        self.assertFalse(is_safe_entry_name("/root/file.txt"))
        self.assertFalse(is_safe_entry_name("\\Windows\\system32"))
        self.assertFalse(is_safe_entry_name("C:/file.txt"))
        self.assertFalse(is_safe_entry_name("D:\\file.txt"))
        self.assertFalse(is_safe_entry_name("file\0null.txt"))
        self.assertFalse(is_safe_entry_name(""))

    def test_zip_slip_and_sibling_containment(self):
        """Verify that attempts to write outside target_dir are blocked."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            zip_file = temp_path / "test_slip.zip"
            target_dir = temp_path / "target"
            sibling_dir = temp_path / "target_sibling"

            # Create test archive with standard and malicious paths
            with zipfile.ZipFile(zip_file, "w") as zf:
                zf.writestr("valid.txt", "safe content")
                zf.writestr("../traversal.txt", "traversal content")
                zf.writestr("../target_sibling/escape.txt", "sibling prefix content")

            validator = SafeArchiveValidator(zip_file)
            inspect_res = validator.inspect()
            self.assertFalse(inspect_res["all_entries_safe"])

            # 1. Valid extraction succeeds
            extracted = validator.safe_extract_entry("valid.txt", target_dir)
            self.assertTrue(extracted.is_file())
            self.assertTrue(extracted.is_relative_to(target_dir))

            # 2. Path traversal entry raises ValueError
            with self.assertRaises(ValueError):
                validator.safe_extract_entry("../traversal.txt", target_dir)

            # 3. Entry that escapes target_dir to sibling raises ValueError
            with self.assertRaises(ValueError):
                validator.safe_extract_entry("../target_sibling/escape.txt", target_dir)


class TestMS3DParser(unittest.TestCase):
    """Verifies pure-Python MS3D binary parsing."""

    def test_parse_franger(self):
        franger_path = PROJECT_ROOT / "game" / "ArtSource" / "CombatCandidates" / "source" / "Icewolf132" / "Completed" / "ranger" / "franger.ms3d"
        if not franger_path.is_file():
            self.skipTest("franger.ms3d not found in extracted source")

        parser = MS3DParser(franger_path)
        data = parser.parse()
        self.assertEqual(data["vertex_count"], 581)
        self.assertEqual(data["triangle_count"], 677)
        self.assertTrue(data["is_static"])
        self.assertEqual(data["bound_vertex_count"], 0)
        self.assertFalse(data["has_animation"])
        self.assertGreater(data["bounds"]["bounding_sphere_radius"], 0)

    def test_invalid_header_rejection(self):
        with tempfile.NamedTemporaryFile(suffix=".ms3d", delete=False) as tf:
            tf.write(b"CORRUPTED_HEADER_DATA_12345")
            tf_path = Path(tf.name)

        try:
            parser = MS3DParser(tf_path)
            with self.assertRaises(ValueError):
                parser.parse()
        finally:
            tf_path.unlink()


class TestUnrealImporterTooling(unittest.TestCase):
    """Verifies Unreal import helper path sanitization, hashing, and status filtering."""

    def test_sanitize_path_segment(self):
        self.assertEqual(unreal_import_candidates.sanitize_path_segment("Federation"), "Federation")
        self.assertEqual(unreal_import_candidates.sanitize_path_segment("Lyran / Independent"), "Lyran_Independent")
        self.assertEqual(unreal_import_candidates.sanitize_path_segment("Klingon-Empire!"), "Klingon_Empire")
        self.assertEqual(unreal_import_candidates.sanitize_path_segment(""), "Common")

    def test_dry_run_filtering_and_hash_verification(self):
        """Verifies that dry-run successfully validates the manifest and filters out experimental studies."""
        # Default run: only Ready candidates
        default_res = unreal_import_candidates.run_dry_run(include_experimental=False)
        self.assertTrue(default_res["valid"])
        self.assertEqual(default_res["error_count"], 0)
        self.assertEqual(default_res["planned_count"], 7)
        self.assertEqual(default_res["skipped_count"], 3)

        planned_names = {p["name"] for p in default_res["planned"]}
        self.assertIn("franger", planned_names)
        self.assertIn("freaver1", planned_names)
        self.assertIn("freaver2", planned_names)
        self.assertIn("freaver3", planned_names)
        self.assertIn("fUlysses", planned_names)
        self.assertIn("klingon_cruiser", planned_names)
        self.assertIn("lcruise1", planned_names)

        # Experimental candidates must be excluded from default
        self.assertNotIn("kqultoq", planned_names)
        self.assertNotIn("RemanBoP", planned_names)
        self.assertNotIn("rhhawk_TMP", planned_names)

        # Including experimental: all 10 planned
        exp_res = unreal_import_candidates.run_dry_run(include_experimental=True)
        self.assertTrue(exp_res["valid"])
        self.assertEqual(exp_res["planned_count"], 10)
        self.assertEqual(exp_res["skipped_count"], 0)


class TestManifestIntegrity(unittest.TestCase):
    """Verifies that all candidates in manifest.json have matching GLB files on disk and valid metadata."""

    def test_manifest_file_and_checksum_consistency(self):
        manifest_path = CANDIDATES_DIR / "manifest.json"
        self.assertTrue(manifest_path.is_file())
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        candidates = manifest.get("initial_candidates", [])
        self.assertEqual(len(candidates), 10)

        converted_dir = CANDIDATES_DIR / "converted"
        for cand in candidates:
            glb_path = converted_dir / cand["output_glb"]
            meta_path = converted_dir / f"{cand['candidate_name']}.meta.json"

            # 1. Output files exist
            self.assertTrue(glb_path.is_file(), f"Missing GLB file: {glb_path}")
            self.assertTrue(meta_path.is_file(), f"Missing meta file: {meta_path}")

            # 2. SHA-256 matches actual file on disk
            actual_glb_hash = hashlib.sha256(glb_path.read_bytes()).hexdigest()
            self.assertEqual(actual_glb_hash, cand["output_glb_sha256"])

            # 3. Metadata geometry matches manifest
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            self.assertEqual(meta["triangles"], cand["triangles"])
            self.assertEqual(meta["vertices"], cand["vertices"])
            self.assertAlmostEqual(
                meta["bounds"]["bounding_sphere_radius"],
                cand["bounds"]["bounding_sphere_radius"],
                places=3
            )

            # 4. Status is strictly classified
            self.assertIn(cand["status"], ["Ready", "Experimental"])
            self.assertEqual(cand["coordinate_transform"]["pivot"], "bounding_box_center")


if __name__ == "__main__":
    unittest.main()
