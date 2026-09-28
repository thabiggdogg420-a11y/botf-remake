"""Binary fixtures independently specify layout boundaries and corruption cases."""
import importlib.util
from pathlib import Path
import struct
import unittest

MODULE = Path(__file__).resolve().parents[2] / "research" / "export-reference.py"
SPEC = importlib.util.spec_from_file_location("reference_export", MODULE)
reference = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(reference)


class ReferenceTests(unittest.TestCase):
    def test_descriptions_and_unresolved_special_requirements(self):
        rows = [{"description_address": 0}, {"description_address": 2}, {"description_address": -1}]
        reference.attach_descriptions(rows, b"One\0Two\0")
        self.assertEqual([r["description"] for r in rows], ["One", "e", None])
        with self.assertRaises(ValueError):
            reference.attach_descriptions([{"description_address": 8}], b"One\0Two\0")
        with self.assertRaises(ValueError):
            reference.attach_descriptions([{"description_address": 0}], b"Unterminated")
        ships = [{"index": 120, "parent": -1, "technology_levels": [11]}]
        warnings = reference.validate_relationships([{"field": 0, "level": 10}], ships, [])
        self.assertEqual(warnings, [{"code": "unresolved_ship_technology", "ship": 120, "field": 0, "level": 11}])
        ships[0]["parent"] = 120
        with self.assertRaises(ValueError):
            reference.validate_relationships([{"field": 0, "level": 10}], ships, [])

    def test_building_address_map_and_upgrade_relationship(self):
        data = bytearray(188)
        struct.pack_into("<ii", data, 0, 1, 184)
        data[8:13] = b"Farm\0"
        struct.pack_into("<hh", data, 88, 0, 32767)
        struct.pack_into("<H", data, 128, 65000)
        struct.pack_into("<h", data, 132, -5)
        struct.pack_into("<i", data, 184, 8)
        row = reference.parse_buildings(data)["records"][0]
        self.assertEqual((row["name"], row["build_cost"], row["energy_cost"]), ("Farm", 65000, -5))
        with self.assertRaises(ValueError):
            reference.parse_buildings(data[:-1])
        struct.pack_into("<h", data, 90, 7)
        with self.assertRaises(ValueError):
            reference.parse_buildings(data)
        struct.pack_into("<h", data, 90, 32767)
        struct.pack_into("<i", data, 184, 9)
        with self.assertRaises(ValueError):
            reference.parse_buildings(data)

    def test_technology_offsets_and_unknown_bytes(self):
        raw = bytearray(72)
        raw[:5] = b"Bion\0"
        struct.pack_into("<H", raw, 40, 513)
        raw[42:48] = b"image\0"
        raw[55:60] = bytes([2, 7, 9, 8, 7])
        struct.pack_into("<qi", raw, 60, 123456789012, -1)
        row = reference.parse_technologies(raw)[0]
        self.assertEqual((row["index"], row["field"], row["level"], row["cost"]), (513, 2, 7, 123456789012))
        self.assertEqual(row["unknown_57_59"], "090807")
        self.assertEqual(row["description_address"], -1)
        for damaged in (raw[:-1], raw + raw):
            with self.assertRaises(ValueError):
                reference.parse_technologies(damaged)

    def test_ship_mapping_overrides_raw_race_and_preserves_numbers(self):
        raw = bytearray(288)
        raw[:5] = b"Test\0"
        struct.pack_into("<h", raw, 102, 4)
        struct.pack_into("<i", raw, 168, 1234)
        struct.pack_into("<d", raw, 260, 2.5)
        struct.pack_into("<h", raw, 280, 3)
        struct.pack_into("<HH", raw, 284, 65000, 400)
        mapping = struct.pack("<ihh6B2x", 9, 4, -1, 1, 2, 3, 4, 5, 6)
        row = reference.parse_ships(raw, mapping)[0]
        self.assertEqual((row["raw_race"], row["race"]), (3, 9))
        self.assertEqual((row["hull_strength"], row["map_range"], row["build_cost"]), (1234, 2.5, 65000))
        self.assertEqual(row["technology_levels"], [1, 2, 3, 4, 5, 6])
        for ships, tech in ((raw[:-1], mapping), (raw, mapping[:-1]), (raw + raw, mapping), (raw, mapping + mapping)):
            with self.assertRaises(ValueError):
                reference.parse_ships(ships, tech)
        struct.pack_into("<d", raw, 260, float("nan"))
        with self.assertRaises(ValueError):
            reference.parse_ships(raw, mapping)


if __name__ == "__main__":
    unittest.main()
