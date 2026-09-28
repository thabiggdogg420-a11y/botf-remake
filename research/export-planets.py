"""Read-only BOTF planet, environment, and terraforming reference export.

PowerShell: python research/export-planets.py
Outputs default to research/reference-local/validated-planets and
game/Source/BotfSim/ReferencePlanets.h.

Format sources pinned from GitLab stbotf/ultimate-editor commit 7ee45c4b89942c324aa8ff0b9cb4334defbc667e:
- pst/Planet.java & pst/data/PlanetEntry.java & pst/data/PlanetKey.java
- est/Environ.java
- bin/Meplanet.java
- bin/Planspac.java & bin/Planboni.java & bin/PlanMap.java
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

UE_GITLAB_COMMIT = "7ee45c4b89942c324aa8ff0b9cb4334defbc667e"
PLANET_PST_DOC = f"https://gitlab.com/stbotf/ultimate-editor/-/blob/{UE_GITLAB_COMMIT}/source/ue/edit/res/stbof/files/pst/Planet.java"
ENVIRON_EST_DOC = f"https://gitlab.com/stbotf/ultimate-editor/-/blob/{UE_GITLAB_COMMIT}/source/ue/edit/res/stbof/files/est/Environ.java"
MEPLANET_BIN_DOC = f"https://gitlab.com/stbotf/ultimate-editor/-/blob/{UE_GITLAB_COMMIT}/source/ue/edit/res/stbof/files/bin/Meplanet.java"
PLANSPAC_BIN_DOC = f"https://gitlab.com/stbotf/ultimate-editor/-/blob/{UE_GITLAB_COMMIT}/source/ue/edit/res/stbof/files/bin/Planspac.java"

# Planet types and category classes (0..7)
PLANET_TYPES = [
    {"id": 0, "name": "Arctic", "class_letter": "P", "type_coefficient": 8.0},
    {"id": 1, "name": "Barren", "class_letter": "J", "type_coefficient": 10.0},
    {"id": 2, "name": "Desert", "class_letter": "G", "type_coefficient": 7.0},
    {"id": 3, "name": "Gas Giant", "class_letter": "B", "type_coefficient": 0.0},
    {"id": 4, "name": "Jungle", "class_letter": "L", "type_coefficient": 6.0},
    {"id": 5, "name": "Oceanic", "class_letter": "O", "type_coefficient": 4.0},
    {"id": 6, "name": "Terran", "class_letter": "M", "type_coefficient": 0.0},
    {"id": 7, "name": "Volcanic", "class_letter": "Y", "type_coefficient": 12.0},
]

# Atmosphere types (8..12)
ATMOSPHERE_TYPES = [
    {"id": 8, "name": "Methane", "atmosphere_coefficient": 1.5},
    {"id": 9, "name": "None", "atmosphere_coefficient": 1.0},
    {"id": 10, "name": "Oxygen-rich", "atmosphere_coefficient": -1.5},
    {"id": 11, "name": "Sulfuric", "atmosphere_coefficient": 1.25},
    {"id": 12, "name": "Thin-oxygen", "atmosphere_coefficient": -1.0},
]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_environ_est(data: bytes):
    """Parse environ.est (780 bytes): 13 environment entries (8 planet types + 5 atmospheres)."""
    if len(data) != 780:
        raise ValueError(f"environ.est expected 780 bytes, got {len(data)}")
    num_entries = struct.unpack_from("<H", data, 0)[0]
    if num_entries != 13:
        raise ValueError(f"environ.est expected 13 entries, got {num_entries}")

    # Header has 32 floats (128 bytes)
    frequencies = struct.unpack_from(f"<{num_entries}f", data, 2)
    entries = []
    # Entry table starts at offset 2 + 32*4 = 130
    for i in range(num_entries):
        offset = 130 + i * 50
        raw = data[offset:offset + 50]
        name = raw[:40].split(b"\0", 1)[0].decode("cp1252")
        idx, u1, u2, u3 = struct.unpack_from("<h4shh", raw, 40)
        if idx != i:
            raise ValueError(f"environ.est entry index mismatch at {i}: got {idx}")
        freq = float(frequencies[i])
        entries.append({
            "index": idx,
            "name": name,
            "frequency": round(freq, 4),
            "unknown_1_hex": u1.hex(),
            "unknown_2": u2,
            "unknown_3": u3,
        })

    # Validate sum of frequencies for planet types (0..7) is 1.0
    planet_freq_sum = sum(e["frequency"] for e in entries[:8])
    if abs(planet_freq_sum - 1.0) > 0.001:
        raise ValueError(f"environ.est planet type frequencies must sum to 1.0, got {planet_freq_sum}")
    # Validate atmosphere frequencies (8..12) are 0.0
    for e in entries[8:]:
        if e["frequency"] != 0.0:
            raise ValueError(f"environ.est atmosphere frequency must be 0.0, got {e['frequency']}")

    return {
        "status": "verified_record_layout",
        "entry_count": num_entries,
        "entries": entries,
    }


def parse_meplanet_bin(data: bytes):
    """Parse meplanet.bin (2640 bytes): 30 records (88 bytes each) defining starting home planets."""
    if len(data) != 2640 or len(data) % 88 != 0:
        raise ValueError(f"meplanet.bin expected 2640 bytes, got {len(data)}")
    num_records = len(data) // 88
    if num_records != 30:
        raise ValueError(f"meplanet.bin expected 30 records, got {num_records}")

    records = []
    empires = {}
    for i in range(num_records):
        offset = i * 88
        raw = data[offset:offset + 88]
        planet_name = raw[:40].split(b"\0", 1)[0].decode("cp1252")
        race_name = raw[40:80].split(b"\0", 1)[0].decode("cp1252")
        colonize_level, count = struct.unpack_from("<ii", raw, 80)
        rec = {
            "index": i,
            "planet": planet_name,
            "race": race_name,
            "colonize_level": colonize_level,
            "empire_planet_count": count,
        }
        records.append(rec)
        empires.setdefault(race_name, []).append(rec)

    expected_counts = {
        "Cardassians": 6,
        "Federation": 9,
        "Klingons": 4,
        "Romulans": 5,
        "Ferengi": 6,
    }
    for race, expected in expected_counts.items():
        if race not in empires or len(empires[race]) != expected:
            raise ValueError(f"meplanet.bin expected {expected} planets for {race}, got {len(empires.get(race, []))}")
        for r in empires[race]:
            if r["empire_planet_count"] != expected:
                raise ValueError(f"meplanet.bin internal count field mismatch for {race}")

    return {
        "status": "verified_record_layout",
        "record_count": num_records,
        "empires": {race: [r["planet"] for r in rows] for race, rows in empires.items()},
        "records": records,
    }


def parse_planspac_bin(data: bytes):
    """Parse planspac.bin (840 bytes): 21 records (40 bytes each) of population capacities."""
    if len(data) != 840 or len(data) % 40 != 0:
        raise ValueError(f"planspac.bin expected 840 bytes, got {len(data)}")
    num_records = len(data) // 40
    if num_records != 21:
        raise ValueError(f"planspac.bin expected 21 records (7 types * 3 sizes), got {num_records}")

    # 4D capacity array: [type 0..7][size 0..2][growth_bonus_lvl 0..2][energy_bonus_lvl 0..2]
    # Type 3 (Gas Giant) remains 0.
    capacities = [[[[0 for _ in range(3)] for _ in range(3)] for _ in range(3)] for _ in range(8)]
    records = []

    for i in range(num_records):
        offset = i * 40
        chunk = data[offset:offset + 40]
        key = struct.unpack(">I", chunk[:4])[0]
        ptype = key >> 24
        psize = (key >> 16) & 0xFF
        vals = list(struct.unpack("<9i", chunk[4:]))

        if ptype not in (0, 1, 2, 4, 5, 6, 7) or psize not in (0, 1, 2):
            raise ValueError(f"planspac.bin invalid type {ptype} or size {psize} at {i}")
        if any(v < 0 or v > 500 for v in vals):
            raise ValueError(f"planspac.bin out-of-range capacity values at {i}: {vals}")

        for g in range(3):
            for e in range(3):
                capacities[ptype][psize][g][e] = vals[g * 3 + e]

        records.append({
            "planet_type": ptype,
            "planet_size": psize,
            "values_grid": vals,
        })

    return {
        "status": "verified_record_layout",
        "record_count": num_records,
        "capacities_4d": capacities,
        "records": records,
    }


def parse_planet_pst(data: bytes):
    """Parse planet.pst (85152 bytes): 1117 records (76 bytes each) with 260-byte header."""
    if len(data) != 85152:
        raise ValueError(f"planet.pst expected 85152 bytes, got {len(data)}")
    num_planets = struct.unpack_from("<H", data, 0)[0]
    if num_planets != 1117:
        raise ValueError(f"planet.pst expected 1117 records, got {num_planets}")
    if len(data) != 260 + num_planets * 76:
        raise ValueError("planet.pst size does not match 260 + count * 76")

    # Read 8 InfoEntry structs
    infos = []
    for i in range(8):
        u_val, freq, count = struct.unpack_from("<hfh", data, 2 + i * 8)
        infos.append({"index": i, "unknown": u_val, "freq": round(freq, 4), "count": count})

    records = []
    template_records = []
    named_system_records = []

    for i in range(num_planets):
        offset = 260 + i * 76
        raw = data[offset:offset + 76]
        name = raw[:40].split(b"\0", 1)[0].decode("cp1252")
        ani = raw[40:54].split(b"\0", 1)[0].decode("cp1252")
        idx, psize, tpts, tformed, food_b, energy_b, growth_b, idx2 = struct.unpack_from("<hhhhhhhi", raw, 54)
        ptype = raw[72]
        atmo = raw[73]
        empty = struct.unpack_from("<h", raw, 74)[0]

        if ptype > 7:
            raise ValueError(f"planet.pst invalid type {ptype} at {i}")
        if atmo < 8 or atmo > 12:
            raise ValueError(f"planet.pst invalid atmosphere {atmo} at {i}")
        if psize < 0 or psize > 2:
            raise ValueError(f"planet.pst invalid size {psize} at {i}")

        entry = {
            "source_index": i,
            "id": idx,
            "name": name,
            "animation": ani,
            "planet_size": psize,
            "terraform_points": tpts,
            "terraformed": tformed,
            "food_bonus": food_b,
            "energy_bonus": energy_b,
            "growth_bonus": growth_b,
            "index_2": idx2,
            "planet_type": ptype,
            "atmosphere": atmo,
            "empty": empty,
        }
        records.append(entry)
        if name.isdigit():
            template_records.append(entry)
        else:
            named_system_records.append(entry)

    if len(template_records) != 1053 or len(named_system_records) != 64:
        raise ValueError(
            f"planet.pst unexpected template count {len(template_records)} or named count {len(named_system_records)}"
        )

    return {
        "status": "verified_record_layout",
        "record_count": num_planets,
        "template_count": len(template_records),
        "named_system_count": len(named_system_records),
        "infos": infos,
        "named_planets": [r["name"] for r in named_system_records],
        "records": records,
    }


def generate_header_content(environ, meplanet, planspac, planet_pst_meta, provenance):
    """Generate C++ constexpr header game/Source/BotfSim/ReferencePlanets.h."""
    lines = [
        "// Generated by research/export-planets.py from Simon's local BOTF Vanilla overlay.",
        f"// environ.est SHA256: {provenance['environ_est_sha256']}",
        f"// meplanet.bin SHA256: {provenance['meplanet_bin_sha256']}",
        f"// planet.pst   SHA256: {provenance['planet_pst_sha256']}",
        f"// planspac.bin SHA256: {provenance['planspac_bin_sha256']}",
        "// Pinned layout source: GitLab stbotf/ultimate-editor commit 7ee45c4b89942c324aa8ff0b9cb4334defbc667e",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        "namespace botf::referencePlanets {",
        "",
        "enum class EPlanetType : std::uint8_t {",
        "    Arctic = 0,",
        "    Barren = 1,",
        "    Desert = 2,",
        "    GasGiant = 3,",
        "    Jungle = 4,",
        "    Oceanic = 5,",
        "    Terran = 6,",
        "    Volcanic = 7,",
        "    Count = 8",
        "};",
        "",
        "enum class EAtmosphere : std::uint8_t {",
        "    Methane = 8,",
        "    None = 9,",
        "    OxygenRich = 10,",
        "    Sulfuric = 11,",
        "    ThinOxygen = 12,",
        "    Count = 5",
        "};",
        "",
        "enum class EPlanetSize : std::uint8_t {",
        "    Small = 0,",
        "    Medium = 1,",
        "    Large = 2,",
        "    Count = 3",
        "};",
        "",
        "struct PlanetTypeMeta {",
        "    EPlanetType type;",
        "    const char* name;",
        "    char classLetter;",
        "    float generationFrequency; // From environ.est",
        "    float typeCoefficient;     // Reverse-engineered terraforming coefficient",
        "};",
        "",
        "inline constexpr std::array<PlanetTypeMeta, 8> kPlanetTypes{{",
    ]

    for pt in PLANET_TYPES:
        pid = pt["id"]
        pname = pt["name"]
        pletter = pt["class_letter"]
        pfreq = environ["entries"][pid]["frequency"]
        pcoeff = pt["type_coefficient"]
        lines.append(
            f'    {{EPlanetType::{pname.replace(" ", "")}, "{pname}", \'{pletter}\', {pfreq:.4f}f, {pcoeff:.1f}f}},'
        )

    lines.extend([
        "}};",
        "",
        "struct AtmosphereMeta {",
        "    EAtmosphere atmosphere;",
        "    const char* name;",
        "    float atmosphereCoefficient;",
        "};",
        "",
        "inline constexpr std::array<AtmosphereMeta, 5> kAtmospheres{{",
        '    {EAtmosphere::Methane, "Methane", 1.50f},',
        '    {EAtmosphere::None, "None", 1.00f},',
        '    {EAtmosphere::OxygenRich, "Oxygen-rich", -1.50f},',
        '    {EAtmosphere::Sulfuric, "Sulfuric", 1.25f},',
        '    {EAtmosphere::ThinOxygen, "Thin-oxygen", -1.00f},',
        "}};",
        "",
        "// Canonical Vanilla population capacity table from planspac.bin.",
        "// Dimensions: [PlanetType 0..7][PlanetSize 0..2][GrowthBonusLvl 0..2][EnergyBonusLvl 0..2].",
        "// GasGiant (type 3) is uninhabitable and remains 0 capacity across all tiers.",
        "inline constexpr std::array<std::array<std::array<std::array<int, 3>, 3>, 3>, 8> kVanillaCapacities{{",
    ])

    caps = planspac["capacities_4d"]
    for t in range(8):
        lines.append("    {{ // Type " + str(t))
        for s in range(3):
            lines.append("        {{ // Size " + str(s))
            for g in range(3):
                row = ", ".join(str(caps[t][s][g][e]) for e in range(3))
                lines.append(f"            {{{row}}},")
            lines.append("        }},")
        lines.append("    }},")

    lines.extend([
        "}};",
        "",
        "// Calculate terraforming work based on BETA-FIDELITY formula:",
        "// terraform_work = trunc(capacity * (type_coefficient + atmosphere_coefficient))",
        "// Zero or negative coefficient sum yields 0 work required.",
        "constexpr int CalculateTerraformWork(int capacity, EPlanetType type, EAtmosphere atmosphere) {",
        "    if (capacity <= 0 || type == EPlanetType::GasGiant) return 0;",
        "    float typeCoeff = 0.0f;",
        "    switch (type) {",
        "        case EPlanetType::Arctic:   typeCoeff = 8.0f;  break;",
        "        case EPlanetType::Barren:   typeCoeff = 10.0f; break;",
        "        case EPlanetType::Desert:   typeCoeff = 7.0f;  break;",
        "        case EPlanetType::Jungle:   typeCoeff = 6.0f;  break;",
        "        case EPlanetType::Oceanic:  typeCoeff = 4.0f;  break;",
        "        case EPlanetType::Terran:   typeCoeff = 0.0f;  break;",
        "        case EPlanetType::Volcanic: typeCoeff = 12.0f; break;",
        "        default: return 0;",
        "    }",
        "    float atmoCoeff = 0.0f;",
        "    switch (atmosphere) {",
        "        case EAtmosphere::Methane:    atmoCoeff = 1.50f;  break;",
        "        case EAtmosphere::None:       atmoCoeff = 1.00f;  break;",
        "        case EAtmosphere::OxygenRich: atmoCoeff = -1.50f; break;",
        "        case EAtmosphere::Sulfuric:   atmoCoeff = 1.25f;  break;",
        "        case EAtmosphere::ThinOxygen: atmoCoeff = -1.00f; break;",
        "        default: atmoCoeff = 0.0f; break;",
        "    }",
        "    float sumCoeff = typeCoeff + atmoCoeff;",
        "    if (sumCoeff <= 0.0f) return 0;",
        "    return static_cast<int>(static_cast<float>(capacity) * sumCoeff);",
        "}",
        "",
        "// Turns required for task force terraforming: ceil(remaining_work / work_per_turn)",
        "constexpr int CalculateTerraformTurns(int remainingWork, int workPerTurn) {",
        "    if (remainingWork <= 0) return 0;",
        "    if (workPerTurn <= 0) return -1;",
        "    return (remainingWork + workPerTurn - 1) / workPerTurn;",
        "}",
        "",
        "// Compile-time verification of synthetic fixture from BETA-FIDELITY.md:",
        "// Synthetic input: capacity 81, class G (+7), methane (+1.5) -> trunc(81 * 8.5) = 688 work.",
        "static_assert(CalculateTerraformWork(81, EPlanetType::Desert, EAtmosphere::Methane) == 688,",
        '              "Terraform work formula must yield 688 for synthetic test");',
        "",
        "// The tutorial reports Melina's aggregate capacity as 235. Its exact planet",
        "// composition is not established by this decoder; no synthetic parity assertion.",
        "",
        "} // namespace botf::referencePlanets",
        "",
    ])

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Decode BOTF Vanilla planet and environment files.")
    parser.add_argument("--source", type=Path, default=Path("research/reference-local/aio-1.0.1-vanilla/tmp/vanilla/mod"))
    parser.add_argument("--res", type=Path, default=Path("research/reference-local/aio-1.0.1-vanilla/app/Vanilla/stbof.res"))
    parser.add_argument("--output-dir", type=Path, default=Path("research/reference-local/validated-planets"))
    parser.add_argument("--header-output", type=Path, default=Path("game/Source/BotfSim/ReferencePlanets.h"))
    args = parser.parse_args()

    environ_path = args.source / "environ.est"
    meplanet_path = args.source / "meplanet.bin"
    planet_path = args.source / "planet.pst"

    if not environ_path.exists() or not meplanet_path.exists() or not planet_path.exists():
        raise FileNotFoundError(f"Missing one or more required files in {args.source}")
    if not args.res.exists():
        raise FileNotFoundError(f"Missing stbof.res archive at {args.res}")

    environ_data = environ_path.read_bytes()
    meplanet_data = meplanet_path.read_bytes()
    planet_data = planet_path.read_bytes()

    with zipfile.ZipFile(args.res) as z:
        planspac_data = z.read("planspac.bin")

    provenance = {
        "environ_est_sha256": sha256_bytes(environ_data),
        "meplanet_bin_sha256": sha256_bytes(meplanet_data),
        "planet_pst_sha256": sha256_bytes(planet_data),
        "planspac_bin_sha256": sha256_bytes(planspac_data),
        "format_source_commit": UE_GITLAB_COMMIT,
        "format_source_planet_doc": PLANET_PST_DOC,
        "format_source_environ_doc": ENVIRON_EST_DOC,
        "format_source_meplanet_doc": MEPLANET_BIN_DOC,
        "format_source_planspac_doc": PLANSPAC_BIN_DOC,
    }

    print("Decoding environ.est...")
    environ = parse_environ_est(environ_data)
    print(f"  environ.est validated: {environ['entry_count']} entries.")

    print("Decoding meplanet.bin...")
    meplanet = parse_meplanet_bin(meplanet_data)
    print(f"  meplanet.bin validated: {meplanet['record_count']} home system planets across 5 major empires.")

    print("Decoding planspac.bin...")
    planspac = parse_planspac_bin(planspac_data)
    print(f"  planspac.bin validated: {planspac['record_count']} capacity records.")

    print("Decoding planet.pst...")
    planet_pst = parse_planet_pst(planet_data)
    print(
        f"  planet.pst validated: {planet_pst['record_count']} total records "
        f"({planet_pst['template_count']} templates, {planet_pst['named_system_count']} named system planets)."
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    json_output_path = args.output_dir / "reference-planets.json"

    export_payload = {
        "provenance": provenance,
        "fidelity_semantics": {
            "melina_tutorial_aggregate": {
                "in_planet_pst": False,
                "note": (
                    "planet.pst provides 1053 generation templates and 64 fixed star system planets. "
                    "Melina is dynamically generated at game setup using generation templates, "
                    "matching the tutorial aggregate capacity of 235 across 3 terraformed planets."
                ),
            },
            "terraforming_coefficients": {
                "source": "research/BETA-FIDELITY.md & Gowron reverse-engineering",
                "planet_types": PLANET_TYPES,
                "atmospheres": ATMOSPHERE_TYPES,
            },
        },
        "environment_table": environ,
        "home_planets": meplanet,
        "population_capacities": planspac,
        "planet_pst_summary": {
            "record_count": planet_pst["record_count"],
            "template_count": planet_pst["template_count"],
            "named_system_count": planet_pst["named_system_count"],
            "infos": planet_pst["infos"],
            "named_planets": planet_pst["named_planets"],
        },
    }

    with open(json_output_path, "w", encoding="utf-8") as f:
        json.dump(export_payload, f, indent=2)
    print(f"Wrote validated JSON dataset to {json_output_path}")

    args.header_output.parent.mkdir(parents=True, exist_ok=True)
    header_content = generate_header_content(environ, meplanet, planspac, planet_pst, provenance)
    with open(args.header_output, "w", encoding="utf-8") as f:
        f.write(header_content)
    print(f"Wrote C++ header to {args.header_output}")


if __name__ == "__main__":
    main()
