// BOTF Remastered reference catalogue: technologies, legacy ships, buildings and races.
// The records are DATA since M0 (docs/DATA-FORMAT.md): data/base/{technologies,ships,buildings,races}.json, loaded
// from the content pack the host installs (Content.h), the embedded base pack by default. Numeric values came from
// research/export-reference.py over local AIO Vanilla data (provenance SHA256 manifest:
// 80a47a7fc29820d774328bb0b6c57501ff213cfbbd81588697463ffc6216e4fe); every `description` is ORIGINAL remaster
// text, and ContentTests guards against known original phrases returning.
#pragma once
#include "Content.h"

namespace botf::referenceData {
inline constexpr content::Table<TechnologyRecord> TechnologyCatalogue{&content::tables::technologies};
inline constexpr content::Table<ShipRecord> ShipCatalogue{&content::tables::ships};
inline constexpr content::Table<BuildingRecord> BuildingCatalogue{&content::tables::buildings};
inline constexpr content::Table<RaceRecord> RaceCatalogue{&content::tables::races};
} // namespace botf::referenceData
