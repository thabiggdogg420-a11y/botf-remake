"""Read-only BOTF reference inventory and technology export; never imports game code.

PowerShell: python research/export-reference.py --source <extracted mod directory>
Outputs default to the ignored research/reference-local/normalized directory.
Binary layout: Ultimate Editor, pinned in FORMAT_SOURCE below. Unknown bytes and
description pointers are retained without assigning unsupported semantics.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import math
import zipfile

FORMAT_SOURCE = "https://gitlab.com/stbotf/ultimate-editor/-/blob/7ee45c4b89942c324aa8ff0b9cb4334defbc667e/source/ue/edit/res/stbof/files/tec/data/TechTreeEntry.java"
SHIP_FORMAT_SOURCE = FORMAT_SOURCE.replace("tec/data/TechTreeEntry.java", "sst/data/ShipDefinition.java")
SHIP_TECH_SOURCE = FORMAT_SOURCE.replace("tec/data/TechTreeEntry.java", "sst/ShipTech.java")
BUILDING_SOURCE = FORMAT_SOURCE.replace("tec/data/TechTreeEntry.java", "bst/data/Building.java")
RACE_RECORD_SIZE = 108


def c_string(value):
    """Return a C++ string literal with deterministic escaping."""
    return json.dumps(value, ensure_ascii=True)


def parse_technologies(data):
    if not data or len(data) % 72:
        raise ValueError("Technology table must contain complete 72-byte records")
    records, keys, indices = [], set(), set()
    for offset in range(0, len(data), 72):
        raw = data[offset:offset + 72]
        name = raw[:40].split(b"\0", 1)[0].decode("cp1252")
        index = struct.unpack_from("<H", raw, 40)[0]
        field, level = raw[55:57]
        cost, description_address = struct.unpack_from("<qi", raw, 60)
        if not name or cost < 0 or (field, level) in keys or index in indices:
            raise ValueError(f"Invalid or duplicate technology at byte {offset}")
        keys.add((field, level))
        indices.add(index)
        records.append(dict(source_offset=offset, index=index, name=name,
                            image=raw[42:55].split(b"\0", 1)[0].decode("cp1252"),
                            field=field, level=level, cost=cost,
                            unknown_57_59=raw[57:60].hex(),
                            description_address=description_address))
    return records


def parse_ships(data, technology_data):
    if not data or len(data) % 288 or not technology_data or len(technology_data) % 16:
        raise ValueError("Incomplete ship or ship technology record")
    requirements = {}
    for offset in range(0, len(technology_data), 16):
        raw = technology_data[offset:offset + 16]
        race, index, parent = struct.unpack_from("<ihh", raw)
        if index < 0 or index in requirements or raw[14:] != b"\0\0":
            raise ValueError(f"Invalid ship technology record at {offset}")
        requirements[index] = dict(race=race, parent=parent, technology_levels=list(raw[8:14]))
    records, indices = [], set()
    integer_fields = "beam_accuracy beam_number beam_multiplier beam_damage beam_penetration torpedo_accuracy torpedo_number torpedo_multiplier torpedo_damage shield_level shield_strength shield_recharge offensive_strength defensive_strength unknown_164 hull_strength defense stealth super_ray_number super_ray_accuracy super_ray_multiplier super_ray_damage super_ray_penetration unknown_200".split()
    double_fields = "agility firing_arc max_beam_range min_torpedo_range max_torpedo_range max_super_ray_range map_range map_speed".split()
    for offset in range(0, len(data), 288):
        raw = data[offset:offset + 288]
        index, role, name_group = struct.unpack_from("<hhh", raw, 102)
        if index in indices or index not in requirements:
            raise ValueError(f"Duplicate or unmapped ship at {offset}")
        indices.add(index)
        doubles = struct.unpack_from("<8d", raw, 212)
        if not all(math.isfinite(value) for value in doubles):
            raise ValueError(f"Non-finite ship value at {offset}")
        record = dict(source_offset=offset, index=index,
                      name=raw[:40].split(b"\0", 1)[0].decode("cp1252"),
                      ship_class=raw[40:84].split(b"\0", 1)[0].decode("cp1252"),
                      description_address=struct.unpack_from("<i", raw, 84)[0],
                      graphics=raw[88:102].split(b"\0", 1)[0].decode("cp1252"),
                      role=role, name_group=name_group,
                      unknown_204=raw[204:212].hex(),
                      scan_range=struct.unpack_from("<H", raw, 276)[0],
                      production=struct.unpack_from("<H", raw, 278)[0],
                      raw_race=struct.unpack_from("<h", raw, 280)[0],
                      weapon_prefix_bytes=raw[282:284].hex(),
                      build_cost=struct.unpack_from("<H", raw, 284)[0],
                      maintenance=struct.unpack_from("<H", raw, 286)[0],
                      **requirements[index])
        record.update(zip(integer_fields, struct.unpack_from("<24i", raw, 108)))
        record.update(zip(double_fields, doubles))
        records.append(record)
    if indices != set(requirements):
        raise ValueError("Ship and technology tables have different IDs")
    return records


def parse_buildings(data):
    if len(data) < 8:
        raise ValueError("Missing building header")
    count, address_map = struct.unpack_from("<ii", data)
    start = address_map - count * 176
    if count <= 0 or count > 10000 or start < 8 or address_map + count * 4 != len(data):
        raise ValueError("Invalid building count or address map boundary")
    addresses = struct.unpack_from(f"<{count}i", data, address_map)
    records, indices = [], set()
    for offset in range(start, address_map, 176):
        raw = data[offset:offset + 176]
        index, upgrade_for = struct.unpack_from("<hh", raw, 80)
        if index < 0 or index >= count or index in indices or addresses[index] != offset:
            raise ValueError(f"Invalid building ID/address at {offset}")
        indices.add(index)
        records.append(dict(source_offset=offset, index=index,
                            name=raw[:40].split(b"\0", 1)[0].decode("cp1252"),
                            plural_name=raw[40:80].split(b"\0", 1)[0].decode("cp1252"),
                            upgrade_for=upgrade_for, no_upgrade_sentinel=32767,
                            image_prefix=raw[88:96].split(b"\0", 1)[0].decode("cp1252"),
                            technology_field=raw[101], technology_level=raw[102],
                            excluded_empire_mask=raw[103], resident_race_mask=struct.unpack_from("<Q", raw, 104)[0],
                            political_requirement=raw[112], system_requirement=raw[113],
                            bonus_type=raw[114], build_limit=raw[115], group=raw[116], category=raw[117],
                            build_cost=struct.unpack_from("<H", raw, 120)[0],
                            energy_cost=struct.unpack_from("<h", raw, 124)[0],
                            population_cost=struct.unpack_from("<h", raw, 128)[0],
                            morale_bonus=struct.unpack_from("<h", raw, 130)[0],
                            bonus_values=list(struct.unpack_from("<6h", raw, 134)),
                            unrest_bonuses=list(struct.unpack_from("<6h", raw, 146)),
                            description_address=struct.unpack_from("<i", raw, 172)[0],
                            raw_record_hex=raw.hex()))
    for row in records:
        if row["upgrade_for"] != 32767 and row["upgrade_for"] not in indices:
            raise ValueError(f"Unknown building upgrade target {row['upgrade_for']}")
    return dict(records=records, prerequisite_header_hex=data[8:start].hex(), address_map=list(addresses))


def parse_races(data):
    """Parse the fixed-size name/icon race catalogue, retaining unknown bytes."""
    if len(data) < 2:
        raise ValueError("Missing race count")
    count = struct.unpack_from("<H", data)[0]
    if count <= 0 or count > 256 or len(data) != 2 + count * RACE_RECORD_SIZE:
        raise ValueError("Invalid race table count or record boundary")
    records = []
    for index in range(count):
        offset = 2 + index * RACE_RECORD_SIZE
        raw = data[offset:offset + RACE_RECORD_SIZE]
        name = raw[:40].split(b"\0", 1)[0].decode("cp1252")
        icon = raw[40:80].split(b"\0", 1)[0].decode("cp1252")
        if not name or not icon:
            raise ValueError(f"Race {index} is missing its name or icon")
        records.append(dict(index=index, name=name, icon=icon,
                            unknown_record_hex=raw[80:].hex()))
    return records


def parse_minor_config(data):
    """Expose named minor-AI config entries without inventing value semantics."""
    if len(data) < 12:
        raise ValueError("Minor AI config is truncated")
    version, count, stride, payload_size = struct.unpack_from("<IHHI", data)
    if version == 0 or count == 0 or stride == 0 or payload_size != count * stride:
        raise ValueError("Invalid minor AI config header")
    if len(data) < 12 + payload_size:
        raise ValueError("Minor AI config payload is truncated")
    payload = data[12:12 + payload_size]
    tail = data[12 + payload_size:]
    # The table contains strings at unaligned offsets; stride describes a
    # section of the format, not individual rows. Preserve discovered labels
    # as labels only until the complete structure reader is verified.
    strings = []
    for raw in data.split(b"\0"):
        if raw and all(32 <= byte < 127 for byte in raw):
            value = raw.decode("cp1252")
            if value not in strings:
                strings.append(value)
    return dict(status="string_inventory_only_structure_unverified", version=version,
                header_count=count, header_stride=stride, header_payload_size=payload_size,
                strings=strings, trailing_bytes=len(tail),
                trailing_sha256=hashlib.sha256(tail).hexdigest())


def planet_resource_metadata(data):
    """Keep the planet table as a fingerprinted opaque resource until its reader is verified."""
    return dict(status="opaque_resource_format_not_yet_verified", bytes=len(data),
                sha256=hashlib.sha256(data).hexdigest())


def attach_descriptions(records, data):
    for row in records:
        address = row["description_address"]
        if address == -1:
            row["description"] = None
            continue
        if address < 0 or address >= len(data):
            raise ValueError(f"Description pointer out of bounds: {address}")
        end = data.find(b"\0", address)
        if end < 0:
            raise ValueError(f"Unterminated description at {address}")
        row["description"] = data[address:end].decode("cp1252")


def validate_relationships(technologies, ships, buildings, races=None):
    warnings = []
    technology_keys = {(row["field"], row["level"]) for row in technologies}
    for row in buildings:
        if (row["technology_field"], row["technology_level"]) not in technology_keys:
            raise ValueError(f"Building {row['index']} has an unknown technology requirement")
    for row in ships:
        if races is not None and not 0 <= row["race"] < len(races):
            warnings.append(dict(code="unresolved_ship_race", ship=row["index"], race=row["race"]))
        for field, level in enumerate(row["technology_levels"]):
            if (field, level) not in technology_keys:
                warnings.append(dict(code="unresolved_ship_technology", ship=row["index"], field=field, level=level))
    for rows, parent_key, sentinel in ((ships, "parent", -1), (buildings, "upgrade_for", 32767)):
        parents = {row["index"]: row[parent_key] for row in rows}
        for start in parents:
            seen, current = set(), start
            while current != sentinel:
                if current in seen or current not in parents:
                    raise ValueError(f"Cyclic or missing {parent_key} relationship from {start}")
                seen.add(current)
                current = parents[current]
    return warnings


def emit_catalogue_header(target, technologies, ships, buildings, races, provenance):
    """Emit stable, read-only C++ reference views for the gameplay core."""
    lines = ["// Generated by research/export-reference.py from local AIO Vanilla reference data.",
             "// Provenance SHA256 manifest: " + provenance,
             "// Reference values only; unknown/unsupported mechanics are not inferred.",
             "#pragma once", "#include <array>", "#include <cstdint>",
             "namespace botf::referenceData {",
             "struct TechnologyRecord { int id; const char* name; int field; int level; std::int64_t cost; const char* description; };",
             "struct ShipRecord { int id; int raceId; const char* name; const char* shipClass; std::array<int, 6> technologyRequirements; int industryCost; int maintenance; int hull; int shields; int attack; double range; double speed; int role; int troopCapacity; const char* description; };",
             "struct BuildingRecord { int id; const char* name; int technologyField; int technologyLevel; int industryCost; int energyCost; int populationCost; int moraleBonus; int excludedEmpireMask; std::uint64_t residentRaceMask; int bonusType; std::array<int, 6> bonusValues; std::array<int, 6> unrestBonuses; const char* description; };",
             "struct RaceRecord { int id; const char* name; const char* icon; };",
             f"inline constexpr std::array<TechnologyRecord, {len(technologies)}> TechnologyCatalogue{{{{"]
    for row in sorted(technologies, key=lambda item: item["index"]):
        desc = row.get("description") or ""
        lines.append(f"    {{{row['index']}, {c_string(row['name'])}, {row['field']}, {row['level']}, {row['cost']}, {c_string(desc)}}},")
    lines.extend(["}};", f"inline constexpr std::array<ShipRecord, {len(ships)}> ShipCatalogue{{{{"])
    for row in sorted(ships, key=lambda item: item["index"]):
        lines.append("    {%d, %d, %s, %s, {%s}, %d, %d, %d, %d, %d, %.17g, %.17g, %d, -1, %s}," % (
            row["index"], row["race"], c_string(row["name"]), c_string(row["ship_class"]),
            ", ".join(map(str, row["technology_levels"])), row["build_cost"], row["maintenance"],
            row["hull_strength"], row["shield_strength"], row["offensive_strength"],
            row["map_range"], row["map_speed"], row["role"], c_string(row.get("description") or "")))
    lines.extend(["}};", f"inline constexpr std::array<BuildingRecord, {len(buildings)}> BuildingCatalogue{{{{"])
    for row in sorted(buildings, key=lambda item: item["index"]):
        lines.append("    {%d, %s, %d, %d, %d, %d, %d, %d, %d, %dULL, %d, {%s}, {%s}, %s}," % (
            row["index"], c_string(row["name"]), row["technology_field"], row["technology_level"],
            row["build_cost"], row["energy_cost"], row["population_cost"], row["morale_bonus"],
            row["excluded_empire_mask"], row["resident_race_mask"], row["bonus_type"],
            ", ".join(map(str, row["bonus_values"])), ", ".join(map(str, row["unrest_bonuses"])),
            c_string(row.get("description") or "")))
    lines.extend(["}};", f"inline constexpr std::array<RaceRecord, {len(races)}> RaceCatalogue{{{{"])
    for row in races:
        lines.append(f"    {{{row['index']}, {c_string(row['name'])}, {c_string(row['icon'])}}},")
    lines.extend(["}};", "} // namespace botf::referenceData", ""])
    with target.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(lines))


def emit_technology_header(source, target):
    data = (source / "techtree.tec").read_bytes()
    records = parse_technologies(data)
    indexed = {(row["field"], row["level"]): row for row in records}
    lines = ["// Generated by research/export-reference.py from Simon's local AIO Vanilla overlay.",
             "// Source SHA256: " + hashlib.sha256(data).hexdigest(),
             "// Names and costs are reference data; runtime formula choices are documented separately.",
             "#pragma once", "#include <array>", "namespace botf::referenceData {",
             "struct Technology { const char* name; int cost; };",
             "inline constexpr std::array<std::array<Technology, 11>, 6> Technologies{{"]
    for field in range(6):
        lines.append("    {{")
        for level in range(11):
            row = indexed[(field, level)]
            if not 0 <= row["cost"] <= 2147483647:
                raise ValueError("Technology cost exceeds supported integer range")
            lines.append("        {" + json.dumps(row["name"], ensure_ascii=True) + ", " + str(row["cost"]) + "},")
        lines.append("    }},")
    lines.extend(["}};", "} // namespace botf::referenceData", ""])
    with target.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(lines))
    print(f"Generated six-field technology table: {target}")


def export(source, output, base_resource=None, catalogue_header=None):
    source, output = source.resolve(), output.resolve()
    records = parse_technologies((source / "techtree.tec").read_bytes())
    buildings = parse_buildings((source / "edifice.bst").read_bytes())
    races = parse_races((source / "race.rst").read_bytes())
    minors = parse_minor_config((source / "aiminor.wtf").read_bytes())
    planet_data = (source / "planet.pst").read_bytes()
    attach_descriptions(records, (source / "techdesc.tec").read_bytes())
    attach_descriptions(buildings["records"], (source / "edifdesc.bst").read_bytes())
    manifest = []
    for path in sorted(source.iterdir()):
        if path.is_file() and path.suffix.lower() in {".tec", ".bst", ".sst", ".rst", ".pst", ".wtf"}:
            data = path.read_bytes()
            manifest.append(dict(file=path.name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    ships = []
    if base_resource:
        with zipfile.ZipFile(base_resource) as archive:
            technology_data = archive.read("shiptech.sst")
        manifest.append(dict(file=str(base_resource.resolve()) + "!shiptech.sst",
                             bytes=len(technology_data), sha256=hashlib.sha256(technology_data).hexdigest()))
        ships = parse_ships((source / "shiplist.sst").read_bytes(), technology_data)
        attach_descriptions(ships, (source / "shipdesc.sst").read_bytes())
    warnings = validate_relationships(records, ships, buildings["records"], races)
    document = dict(source_directory=str(source), format_source=FORMAT_SOURCE,
                    ship_format_source=SHIP_FORMAT_SOURCE, ship_technology_source=SHIP_TECH_SOURCE,
                    building_format_source=BUILDING_SOURCE,
                    status="Extracted All-in-One Vanilla overlay; not proven identical to retail 1.0.2",
                    encoding="cp1252 (English source)", files=manifest,
                    technology_records=records, ship_records=ships, buildings=buildings,
                    races=races, minor_ai_configuration=minors,
                    planet_resource=planet_resource_metadata(planet_data),
                    unresolved_references=warnings)
    # Exclusive creation avoids silently replacing prior reference evidence.
    output.mkdir(parents=True, exist_ok=True)
    target = output / "reference.json"
    with target.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    groups = sorted({r["field"] for r in records})
    print(f"Exported {len(records)} technology records in fields {groups}, {len(ships)} ships, {len(buildings['records'])} buildings; {len(manifest)} hashed tables: {target}")
    if catalogue_header:
        # Provenance is a compact digest over the sorted source manifest.
        provenance = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode("utf-8")).hexdigest()
        emit_catalogue_header(catalogue_header.resolve(), records, ships, buildings["records"], races, provenance)
        print(f"Generated C++ read-only catalogue: {catalogue_header.resolve()}")
    for warning in warnings:
        print("REFERENCE WARNING: " + json.dumps(warning, sort_keys=True))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--base-resource", type=Path, help="stbof.res containing the shared shiptech.sst mapping")
    parser.add_argument("--technology-header", type=Path, help="Generate a new C++ names/costs header instead of the JSON export")
    parser.add_argument("--catalogue-header", type=Path, help="Generate C++ read-only technology/ship/building/race catalogue")
    parser.add_argument("--output", type=Path, default=Path(__file__).parent / "reference-local" / "normalized")
    args = parser.parse_args()
    if args.technology_header:
        emit_technology_header(args.source, args.technology_header)
    else:
        export(args.source, args.output, args.base_resource, args.catalogue_header)
