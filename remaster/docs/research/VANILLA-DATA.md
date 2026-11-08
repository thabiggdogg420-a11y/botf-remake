# BOTF Remastered: Recovered Vanilla Reference Data

Compiled by Tuvok (Lane L1). Source: Local AIO Vanilla reference data (`remaster/game/Source/BotfSim/ReferenceTechnologies.h`, `remaster/game/Source/BotfSim/ReferenceCatalogue.h`, `research/export-reference.py` SHA256 `fc4e08986907208ac546beee01fdbb04dbedf52af8825419de672f7c96a1f691`).
This document closes the research gaps identified in `BOTF-FEATURE-REFERENCE.md` §1.3 (per-level technology unlocks) and §1.12 (victory threshold discrepancies).

---

## 1. Technologies & Research Costs (6 Fields + Sociology)

Vanilla BOTF has 6 player-directed research fields (levels 0-10), plus internal/social structures indexed under field 6 (Sociology) in binary files `techtree.tec` and `edifice.bst`.

| Field ID | Field Name | Level | Technology Name | Research Cost | Source Reference |
|---|---|---|---|---|---|
| 0 | Biotech | 0 | Basic Biotech | 0 | `ReferenceCatalogue.h:18` / `ReferenceTechnologies.h:13` |
| 0 | Biotech | 1 | Hydroponics | 0 | `ReferenceCatalogue.h:19` / `ReferenceTechnologies.h:14` |
| 0 | Biotech | 2 | Bionics | 200 | `ReferenceCatalogue.h:20` / `ReferenceTechnologies.h:15` |
| 0 | Biotech | 3 | Genetics | 1,100 | `ReferenceCatalogue.h:21` / `ReferenceTechnologies.h:16` |
| 0 | Biotech | 4 | Metagenics | 4,000 | `ReferenceCatalogue.h:22` / `ReferenceTechnologies.h:17` |
| 0 | Biotech | 5 | Climate Control | 9,000 | `ReferenceCatalogue.h:23` / `ReferenceTechnologies.h:18` |
| 0 | Biotech | 6 | Air Conversion | 15,400 | `ReferenceCatalogue.h:24` / `ReferenceTechnologies.h:19` |
| 0 | Biotech | 7 | Biofiltration | 25,800 | `ReferenceCatalogue.h:25` / `ReferenceTechnologies.h:20` |
| 0 | Biotech | 8 | Eco-Modification | 39,100 | `ReferenceCatalogue.h:26` / `ReferenceTechnologies.h:21` |
| 0 | Biotech | 9 | Xenobiology | 58,400 | `ReferenceCatalogue.h:27` / `ReferenceTechnologies.h:22` |
| 0 | Biotech | 10 | Silicobiology | 81,700 | `ReferenceCatalogue.h:28` / `ReferenceTechnologies.h:23` |
| 1 | Computers | 0 | Basic Computers | 0 | `ReferenceCatalogue.h:29` / `ReferenceTechnologies.h:26` |
| 1 | Computers | 1 | Microtape Storage | 0 | `ReferenceCatalogue.h:30` / `ReferenceTechnologies.h:27` |
| 1 | Computers | 2 | Linguistics | 200 | `ReferenceCatalogue.h:31` / `ReferenceTechnologies.h:28` |
| 1 | Computers | 3 | Data Imaging | 1,300 | `ReferenceCatalogue.h:32` / `ReferenceTechnologies.h:29` |
| 1 | Computers | 4 | Duotronics | 4,500 | `ReferenceCatalogue.h:33` / `ReferenceTechnologies.h:30` |
| 1 | Computers | 5 | Crystal Memory | 10,300 | `ReferenceCatalogue.h:34` / `ReferenceTechnologies.h:31` |
| 1 | Computers | 6 | Quadritronics | 17,600 | `ReferenceCatalogue.h:35` / `ReferenceTechnologies.h:32` |
| 1 | Computers | 7 | Isolinear Optics | 29,500 | `ReferenceCatalogue.h:36` / `ReferenceTechnologies.h:33` |
| 1 | Computers | 8 | Circuit Distortion | 44,600 | `ReferenceCatalogue.h:37` / `ReferenceTechnologies.h:34` |
| 1 | Computers | 9 | Ultraconduction | 66,800 | `ReferenceCatalogue.h:38` / `ReferenceTechnologies.h:35` |
| 1 | Computers | 10 | Positronics | 93,400 | `ReferenceCatalogue.h:39` / `ReferenceTechnologies.h:36` |
| 2 | Construction | 0 | Basic Construction | 0 | `ReferenceCatalogue.h:40` / `ReferenceTechnologies.h:39` |
| 2 | Construction | 1 | Gamma Welding | 0 | `ReferenceCatalogue.h:41` / `ReferenceTechnologies.h:40` |
| 2 | Construction | 2 | Integrity Fields | 300 | `ReferenceCatalogue.h:42` / `ReferenceTechnologies.h:41` |
| 2 | Construction | 3 | Epitaxy | 1,600 | `ReferenceCatalogue.h:43` / `ReferenceTechnologies.h:42` |
| 2 | Construction | 4 | Phase Transition | 5,700 | `ReferenceCatalogue.h:44` / `ReferenceTechnologies.h:43` |
| 2 | Construction | 5 | Warp Fields | 12,900 | `ReferenceCatalogue.h:45` / `ReferenceTechnologies.h:44` |
| 2 | Construction | 6 | Diffusion Bonding | 22,000 | `ReferenceCatalogue.h:46` / `ReferenceTechnologies.h:45` |
| 2 | Construction | 7 | Nuclear Epitaxy | 36,900 | `ReferenceCatalogue.h:47` / `ReferenceTechnologies.h:46` |
| 2 | Construction | 8 | Electron Bonding | 55,800 | `ReferenceCatalogue.h:48` / `ReferenceTechnologies.h:47` |
| 2 | Construction | 9 | Quantum Epitaxy | 83,500 | `ReferenceCatalogue.h:49` / `ReferenceTechnologies.h:48` |
| 2 | Construction | 10 | Holographics | 116,800 | `ReferenceCatalogue.h:50` / `ReferenceTechnologies.h:49` |
| 3 | Energy | 0 | Basic Energy | 0 | `ReferenceCatalogue.h:51` / `ReferenceTechnologies.h:52` |
| 3 | Energy | 1 | Force Fields | 0 | `ReferenceCatalogue.h:52` / `ReferenceTechnologies.h:53` |
| 3 | Energy | 2 | Subspace Physics | 100 | `ReferenceCatalogue.h:53` / `ReferenceTechnologies.h:54` |
| 3 | Energy | 3 | Matter Conversion | 900 | `ReferenceCatalogue.h:54` / `ReferenceTechnologies.h:55` |
| 3 | Energy | 4 | Graviton Physics | 3,400 | `ReferenceCatalogue.h:55` / `ReferenceTechnologies.h:56` |
| 3 | Energy | 5 | Power Distribution | 7,700 | `ReferenceCatalogue.h:56` / `ReferenceTechnologies.h:57` |
| 3 | Energy | 6 | Antimatter Fusion | 13,200 | `ReferenceCatalogue.h:57` / `ReferenceTechnologies.h:58` |
| 3 | Energy | 7 | Virtual Particles | 22,100 | `ReferenceCatalogue.h:58` / `ReferenceTechnologies.h:59` |
| 3 | Energy | 8 | Ultraconductors | 33,500 | `ReferenceCatalogue.h:59` / `ReferenceTechnologies.h:60` |
| 3 | Energy | 9 | Wave Analysis | 50,100 | `ReferenceCatalogue.h:60` / `ReferenceTechnologies.h:61` |
| 3 | Energy | 10 | Determinacy | 70,000 | `ReferenceCatalogue.h:61` / `ReferenceTechnologies.h:62` |
| 4 | Propulsion | 0 | Basic Propulsion | 0 | `ReferenceCatalogue.h:62` / `ReferenceTechnologies.h:65` |
| 4 | Propulsion | 1 | Warp Drive | 0 | `ReferenceCatalogue.h:63` / `ReferenceTechnologies.h:66` |
| 4 | Propulsion | 2 | Distortion | 200 | `ReferenceCatalogue.h:64` / `ReferenceTechnologies.h:67` |
| 4 | Propulsion | 3 | Plasma Injection | 1,400 | `ReferenceCatalogue.h:65` / `ReferenceTechnologies.h:68` |
| 4 | Propulsion | 4 | Microdistortion | 5,100 | `ReferenceCatalogue.h:66` / `ReferenceTechnologies.h:69` |
| 4 | Propulsion | 5 | Field Manipulation | 11,600 | `ReferenceCatalogue.h:67` / `ReferenceTechnologies.h:70` |
| 4 | Propulsion | 6 | Field Geometry | 19,800 | `ReferenceCatalogue.h:68` / `ReferenceTechnologies.h:71` |
| 4 | Propulsion | 7 | Superconduction | 33,200 | `ReferenceCatalogue.h:69` / `ReferenceTechnologies.h:72` |
| 4 | Propulsion | 8 | Field Compression | 50,200 | `ReferenceCatalogue.h:70` / `ReferenceTechnologies.h:73` |
| 4 | Propulsion | 9 | Charge Reversal | 75,100 | `ReferenceCatalogue.h:71` / `ReferenceTechnologies.h:74` |
| 4 | Propulsion | 10 | Ultraconductivity | 105,100 | `ReferenceCatalogue.h:72` / `ReferenceTechnologies.h:75` |
| 5 | Weapons | 0 | Basic Weapons | 0 | `ReferenceCatalogue.h:73` / `ReferenceTechnologies.h:78` |
| 5 | Weapons | 1 | Beam Emitters | 0 | `ReferenceCatalogue.h:74` / `ReferenceTechnologies.h:79` |
| 5 | Weapons | 2 | Torpedo Hardening | 300 | `ReferenceCatalogue.h:75` / `ReferenceTechnologies.h:80` |
| 5 | Weapons | 3 | Beam Hardening | 1,800 | `ReferenceCatalogue.h:76` / `ReferenceTechnologies.h:81` |
| 5 | Weapons | 4 | Matter Packets | 6,200 | `ReferenceCatalogue.h:77` / `ReferenceTechnologies.h:82` |
| 5 | Weapons | 5 | Multifreq. Beams | 14,200 | `ReferenceCatalogue.h:78` / `ReferenceTechnologies.h:83` |
| 5 | Weapons | 6 | Subspace Guidance | 24,200 | `ReferenceCatalogue.h:79` / `ReferenceTechnologies.h:84` |
| 5 | Weapons | 7 | Micropulsing | 40,600 | `ReferenceCatalogue.h:80` / `ReferenceTechnologies.h:85` |
| 5 | Weapons | 8 | Compression | 61,400 | `ReferenceCatalogue.h:81` / `ReferenceTechnologies.h:86` |
| 5 | Weapons | 9 | Quantum Shielding | 91,800 | `ReferenceCatalogue.h:82` / `ReferenceTechnologies.h:87` |
| 5 | Weapons | 10 | Ultra. Emitters | 128,500 | `ReferenceCatalogue.h:83` / `ReferenceTechnologies.h:88` |
| 6 | Sociology | 0 | Sociology 0 | 0 | `ReferenceCatalogue.h:84` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 1 | Sociology 1 | 0 | `ReferenceCatalogue.h:85` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 2 | Sociology 2 | 200 | `ReferenceCatalogue.h:86` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 3 | Sociology 3 | 1,100 | `ReferenceCatalogue.h:87` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 4 | Sociology 4 | 4,000 | `ReferenceCatalogue.h:88` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 5 | Sociology 5 | 9,000 | `ReferenceCatalogue.h:89` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 6 | Sociology 6 | 15,400 | `ReferenceCatalogue.h:90` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 7 | Sociology 7 | 25,800 | `ReferenceCatalogue.h:91` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 8 | Sociology 8 | 39,100 | `ReferenceCatalogue.h:92` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 9 | Sociology 9 | 58,400 | `ReferenceCatalogue.h:93` / `ReferenceTechnologies.h:n/a` |
| 6 | Sociology | 10 | Sociology 10 | 81,700 | `ReferenceCatalogue.h:94` / `ReferenceTechnologies.h:n/a` |

---

## 2. Structure Unlocks per Tech Field & Level (318 Buildings)

In vanilla BOTF, structures unlock when their required technology field reaches the specified level. Buildings in Field 6 (Sociology) represent empire intelligence, science facilities, national monuments, and minor-race structures.

### 2.1 Field 0: Biotech

| Level | ID | Structure Name | Industry Cost | Energy | Pop | Morale | Excluded Mask | Source Reference |
|---|---|---|---|---|---|---|---|---|
| 0 | 97 | Primitive Farm | 10 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:321` |
| 0 | 143 | Primitive Farm | 10 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:367` |
| 0 | 189 | Primitive Farm | 10 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:413` |
| 0 | 235 | Primitive Farm | 10 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:459` |
| 0 | 281 | Primitive Farm | 10 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:505` |
| 1 | 60 | Harvesting Complex | 2000 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:284` |
| 1 | 72 | Mintakan Farm | 2000 | 0 | 0 | 0 | 0 | `ReferenceCatalogue.h:296` |
| 1 | 77 | Bioengineering Center | 3500 | 200 | 0 | 0 | 0 | `ReferenceCatalogue.h:301` |
| 1 | 98 | Type 1 Hydroponic Farm | 55 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:322` |
| 1 | 144 | Type 1 Automated Farm | 60 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:368` |
| 1 | 190 | Class 1 Grower's Market | 35 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:414` |
| 1 | 236 | Class 1 Hunting Ground | 85 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:460` |
| 1 | 282 | Mark I Organics Plant | 35 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:506` |
| 2 | 99 | Type 2 Hydroponic Farm | 160 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:323` |
| 2 | 145 | Type 2 Automated Farm | 165 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:369` |
| 2 | 191 | Class 2 Grower's Market | 105 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:415` |
| 2 | 237 | Class 2 Hunting Ground | 205 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:461` |
| 2 | 283 | Mark II Organics Plant | 125 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:507` |
| 3 | 8 | Aquaculture Center | 300 | 0 | 0 | 0 | 0 | `ReferenceCatalogue.h:232` |
| 3 | 100 | Type 3 Hydroponic Farm | 330 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:324` |
| 3 | 146 | Type 3 Automated Farm | 320 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:370` |
| 3 | 192 | Class 3 Grower's Market | 235 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:416` |
| 3 | 238 | Class 3 Hunting Ground | 410 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:462` |
| 3 | 284 | Mark III Organics Plant | 265 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:508` |
| 4 | 7 | Phoenix Facility | 400 | 30 | 0 | 0 | 14 | `ReferenceCatalogue.h:231` |
| 4 | 34 | Genesis Research Lab | 2500 | 100 | 0 | 1 | 29 | `ReferenceCatalogue.h:258` |
| 4 | 86 | Organic Regenerators | 400 | 40 | 0 | 0 | 15 | `ReferenceCatalogue.h:310` |
| 4 | 101 | Type 4 Hydroponic Farm | 640 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:325` |
| 4 | 147 | Type 4 Automated Farm | 630 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:371` |
| 4 | 193 | Class 4 Grower's Market | 470 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:417` |
| 4 | 239 | Class 4 Hunting Ground | 750 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:463` |
| 4 | 285 | Mark IV Organics Plant | 530 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:509` |
| 5 | 102 | Type 5 Hydroponic Farm | 1075 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:326` |
| 5 | 148 | Type 5 Automated Farm | 1040 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:372` |
| 5 | 194 | Class 5 Grower's Market | 830 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:418` |
| 5 | 240 | Class 5 Hunting Ground | 1240 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:464` |
| 5 | 286 | Mark V Organics Plant | 910 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:510` |
| 6 | 103 | Type 6 Hydroponic Farm | 1740 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:327` |
| 6 | 149 | Type 6 Automated Farm | 1705 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:373` |
| 6 | 195 | Class 6 Grower's Market | 1370 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:419` |
| 6 | 241 | Class 6 Hunting Ground | 1950 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:465` |
| 6 | 287 | Mark VI Organics Plant | 1510 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:511` |
| 7 | 104 | Type 7 Hydroponic Farm | 2590 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:328` |
| 7 | 150 | Type 7 Automated Farm | 2520 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:374` |
| 7 | 196 | Class 7 Grower's Market | 2110 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:420` |
| 7 | 242 | Class 7 Hunting Ground | 2885 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:466` |
| 7 | 288 | Mark VII Organics Plant | 2280 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:512` |
| 8 | 105 | Type 8 Hydroponic Farm | 3760 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:329` |
| 8 | 151 | Type 8 Automated Farm | 3690 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:375` |
| 8 | 197 | Class 8 Grower's Market | 3115 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:421` |
| 8 | 243 | Class 8 Hunting Ground | 4115 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:467` |
| 8 | 289 | Mark VIII Organics Plant | 3355 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:513` |
| 9 | 106 | Type 9 Hydroponic Farm | 5175 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:330` |
| 9 | 152 | Type 9 Automated Farm | 5060 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:376` |
| 9 | 198 | Class 9 Grower's Market | 4385 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:422` |
| 9 | 244 | Class 9 Hunting Ground | 5640 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:468` |
| 9 | 290 | Mark IX Organics Plant | 4665 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:514` |

### 2.2 Field 1: Computers

| Level | ID | Structure Name | Industry Cost | Energy | Pop | Morale | Excluded Mask | Source Reference |
|---|---|---|---|---|---|---|---|---|
| 1 | 66 | Planetary Computer | 2500 | 150 | 0 | 0 | 0 | `ReferenceCatalogue.h:290` |
| 1 | 70 | Ktarian Game Studio | 3500 | 100 | 0 | 0 | 0 | `ReferenceCatalogue.h:294` |
| 2 | 12 | Subspace Scanner | 500 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:236` |
| 3 | 4 | Communications Grid | 300 | 10 | 0 | 0 | 0 | `ReferenceCatalogue.h:228` |
| 3 | 17 | Trade Center | 500 | 20 | 0 | 0 | 25 | `ReferenceCatalogue.h:241` |
| 4 | 29 | Holo Cinema | 2000 | 50 | 0 | 0 | 27 | `ReferenceCatalogue.h:253` |
| 4 | 35 | Daystrom Institute | 2500 | 100 | 0 | 1 | 29 | `ReferenceCatalogue.h:259` |
| 5 | 10 | Theoretical Simulators | 500 | 20 | 0 | 0 | 18 | `ReferenceCatalogue.h:234` |
| 5 | 13 | Listening Post | 2000 | 100 | 0 | 0 | 0 | `ReferenceCatalogue.h:237` |
| 6 | 24 | Covert Sensor Array | 5000 | 300 | 0 | 0 | 30 | `ReferenceCatalogue.h:248` |
| 8 | 14 | Isolinear Scanner | 8000 | 150 | 0 | 0 | 1 | `ReferenceCatalogue.h:238` |

### 2.3 Field 2: Construction

| Level | ID | Structure Name | Industry Cost | Energy | Pop | Morale | Excluded Mask | Source Reference |
|---|---|---|---|---|---|---|---|---|
| 0 | 15 | Basic Shipyard | 500 | 25 | 0 | 0 | 0 | `ReferenceCatalogue.h:239` |
| 0 | 21 | Union Shipyard | 500 | 25 | 0 | 0 | 30 | `ReferenceCatalogue.h:245` |
| 0 | 52 | Alliance Shipyard | 500 | 25 | 0 | 0 | 27 | `ReferenceCatalogue.h:276` |
| 0 | 54 | Starfleet Shipyard | 500 | 25 | 0 | 0 | 29 | `ReferenceCatalogue.h:278` |
| 0 | 55 | Military Shipyard | 500 | 25 | 0 | 0 | 23 | `ReferenceCatalogue.h:279` |
| 0 | 87 | Imperial Shipyard | 500 | 25 | 0 | 0 | 15 | `ReferenceCatalogue.h:311` |
| 1 | 6 | Dilithium Refinery | 500 | 30 | 0 | 0 | 0 | `ReferenceCatalogue.h:230` |
| 1 | 62 | Architectural Center | 2500 | 100 | 0 | 0 | 0 | `ReferenceCatalogue.h:286` |
| 1 | 63 | Industrial Center | 5000 | 100 | 0 | 0 | 0 | `ReferenceCatalogue.h:287` |
| 1 | 88 | Type 1 Assembly Yard | 110 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:312` |
| 1 | 134 | Type 1 Mass Replicator | 110 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:358` |
| 1 | 180 | Class 1 Machining Guild | 65 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:404` |
| 1 | 226 | Class 1 Foundry | 145 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:450` |
| 1 | 272 | Mark I Fabricator | 80 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:496` |
| 2 | 89 | Type 2 Assembly Yard | 270 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:313` |
| 2 | 135 | Type 2 Mass Replicator | 270 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:359` |
| 2 | 181 | Class 2 Machining Guild | 195 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:405` |
| 2 | 227 | Class 2 Foundry | 325 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:451` |
| 2 | 273 | Mark II Fabricator | 225 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:497` |
| 3 | 2 | Bunker Network | 300 | 10 | 0 | 0 | 4 | `ReferenceCatalogue.h:226` |
| 3 | 90 | Type 3 Assembly Yard | 550 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:314` |
| 3 | 136 | Type 3 Mass Replicator | 520 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:360` |
| 3 | 182 | Class 3 Machining Guild | 415 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:406` |
| 3 | 228 | Class 3 Foundry | 650 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:452` |
| 3 | 274 | Mark III Fabricator | 455 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:498` |
| 4 | 91 | Type 4 Assembly Yard | 1025 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:315` |
| 4 | 137 | Type 4 Mass Replicator | 990 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:361` |
| 4 | 183 | Class 4 Machining Guild | 780 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:407` |
| 4 | 229 | Class 4 Foundry | 1155 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:453` |
| 4 | 275 | Mark IV Fabricator | 870 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:499` |
| 5 | 38 | Utopia Planitia | 7000 | 100 | 0 | 1 | 29 | `ReferenceCatalogue.h:262` |
| 5 | 92 | Type 5 Assembly Yard | 1720 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:316` |
| 5 | 138 | Type 5 Mass Replicator | 1640 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:362` |
| 5 | 184 | Class 5 Machining Guild | 1370 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:408` |
| 5 | 230 | Class 5 Foundry | 1930 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:454` |
| 5 | 276 | Mark V Fabricator | 1480 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:500` |
| 6 | 93 | Type 6 Assembly Yard | 2725 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:317` |
| 6 | 139 | Type 6 Mass Replicator | 2640 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:363` |
| 6 | 185 | Class 6 Machining Guild | 2205 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:409` |
| 6 | 231 | Class 6 Foundry | 2990 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:455` |
| 6 | 277 | Mark VI Fabricator | 2390 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:501` |
| 7 | 94 | Type 7 Assembly Yard | 4070 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:318` |
| 7 | 140 | Type 7 Mass Replicator | 3920 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:364` |
| 7 | 186 | Class 7 Machining Guild | 3390 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:410` |
| 7 | 232 | Class 7 Foundry | 4445 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:456` |
| 7 | 278 | Mark VII Fabricator | 3620 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:502` |
| 8 | 95 | Type 8 Assembly Yard | 5830 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:319` |
| 8 | 141 | Type 8 Mass Replicator | 5670 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:365` |
| 8 | 187 | Class 8 Machining Guild | 4920 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:411` |
| 8 | 233 | Class 8 Foundry | 6270 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:457` |
| 8 | 279 | Mark VIII Fabricator | 5245 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:503` |
| 9 | 96 | Type 9 Assembly Yard | 8050 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:320` |
| 9 | 142 | Type 9 Mass Replicator | 7810 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:366` |
| 9 | 188 | Class 9 Machining Guild | 6935 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:412` |
| 9 | 234 | Class 9 Foundry | 8640 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:458` |
| 9 | 280 | Mark IX Fabricator | 7315 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:504` |

### 2.4 Field 3: Energy

| Level | ID | Structure Name | Industry Cost | Energy | Pop | Morale | Excluded Mask | Source Reference |
|---|---|---|---|---|---|---|---|---|
| 1 | 78 | Physics Institute | 2000 | 200 | 0 | 0 | 0 | `ReferenceCatalogue.h:302` |
| 1 | 79 | Defense Network | 3000 | 150 | 0 | 0 | 0 | `ReferenceCatalogue.h:303` |
| 1 | 116 | Type 1 Fusion Plant | 55 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:340` |
| 1 | 162 | Type 1 Plasma Reactor | 60 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:386` |
| 1 | 208 | Class 1 Power Exchange | 35 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:432` |
| 1 | 254 | Class 1 Matter Furnace | 85 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:478` |
| 1 | 300 | Mark I Gravitic Turbine | 35 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:524` |
| 2 | 19 | Wind Turbines | 200 | 0 | 0 | 0 | 0 | `ReferenceCatalogue.h:243` |
| 2 | 117 | Type 2 Fusion Plant | 160 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:341` |
| 2 | 163 | Type 2 Plasma Reactor | 165 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:387` |
| 2 | 209 | Class 2 Power Exchange | 105 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:433` |
| 2 | 255 | Class 2 Matter Furnace | 205 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:479` |
| 2 | 301 | Mark II Gravitic Turbine | 125 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:525` |
| 3 | 1 | Basic Replicators | 300 | 10 | 0 | 0 | 22 | `ReferenceCatalogue.h:225` |
| 3 | 5 | Orbital Battery | 500 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:229` |
| 3 | 118 | Type 3 Fusion Plant | 330 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:342` |
| 3 | 164 | Type 3 Plasma Reactor | 320 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:388` |
| 3 | 210 | Class 3 Power Exchange | 235 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:434` |
| 3 | 256 | Class 3 Matter Furnace | 410 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:480` |
| 3 | 302 | Mark III Gravitic Turbine | 265 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:526` |
| 4 | 0 | Advanced Replicators | 400 | 10 | 0 | 0 | 11 | `ReferenceCatalogue.h:224` |
| 4 | 20 | Shield Generator | 500 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:244` |
| 4 | 47 | Astrophysics Academy | 2500 | 100 | 0 | 1 | 15 | `ReferenceCatalogue.h:271` |
| 4 | 119 | Type 4 Fusion Plant | 640 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:343` |
| 4 | 165 | Type 4 Plasma Reactor | 630 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:389` |
| 4 | 211 | Class 4 Power Exchange | 470 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:435` |
| 4 | 257 | Class 4 Matter Furnace | 750 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:481` |
| 4 | 303 | Mark IV Gravitic Turbine | 530 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:527` |
| 5 | 3 | Charge Collectors | 500 | 0 | 0 | 0 | 0 | `ReferenceCatalogue.h:227` |
| 5 | 53 | Federation Replicator | 500 | 10 | 0 | 0 | 29 | `ReferenceCatalogue.h:277` |
| 5 | 120 | Type 5 Fusion Plant | 1075 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:344` |
| 5 | 166 | Type 5 Plasma Reactor | 1040 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:390` |
| 5 | 212 | Class 5 Power Exchange | 830 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:436` |
| 5 | 258 | Class 5 Matter Furnace | 1240 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:482` |
| 5 | 304 | Mark V Gravitic Turbine | 910 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:528` |
| 6 | 11 | Subatomic Simulators | 600 | 80 | 0 | 0 | 13 | `ReferenceCatalogue.h:235` |
| 6 | 121 | Type 6 Fusion Plant | 1740 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:345` |
| 6 | 167 | Type 6 Plasma Reactor | 1705 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:391` |
| 6 | 213 | Class 6 Power Exchange | 1370 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:437` |
| 6 | 259 | Class 6 Matter Furnace | 1950 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:483` |
| 6 | 305 | Mark VI Gravitic Turbine | 1510 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:529` |
| 7 | 122 | Type 7 Fusion Plant | 2590 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:346` |
| 7 | 168 | Type 7 Plasma Reactor | 2520 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:392` |
| 7 | 214 | Class 7 Power Exchange | 2110 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:438` |
| 7 | 260 | Class 7 Matter Furnace | 2885 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:484` |
| 7 | 306 | Mark VII Gravitic Turbine | 2280 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:530` |
| 8 | 123 | Type 8 Fusion Plant | 3760 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:347` |
| 8 | 169 | Type 8 Plasma Reactor | 3690 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:393` |
| 8 | 215 | Class 8 Power Exchange | 3115 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:439` |
| 8 | 261 | Class 8 Matter Furnace | 4115 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:485` |
| 8 | 307 | Mark VIII Gravitic Turbine | 3355 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:531` |
| 9 | 124 | Type 9 Fusion Plant | 5175 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:348` |
| 9 | 170 | Type 9 Plasma Reactor | 5060 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:394` |
| 9 | 216 | Class 9 Power Exchange | 4385 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:440` |
| 9 | 262 | Class 9 Matter Furnace | 5640 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:486` |
| 9 | 308 | Mark IX Gravitic Turbine | 4665 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:532` |

### 2.5 Field 4: Propulsion

| Level | ID | Structure Name | Industry Cost | Energy | Pop | Morale | Excluded Mask | Source Reference |
|---|---|---|---|---|---|---|---|---|
| 1 | 71 | Kinetics Laboratory | 3000 | 150 | 0 | 0 | 0 | `ReferenceCatalogue.h:295` |
| 1 | 75 | Collection Facility | 1500 | 150 | 0 | 0 | 0 | `ReferenceCatalogue.h:299` |
| 5 | 46 | Singularity Plant | 2500 | 800 | 0 | 0 | 15 | `ReferenceCatalogue.h:270` |

### 2.6 Field 5: Weapons

| Level | ID | Structure Name | Industry Cost | Energy | Pop | Morale | Excluded Mask | Source Reference |
|---|---|---|---|---|---|---|---|---|
| 1 | 58 | Super Soldier Academy | 3000 | 60 | 0 | 0 | 0 | `ReferenceCatalogue.h:282` |
| 1 | 68 | Gladiatorial Arena | 2500 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:292` |
| 4 | 45 | Combat Testing Center | 2500 | 100 | 0 | 1 | 23 | `ReferenceCatalogue.h:269` |

### 2.7 Field 6: Sociology

| Level | ID | Structure Name | Industry Cost | Energy | Pop | Morale | Excluded Mask | Source Reference |
|---|---|---|---|---|---|---|---|---|
| 0 | 18 | Trade Goods | 0 | 0 | 0 | 0 | 0 | `ReferenceCatalogue.h:242` |
| 0 | 27 | Inquisition | 0 | 0 | 0 | 0 | 30 | `ReferenceCatalogue.h:251` |
| 0 | 33 | Festival of Fun | 0 | 0 | 0 | 0 | 27 | `ReferenceCatalogue.h:257` |
| 0 | 39 | Martial Law | 0 | 0 | 0 | 0 | 29 | `ReferenceCatalogue.h:263` |
| 0 | 44 | Police State | 0 | 0 | 0 | 0 | 23 | `ReferenceCatalogue.h:268` |
| 0 | 51 | Tribunal | 0 | 0 | 0 | 0 | 15 | `ReferenceCatalogue.h:275` |
| 1 | 56 | Acamarian Clan Hall | 3000 | 50 | 0 | 1 | 0 | `ReferenceCatalogue.h:280` |
| 1 | 57 | Andorian War College | 5000 | 100 | 0 | 0 | 0 | `ReferenceCatalogue.h:281` |
| 1 | 59 | Mustering Base | 400 | 40 | 0 | 0 | 0 | `ReferenceCatalogue.h:283` |
| 1 | 61 | Bajoran Jalanda Forum | 10000 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:285` |
| 1 | 64 | Counseling Academy | 3000 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:288` |
| 1 | 65 | Cosmetology Center | 2000 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:289` |
| 1 | 67 | Research Think Tank | 3000 | 200 | 0 | 0 | 0 | `ReferenceCatalogue.h:291` |
| 1 | 69 | Palace of Edo | 7000 | 70 | 0 | 0 | 0 | `ReferenceCatalogue.h:293` |
| 1 | 73 | Monument of Surrender | 5000 | 0 | 0 | 0 | 0 | `ReferenceCatalogue.h:297` |
| 1 | 74 | Recruitment Center | 2000 | 40 | 0 | 0 | 0 | `ReferenceCatalogue.h:298` |
| 1 | 76 | Selay Mustering Base | 400 | 40 | 0 | 0 | 0 | `ReferenceCatalogue.h:300` |
| 1 | 80 | Mythology Library | 1500 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:304` |
| 1 | 81 | Research Committee | 5000 | 130 | 0 | 0 | 0 | `ReferenceCatalogue.h:305` |
| 1 | 82 | Psychohistorical Archive | 2000 | 80 | 0 | 0 | 0 | `ReferenceCatalogue.h:306` |
| 1 | 83 | Science Academy | 7500 | 170 | 0 | 0 | 0 | `ReferenceCatalogue.h:307` |
| 1 | 84 | Intelligence Service | 2500 | 50 | 0 | 0 | 0 | `ReferenceCatalogue.h:308` |
| 1 | 85 | Military Academy | 8000 | 200 | 0 | 0 | 0 | `ReferenceCatalogue.h:309` |
| 1 | 107 | Type 1 Shadow Team | 55 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:331` |
| 1 | 125 | Type 1 Science Center | 110 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:349` |
| 1 | 153 | Type 1 Databank | 60 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:377` |
| 1 | 171 | Type 1 University | 110 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:395` |
| 1 | 199 | Class 1 Data Brokerage | 35 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:423` |
| 1 | 217 | Class 1 Research Lab | 65 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:441` |
| 1 | 245 | Class 1 Security Center | 85 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:469` |
| 1 | 263 | Class 1 Hall of Learning | 145 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:487` |
| 1 | 291 | Mark I Intel Center | 35 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:515` |
| 1 | 309 | Mark I Laboratory | 80 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:533` |
| 2 | 16 | Forced-labor Farms | 200 | 0 | 0 | -1 | 22 | `ReferenceCatalogue.h:240` |
| 2 | 26 | Labor Camp | 1000 | 0 | 0 | -1 | 30 | `ReferenceCatalogue.h:250` |
| 2 | 32 | Franchise Office | 5000 | 100 | 0 | 1 | 27 | `ReferenceCatalogue.h:256` |
| 2 | 41 | Mining Prison | 500 | 20 | 0 | -1 | 23 | `ReferenceCatalogue.h:265` |
| 2 | 108 | Type 2 Shadow Team | 160 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:332` |
| 2 | 126 | Type 2 Science Center | 270 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:350` |
| 2 | 154 | Type 2 Databank | 165 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:378` |
| 2 | 172 | Type 2 University | 270 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:396` |
| 2 | 200 | Class 2 Data Brokerage | 105 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:424` |
| 2 | 218 | Class 2 Research Lab | 195 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:442` |
| 2 | 246 | Class 2 Security Center | 205 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:470` |
| 2 | 264 | Class 2 Hall of Learning | 325 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:488` |
| 2 | 292 | Mark II Intel Center | 125 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:516` |
| 2 | 310 | Mark II Laboratory | 225 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:534` |
| 3 | 9 | Private Farms | 1500 | 80 | 0 | 1 | 25 | `ReferenceCatalogue.h:233` |
| 3 | 22 | Re-education Center | 3000 | 100 | 0 | 0 | 30 | `ReferenceCatalogue.h:246` |
| 3 | 31 | Tower of Commerce | 2500 | 50 | 0 | 1 | 27 | `ReferenceCatalogue.h:255` |
| 3 | 36 | Federation Council  | 4000 | 125 | 0 | 0 | 29 | `ReferenceCatalogue.h:260` |
| 3 | 42 | Great Hall | 5000 | 100 | 0 | 0 | 23 | `ReferenceCatalogue.h:266` |
| 3 | 48 | Imperial Senate | 3000 | 170 | 0 | 0 | 15 | `ReferenceCatalogue.h:272` |
| 3 | 109 | Type 3 Shadow Team | 330 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:333` |
| 3 | 127 | Type 3 Science Center | 550 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:351` |
| 3 | 155 | Type 3 Databank | 320 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:379` |
| 3 | 173 | Type 3 University | 520 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:397` |
| 3 | 201 | Class 3 Data Brokerage | 235 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:425` |
| 3 | 219 | Class 3 Research Lab | 415 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:443` |
| 3 | 247 | Class 3 Security Center | 410 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:471` |
| 3 | 265 | Class 3 Hall of Learning | 650 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:489` |
| 3 | 293 | Mark III Intel Center | 265 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:517` |
| 3 | 311 | Mark III Laboratory | 455 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:535` |
| 4 | 23 | Obsidian Order | 7500 | 300 | 0 | 1 | 30 | `ReferenceCatalogue.h:247` |
| 4 | 30 | Commerce Authority | 4000 | 200 | 0 | 1 | 27 | `ReferenceCatalogue.h:254` |
| 4 | 43 | Tactical College | 5000 | 100 | 0 | 1 | 23 | `ReferenceCatalogue.h:267` |
| 4 | 49 | TalShiar Headquarters | 5000 | 200 | 0 | 1 | 15 | `ReferenceCatalogue.h:273` |
| 4 | 110 | Type 4 Shadow Team | 640 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:334` |
| 4 | 128 | Type 4 Science Center | 1025 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:352` |
| 4 | 156 | Type 4 Databank | 630 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:380` |
| 4 | 174 | Type 4 University | 990 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:398` |
| 4 | 202 | Class 4 Data Brokerage | 470 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:426` |
| 4 | 220 | Class 4 Research Lab | 780 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:444` |
| 4 | 248 | Class 4 Security Center | 750 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:472` |
| 4 | 266 | Class 4 Hall of Learning | 1155 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:490` |
| 4 | 294 | Mark IV Intel Center | 530 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:518` |
| 4 | 312 | Mark IV Laboratory | 870 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:536` |
| 5 | 25 | Central Command | 5000 | 100 | 0 | 1 | 30 | `ReferenceCatalogue.h:249` |
| 5 | 28 | Fantasy Land | 2500 | 200 | 0 | 0 | 27 | `ReferenceCatalogue.h:252` |
| 5 | 37 | Starfleet Academy | 5000 | 100 | 0 | 1 | 29 | `ReferenceCatalogue.h:261` |
| 5 | 40 | Hall of Warriors | 5000 | 100 | 0 | 1 | 23 | `ReferenceCatalogue.h:264` |
| 5 | 50 | Naval Academy | 5000 | 100 | 0 | 1 | 15 | `ReferenceCatalogue.h:274` |
| 5 | 111 | Type 5 Shadow Team | 1075 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:335` |
| 5 | 129 | Type 5 Science Center | 1720 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:353` |
| 5 | 157 | Type 5 Databank | 1040 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:381` |
| 5 | 175 | Type 5 University | 1640 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:399` |
| 5 | 203 | Class 5 Data Brokerage | 830 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:427` |
| 5 | 221 | Class 5 Research Lab | 1370 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:445` |
| 5 | 249 | Class 5 Security Center | 1240 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:473` |
| 5 | 267 | Class 5 Hall of Learning | 1930 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:491` |
| 5 | 295 | Mark V Intel Center | 910 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:519` |
| 5 | 313 | Mark V Laboratory | 1480 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:537` |
| 6 | 112 | Type 6 Shadow Team | 1740 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:336` |
| 6 | 130 | Type 6 Science Center | 2725 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:354` |
| 6 | 158 | Type 6 Databank | 1705 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:382` |
| 6 | 176 | Type 6 University | 2640 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:400` |
| 6 | 204 | Class 6 Data Brokerage | 1370 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:428` |
| 6 | 222 | Class 6 Research Lab | 2205 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:446` |
| 6 | 250 | Class 6 Security Center | 1950 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:474` |
| 6 | 268 | Class 6 Hall of Learning | 2990 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:492` |
| 6 | 296 | Mark VI Intel Center | 1510 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:520` |
| 6 | 314 | Mark VI Laboratory | 2390 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:538` |
| 7 | 113 | Type 7 Shadow Team | 2590 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:337` |
| 7 | 131 | Type 7 Science Center | 4070 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:355` |
| 7 | 159 | Type 7 Databank | 2520 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:383` |
| 7 | 177 | Type 7 University | 3920 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:401` |
| 7 | 205 | Class 7 Data Brokerage | 2110 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:429` |
| 7 | 223 | Class 7 Research Lab | 3390 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:447` |
| 7 | 251 | Class 7 Security Center | 2885 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:475` |
| 7 | 269 | Class 7 Hall of Learning | 4445 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:493` |
| 7 | 297 | Mark VII Intel Center | 2280 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:521` |
| 7 | 315 | Mark VII Laboratory | 3620 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:539` |
| 8 | 114 | Type 8 Shadow Team | 3760 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:338` |
| 8 | 132 | Type 8 Science Center | 5830 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:356` |
| 8 | 160 | Type 8 Databank | 3690 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:384` |
| 8 | 178 | Type 8 University | 5670 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:402` |
| 8 | 206 | Class 8 Data Brokerage | 3115 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:430` |
| 8 | 224 | Class 8 Research Lab | 4920 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:448` |
| 8 | 252 | Class 8 Security Center | 4115 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:476` |
| 8 | 270 | Class 8 Hall of Learning | 6270 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:494` |
| 8 | 298 | Mark VIII Intel Center | 3355 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:522` |
| 8 | 316 | Mark VIII Laboratory | 5245 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:540` |
| 9 | 115 | Type 9 Shadow Team | 5175 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:339` |
| 9 | 133 | Type 9 Science Center | 8050 | 0 | 10 | 0 | 30 | `ReferenceCatalogue.h:357` |
| 9 | 161 | Type 9 Databank | 5060 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:385` |
| 9 | 179 | Type 9 University | 7810 | 0 | 10 | 0 | 29 | `ReferenceCatalogue.h:403` |
| 9 | 207 | Class 9 Data Brokerage | 4385 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:431` |
| 9 | 225 | Class 9 Research Lab | 6935 | 0 | 10 | 0 | 27 | `ReferenceCatalogue.h:449` |
| 9 | 253 | Class 9 Security Center | 5640 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:477` |
| 9 | 271 | Class 9 Hall of Learning | 8640 | 0 | 10 | 0 | 23 | `ReferenceCatalogue.h:495` |
| 9 | 299 | Mark IX Intel Center | 4665 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:523` |
| 9 | 317 | Mark IX Laboratory | 7315 | 0 | 10 | 0 | 15 | `ReferenceCatalogue.h:541` |

---

## 3. Ship Roster & Six-Field Technology Requirements (125 Ships)

Vanilla BOTF ships require specific technology levels across all six primary fields: `[Biotech, Computers, Construction, Energy, Propulsion, Weapons]`.

| ID | Empire / Category | Ship Name | Class | Role | Tech Requirements [Bio, Comp, Con, Eng, Prop, Wep] | Cost | Upkeep | Hull | Shields | Attack | Range | Speed | Source Reference |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Cardassian | Strike Cruiser | Keldon | Cruiser | `[2, 3, 4, 3, 2, 4]` | 3570 | 73 | 200 | 240 | 760 | 0.0 | 1.0 | `ReferenceCatalogue.h:97` |
| 1 | Cardassian | Strike Cruiser II | Keldon | Cruiser | `[6, 7, 8, 7, 6, 8]` | 11570 | 73 | 240 | 320 | 2878 | 0.0 | 1.0 | `ReferenceCatalogue.h:98` |
| 2 | Cardassian | Colony Ship | Ranol | Command Cruiser/Battleship/Dreadnought | `[1, 1, 1, 1, 1, 1]` | 1210 | 23 | 90 | 50 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:99` |
| 3 | Cardassian | Colony Ship II | Ranol | Command Cruiser/Battleship/Dreadnought | `[5, 4, 5, 4, 3, 1]` | 3380 | 23 | 130 | 110 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:100` |
| 4 | Cardassian | Battleship | Galor | Strike Cruiser | `[5, 5, 6, 6, 5, 6]` | 9480 | 137 | 460 | 510 | 1592 | 0.0 | 1.0 | `ReferenceCatalogue.h:101` |
| 5 | Cardassian | Battleship II | Galor | Strike Cruiser | `[9, 9, 10, 10, 9, 10]` | 23610 | 137 | 500 | 590 | 4998 | 0.0 | 2.0 | `ReferenceCatalogue.h:102` |
| 6 | Cardassian | Outpost | Outpost  | Outpost | `[1, 1, 1, 1, 1, 1]` | 1550 | 12 | 210 | 270 | 855 | 0.0 | 0.0 | `ReferenceCatalogue.h:103` |
| 7 | Cardassian | Outpost II | Outpost  | Outpost | `[2, 3, 3, 3, 1, 3]` | 3280 | 12 | 230 | 310 | 2029 | 0.0 | 0.0 | `ReferenceCatalogue.h:104` |
| 8 | Cardassian | Destroyer | Netel | Scout | `[1, 1, 2, 1, 1, 2]` | 1180 | 59 | 100 | 140 | 147 | 1.0 | 1.0 | `ReferenceCatalogue.h:105` |
| 9 | Cardassian | Destroyer II | Netel | Scout | `[4, 5, 6, 5, 5, 6]` | 4730 | 59 | 140 | 220 | 990 | 1.0 | 2.0 | `ReferenceCatalogue.h:106` |
| 10 | Cardassian | Scout | Nerok | Colony Ship | `[1, 1, 1, 1, 1, 1]` | 400 | 26 | 30 | 80 | 44 | 2.0 | 1.0 | `ReferenceCatalogue.h:107` |
| 11 | Cardassian | Scout II | Nerok | Colony Ship | `[5, 5, 5, 4, 4, 4]` | 1770 | 26 | 70 | 140 | 327 | 2.0 | 2.0 | `ReferenceCatalogue.h:108` |
| 12 | Cardassian | Starbase | Starbase  | Starbase | `[5, 6, 6, 5, 4, 6]` | 15320 | 104 | 460 | 600 | 8842 | 0.0 | 0.0 | `ReferenceCatalogue.h:109` |
| 13 | Cardassian | Starbase II | Starbase  | Starbase | `[7, 8, 8, 8, 6, 8]` | 24150 | 104 | 480 | 660 | 13601 | 0.0 | 0.0 | `ReferenceCatalogue.h:110` |
| 14 | Cardassian | Troop Transport | Toran | Role 9 | `[1, 1, 1, 1, 1, 1]` | 910 | 26 | 70 | 80 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:111` |
| 15 | Cardassian | Troop Transport II | Toran | Role 9 | `[4, 4, 5, 4, 4, 1]` | 2450 | 26 | 110 | 140 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:112` |
| 16 | Cardassian | Cruiser | Neterok | Destroyer/Raider | `[1, 2, 3, 2, 2, 3]` | 2990 | 91 | 270 | 310 | 347 | 1.0 | 1.0 | `ReferenceCatalogue.h:113` |
| 17 | Cardassian | Cruiser II | Neterok | Destroyer/Raider | `[3, 4, 5, 4, 4, 5]` | 5180 | 91 | 290 | 350 | 839 | 1.0 | 1.0 | `ReferenceCatalogue.h:114` |
| 18 | Cardassian | Heavy Cruiser | Telok | Destroyer/Raider | `[5, 6, 7, 6, 6, 7]` | 9670 | 100 | 330 | 420 | 1903 | 1.0 | 2.0 | `ReferenceCatalogue.h:115` |
| 19 | Cardassian | Heavy Cruiser II | Telok | Destroyer/Raider | `[7, 8, 9, 8, 8, 9]` | 15570 | 100 | 350 | 460 | 3362 | 1.0 | 2.0 | `ReferenceCatalogue.h:116` |
| 20 | Federation | Strike Cruiser | Nebula | Cruiser | `[3, 4, 5, 5, 3, 5]` | 5400 | 73 | 131 | 390 | 1064 | 0.0 | 1.0 | `ReferenceCatalogue.h:117` |
| 21 | Federation | Strike Cruiser II | Nebula | Cruiser | `[5, 6, 8, 8, 6, 8]` | 11410 | 73 | 152 | 480 | 2517 | 0.0 | 1.0 | `ReferenceCatalogue.h:118` |
| 22 | Federation | Colony Ship | Edward | Command Cruiser/Battleship/Dreadnought | `[1, 1, 1, 1, 1, 1]` | 1210 | 23 | 55 | 70 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:119` |
| 23 | Federation | Colony Ship II | Edward | Command Cruiser/Battleship/Dreadnought | `[5, 4, 5, 5, 3, 1]` | 4680 | 23 | 83 | 190 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:120` |
| 24 | Federation | Command Cruiser | Galaxy | Strike Cruiser | `[3, 3, 4, 4, 3, 4]` | 5320 | 115 | 208 | 600 | 593 | 0.0 | 1.0 | `ReferenceCatalogue.h:121` |
| 25 | Federation | Command Cruiser II | Galaxy | Strike Cruiser | `[5, 5, 6, 6, 5, 6]` | 8520 | 115 | 222 | 660 | 1575 | 0.0 | 1.0 | `ReferenceCatalogue.h:122` |
| 26 | Federation | Dreadnought | Sovereign | Strike Cruiser | `[7, 7, 8, 8, 7, 8]` | 14620 | 134 | 260 | 840 | 2345 | 0.0 | 1.0 | `ReferenceCatalogue.h:123` |
| 27 | Federation | Dreadnought II | Sovereign | Strike Cruiser | `[9, 9, 10, 10, 9, 10]` | 21830 | 134 | 274 | 900 | 3918 | 0.0 | 2.0 | `ReferenceCatalogue.h:124` |
| 28 | Federation | Outpost | Outpost  | Outpost | `[1, 1, 1, 1, 1, 1]` | 1480 | 12 | 127 | 330 | 782 | 0.0 | 0.0 | `ReferenceCatalogue.h:125` |
| 29 | Federation | Outpost II | Outpost  | Outpost | `[2, 3, 3, 3, 1, 2]` | 2280 | 12 | 141 | 390 | 1196 | 0.0 | 0.0 | `ReferenceCatalogue.h:126` |
| 30 | Federation | Destroyer | Miranda | Scout | `[1, 1, 2, 2, 2, 1]` | 1410 | 59 | 62 | 220 | 70 | 1.0 | 1.0 | `ReferenceCatalogue.h:127` |
| 31 | Federation | Destroyer II | Miranda | Scout | `[2, 3, 4, 4, 4, 3]` | 2660 | 59 | 76 | 280 | 244 | 1.0 | 2.0 | `ReferenceCatalogue.h:128` |
| 32 | Federation | Heavy Destroyer | Constellation | Scout | `[4, 5, 6, 6, 6, 5]` | 5000 | 65 | 102 | 380 | 602 | 1.0 | 3.0 | `ReferenceCatalogue.h:129` |
| 33 | Federation | Heavy Destroyer II | Constellation | Scout | `[6, 7, 8, 8, 8, 7]` | 8140 | 65 | 116 | 440 | 1234 | 1.0 | 3.0 | `ReferenceCatalogue.h:130` |
| 34 | Federation | Heavy Escort | Defiant | Scout | `[8, 9, 9, 10, 10, 10]` | 18270 | 91 | 123 | 580 | 3513 | 1.0 | 4.0 | `ReferenceCatalogue.h:131` |
| 35 | Federation | Science Vessel | Oberth | Colony Ship | `[1, 1, 1, 1, 1, 1]` | 490 | 26 | 19 | 110 | 42 | 2.0 | 1.0 | `ReferenceCatalogue.h:132` |
| 36 | Federation | Science Vessel II | Oberth | Colony Ship | `[5, 5, 5, 5, 4, 3]` | 1990 | 26 | 47 | 230 | 180 | 2.0 | 2.0 | `ReferenceCatalogue.h:133` |
| 37 | Federation | Starbase | Starbase  | Starbase | `[5, 6, 6, 6, 4, 5]` | 11120 | 104 | 282 | 780 | 5978 | 0.0 | 0.0 | `ReferenceCatalogue.h:134` |
| 38 | Federation | Starbase II | Starbase  | Starbase | `[7, 8, 8, 8, 6, 7]` | 17580 | 104 | 296 | 840 | 9469 | 0.0 | 0.0 | `ReferenceCatalogue.h:135` |
| 39 | Federation | Troop Transport | London | Role 9 | `[1, 1, 1, 1, 1, 1]` | 1030 | 26 | 43 | 110 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:136` |
| 40 | Federation | Troop Transport II | London | Role 9 | `[4, 4, 5, 5, 4, 1]` | 3520 | 26 | 71 | 230 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:137` |
| 41 | Federation | Heavy Cruiser | Galaxy-X | Destroyer/Raider | `[5, 6, 7, 7, 6, 7]` | 9730 | 100 | 205 | 610 | 1555 | 1.0 | 2.0 | `ReferenceCatalogue.h:138` |
| 42 | Federation | Heavy Cruiser II | Galaxy-X | Destroyer/Raider | `[7, 8, 9, 9, 8, 9]` | 15090 | 100 | 219 | 670 | 2739 | 1.0 | 2.0 | `ReferenceCatalogue.h:139` |
| 43 | Federation | Light Cruiser | Ambassador | Destroyer/Raider | `[1, 2, 3, 3, 2, 3]` | 3100 | 85 | 153 | 410 | 296 | 1.0 | 1.0 | `ReferenceCatalogue.h:140` |
| 44 | Federation | Light Cruiser II | Ambassador | Destroyer/Raider | `[3, 4, 5, 5, 4, 5]` | 5250 | 85 | 167 | 470 | 704 | 1.0 | 1.0 | `ReferenceCatalogue.h:141` |
| 45 | Ferengi | Strike Cruiser | Ooron | Cruiser | `[2, 3, 3, 4, 3, 4]` | 4880 | 73 | 95 | 310 | 1128 | 0.0 | 1.0 | `ReferenceCatalogue.h:142` |
| 46 | Ferengi | Strike Cruiser II | Ooron | Cruiser | `[6, 7, 7, 8, 7, 8]` | 16590 | 73 | 115 | 410 | 4196 | 0.0 | 1.0 | `ReferenceCatalogue.h:143` |
| 47 | Ferengi | Colony Ship | Tomax | Command Cruiser/Battleship/Dreadnought | `[1, 1, 1, 1, 1, 1]` | 1010 | 23 | 45 | 60 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:144` |
| 48 | Ferengi | Colony Ship II | Tomax | Command Cruiser/Battleship/Dreadnought | `[5, 4, 4, 5, 4, 1]` | 4160 | 23 | 60 | 160 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:145` |
| 49 | Ferengi | Marauder | D'Kora | Strike Cruiser | `[6, 6, 6, 7, 7, 7]` | 14600 | 124 | 180 | 665 | 2891 | 0.0 | 1.0 | `ReferenceCatalogue.h:146` |
| 50 | Ferengi | Marauder II | D'Kora | Strike Cruiser | `[9, 9, 9, 10, 10, 10]` | 29510 | 124 | 195 | 740 | 6449 | 0.0 | 2.0 | `ReferenceCatalogue.h:147` |
| 51 | Ferengi | Outpost | Outpost  | Outpost | `[1, 1, 1, 1, 1, 1]` | 1700 | 12 | 105 | 300 | 1033 | 0.0 | 0.0 | `ReferenceCatalogue.h:148` |
| 52 | Ferengi | Outpost II | Outpost  | Outpost | `[2, 3, 3, 3, 2, 2]` | 2840 | 12 | 115 | 350 | 1685 | 0.0 | 0.0 | `ReferenceCatalogue.h:149` |
| 53 | Ferengi | Light Raider | Ngort | Scout | `[1, 1, 1, 2, 2, 1]` | 1280 | 54 | 35 | 190 | 96 | 1.0 | 1.0 | `ReferenceCatalogue.h:150` |
| 54 | Ferengi | Light Raider II | Ngort | Scout | `[2, 3, 3, 4, 4, 3]` | 2650 | 54 | 45 | 240 | 349 | 1.0 | 2.0 | `ReferenceCatalogue.h:151` |
| 55 | Ferengi | Raider | Tokorn | Scout | `[4, 5, 5, 6, 6, 5]` | 5420 | 59 | 55 | 325 | 921 | 1.0 | 3.0 | `ReferenceCatalogue.h:152` |
| 56 | Ferengi | Raider II | Tokorn | Scout | `[7, 8, 8, 9, 9, 8]` | 12530 | 59 | 70 | 400 | 2624 | 1.0 | 4.0 | `ReferenceCatalogue.h:153` |
| 57 | Ferengi | Explorer | Bronta | Colony Ship | `[1, 1, 1, 1, 1, 1]` | 460 | 26 | 15 | 95 | 58 | 2.0 | 1.0 | `ReferenceCatalogue.h:154` |
| 58 | Ferengi | Explorer II | Bronta | Colony Ship | `[5, 5, 4, 5, 5, 3]` | 2060 | 26 | 30 | 195 | 266 | 2.0 | 3.0 | `ReferenceCatalogue.h:155` |
| 59 | Ferengi | Starbase | Starbase  | Starbase | `[5, 6, 6, 6, 4, 5]` | 14400 | 104 | 230 | 700 | 8203 | 0.0 | 0.0 | `ReferenceCatalogue.h:156` |
| 60 | Ferengi | Starbase II | Starbase  | Starbase | `[7, 8, 8, 8, 7, 7]` | 23590 | 104 | 240 | 750 | 13125 | 0.0 | 0.0 | `ReferenceCatalogue.h:157` |
| 61 | Ferengi | Troop Transport | Glantor | Role 9 | `[1, 1, 1, 1, 1, 1]` | 880 | 26 | 35 | 95 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:158` |
| 62 | Ferengi | Troop Transport II | Glantor | Role 9 | `[4, 4, 5, 5, 5, 1]` | 3230 | 26 | 55 | 195 | 0 | 1.0 | 2.0 | `ReferenceCatalogue.h:159` |
| 63 | Ferengi | War Cruiser | Goront | Destroyer/Raider | `[1, 2, 2, 3, 3, 3]` | 3640 | 94 | 130 | 390 | 500 | 1.0 | 1.0 | `ReferenceCatalogue.h:160` |
| 64 | Ferengi | War Cruiser II | Goront | Destroyer/Raider | `[5, 6, 6, 7, 7, 7]` | 11490 | 94 | 150 | 490 | 2363 | 1.0 | 2.0 | `ReferenceCatalogue.h:161` |
| 65 | Klingon | Strike Cruiser | Negh'Var | Cruiser | `[4, 4, 4, 4, 4, 5]` | 5040 | 73 | 100 | 260 | 1189 | 0.0 | 1.0 | `ReferenceCatalogue.h:162` |
| 66 | Klingon | Strike Cruiser II | Negh'Var | Cruiser | `[6, 7, 7, 7, 7, 8]` | 12610 | 73 | 115 | 320 | 3180 | 0.0 | 1.0 | `ReferenceCatalogue.h:163` |
| 67 | Klingon | Armed Colony Ship | LI'wI' | Command Cruiser/Battleship/Dreadnought | `[1, 1, 1, 1, 1, 1]` | 1160 | 26 | 35 | 50 | 48 | 1.0 | 1.0 | `ReferenceCatalogue.h:164` |
| 68 | Klingon | Armed Colony Ship II | LI'wI' | Command Cruiser/Battleship/Dreadnought | `[5, 4, 4, 4, 4, 3]` | 4680 | 26 | 50 | 110 | 208 | 1.0 | 1.0 | `ReferenceCatalogue.h:165` |
| 69 | Klingon | Attack Cruiser | Vor'cha | Strike Cruiser | `[5, 5, 5, 6, 6, 6]` | 10230 | 124 | 175 | 480 | 2070 | 0.0 | 1.0 | `ReferenceCatalogue.h:166` |
| 70 | Klingon | Attack Cruiser II | Vor'cha | Strike Cruiser | `[9, 9, 9, 10, 10, 10]` | 26860 | 124 | 195 | 560 | 6102 | 0.0 | 2.0 | `ReferenceCatalogue.h:167` |
| 71 | Klingon | Outpost | Outpost  | Outpost | `[1, 1, 1, 1, 1, 1]` | 1650 | 12 | 105 | 270 | 1027 | 0.0 | 0.0 | `ReferenceCatalogue.h:168` |
| 72 | Klingon | Outpost II | Outpost  | Outpost | `[2, 3, 3, 3, 2, 3]` | 3710 | 12 | 115 | 310 | 2401 | 0.0 | 0.0 | `ReferenceCatalogue.h:169` |
| 73 | Klingon | Destroyer | May'Duj | Scout | `[1, 1, 1, 1, 2, 2]` | 1170 | 59 | 45 | 140 | 158 | 1.0 | 1.0 | `ReferenceCatalogue.h:170` |
| 74 | Klingon | Destroyer II | May'Duj | Scout | `[4, 5, 5, 5, 6, 6]` | 5120 | 59 | 65 | 220 | 1086 | 1.0 | 3.0 | `ReferenceCatalogue.h:171` |
| 75 | Klingon | Scout | B'rel | Colony Ship | `[1, 1, 1, 1, 1, 1]` | 390 | 26 | 15 | 80 | 48 | 2.0 | 1.0 | `ReferenceCatalogue.h:172` |
| 76 | Klingon | Scout II | B'rel | Colony Ship | `[5, 5, 4, 4, 5, 4]` | 1850 | 26 | 30 | 140 | 336 | 2.0 | 3.0 | `ReferenceCatalogue.h:173` |
| 77 | Klingon | Starbase | Starbase  | Starbase | `[5, 6, 6, 5, 5, 6]` | 18540 | 104 | 230 | 600 | 10849 | 0.0 | 0.0 | `ReferenceCatalogue.h:174` |
| 78 | Klingon | Starbase II | Starbase  | Starbase | `[7, 8, 8, 8, 7, 8]` | 29460 | 104 | 240 | 660 | 16616 | 0.0 | 0.0 | `ReferenceCatalogue.h:175` |
| 79 | Klingon | Assault Transport | CharghwI' | Role 9 | `[1, 1, 1, 1, 1, 1]` | 930 | 28 | 25 | 80 | 48 | 1.0 | 1.0 | `ReferenceCatalogue.h:176` |
| 80 | Klingon | Assault Transport II | CharghwI' | Role 9 | `[5, 4, 5, 4, 5, 3]` | 3450 | 28 | 45 | 140 | 208 | 1.0 | 2.0 | `ReferenceCatalogue.h:177` |
| 81 | Klingon | Battle Cruiser | K'T'Inga | Destroyer/Raider | `[1, 2, 2, 2, 3, 3]` | 3210 | 100 | 140 | 340 | 418 | 1.0 | 1.0 | `ReferenceCatalogue.h:178` |
| 82 | Klingon | Battle Cruiser II | K'T'Inga | Destroyer/Raider | `[3, 4, 4, 4, 5, 5]` | 5840 | 100 | 150 | 380 | 1014 | 1.0 | 2.0 | `ReferenceCatalogue.h:179` |
| 83 | Klingon | Heavy Cruiser | K'Vort | Destroyer/Raider | `[5, 6, 6, 6, 7, 7]` | 10000 | 100 | 160 | 420 | 2003 | 1.0 | 2.0 | `ReferenceCatalogue.h:180` |
| 84 | Klingon | Heavy Cruiser II | K'Vort | Destroyer/Raider | `[7, 8, 8, 8, 9, 9]` | 16640 | 100 | 170 | 460 | 3625 | 1.0 | 3.0 | `ReferenceCatalogue.h:181` |
| 85 | Romulan | Strike Cruiser | R'tan | Cruiser | `[2, 4, 3, 4, 2, 4]` | 4000 | 73 | 95 | 260 | 975 | 0.0 | 1.0 | `ReferenceCatalogue.h:182` |
| 86 | Romulan | Strike Cruiser II | R'tan | Cruiser | `[6, 8, 7, 8, 6, 8]` | 14100 | 73 | 115 | 340 | 3784 | 0.0 | 1.0 | `ReferenceCatalogue.h:183` |
| 87 | Romulan | Colony Ship | D'retex | Command Cruiser/Battleship/Dreadnought | `[1, 1, 1, 1, 1, 1]` | 860 | 23 | 45 | 50 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:184` |
| 88 | Romulan | Colony Ship II | D'retex | Command Cruiser/Battleship/Dreadnought | `[5, 5, 4, 5, 3, 1]` | 3240 | 23 | 60 | 130 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:185` |
| 89 | Romulan | Warbird | D'Deridex | Strike Cruiser | `[5, 6, 5, 6, 5, 6]` | 9890 | 128 | 175 | 540 | 2068 | 0.0 | 1.0 | `ReferenceCatalogue.h:186` |
| 90 | Romulan | Warbird II | D'Deridex | Strike Cruiser | `[9, 10, 9, 10, 9, 10]` | 26730 | 128 | 195 | 620 | 6373 | 0.0 | 2.0 | `ReferenceCatalogue.h:187` |
| 91 | Romulan | Outpost | Outpost  | Outpost | `[1, 1, 1, 1, 1, 1]` | 1430 | 12 | 105 | 270 | 871 | 0.0 | 0.0 | `ReferenceCatalogue.h:188` |
| 92 | Romulan | Outpost II | Outpost  | Outpost | `[2, 3, 3, 3, 1, 2]` | 2270 | 12 | 115 | 310 | 1387 | 0.0 | 0.0 | `ReferenceCatalogue.h:189` |
| 93 | Romulan | Destroyer | D'ridren | Scout | `[1, 2, 1, 2, 2, 1]` | 1070 | 59 | 45 | 160 | 84 | 1.0 | 1.0 | `ReferenceCatalogue.h:190` |
| 94 | Romulan | Destroyer II | D'ridren | Scout | `[4, 6, 5, 6, 6, 5]` | 4200 | 59 | 65 | 240 | 805 | 1.0 | 3.0 | `ReferenceCatalogue.h:191` |
| 95 | Romulan | Scout | D'renet | Colony Ship | `[1, 1, 1, 1, 1, 1]` | 370 | 26 | 15 | 80 | 48 | 2.0 | 1.0 | `ReferenceCatalogue.h:192` |
| 96 | Romulan | Scout II | D'renet | Colony Ship | `[5, 5, 4, 5, 4, 3]` | 1550 | 26 | 30 | 160 | 227 | 2.0 | 2.0 | `ReferenceCatalogue.h:193` |
| 97 | Romulan | Starbase | Starbase | Starbase | `[5, 6, 6, 6, 4, 5]` | 12680 | 104 | 230 | 620 | 7490 | 0.0 | 0.0 | `ReferenceCatalogue.h:194` |
| 98 | Romulan | Starbase II | Starbase | Starbase | `[7, 8, 8, 8, 6, 7]` | 20970 | 104 | 240 | 660 | 12157 | 0.0 | 0.0 | `ReferenceCatalogue.h:195` |
| 99 | Romulan | Troop Transport | R'daran | Role 9 | `[1, 1, 1, 1, 1, 1]` | 720 | 26 | 35 | 80 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:196` |
| 100 | Romulan | Troop Transport II | R'daran | Role 9 | `[4, 5, 5, 5, 4, 1]` | 2470 | 26 | 55 | 160 | 0 | 1.0 | 1.0 | `ReferenceCatalogue.h:197` |
| 101 | Romulan | Battle Cruiser | D'dredar | Destroyer/Raider | `[5, 7, 6, 7, 6, 7]` | 9930 | 100 | 160 | 440 | 2162 | 1.0 | 2.0 | `ReferenceCatalogue.h:198` |
| 102 | Romulan | Battle Cruiser II | D'dredar | Destroyer/Raider | `[7, 9, 8, 9, 8, 9]` | 16570 | 100 | 170 | 480 | 3884 | 1.0 | 2.0 | `ReferenceCatalogue.h:199` |
| 103 | Romulan | Cruiser | R'derex | Destroyer/Raider | `[1, 3, 2, 3, 2, 3]` | 2830 | 91 | 130 | 330 | 380 | 1.0 | 1.0 | `ReferenceCatalogue.h:200` |
| 104 | Romulan | Cruiser II | R'derex | Destroyer/Raider | `[3, 5, 4, 5, 4, 5]` | 5120 | 91 | 140 | 370 | 941 | 1.0 | 1.0 | `ReferenceCatalogue.h:201` |
| 105 | Minor/Threat (5) | Acamarian Raider | Raider | Scout | `[3, 3, 3, 3, 3, 3]` | 1940 | 59 | 55 | 180 | 261 | 1.0 | 2.0 | `ReferenceCatalogue.h:202` |
| 106 | Minor/Threat (10) | Bajoran fighter | Fighter | Scout | `[3, 3, 3, 3, 3, 3]` | 1720 | 57 | 55 | 180 | 257 | 1.0 | 2.0 | `ReferenceCatalogue.h:203` |
| 107 | Minor/Threat (19) | Ktarian Raider | Raider | Scout | `[3, 3, 3, 3, 3, 3]` | 1690 | 57 | 55 | 180 | 244 | 1.0 | 2.0 | `ReferenceCatalogue.h:204` |
| 108 | Minor/Threat (24) | Pakled Transport | Transport | Destroyer/Raider | `[3, 3, 3, 3, 3, 3]` | 1520 | 46 | 55 | 180 | 244 | 1.0 | 1.0 | `ReferenceCatalogue.h:205` |
| 109 | Minor/Threat (26) | Sheliak Raider | Raider | Scout | `[3, 3, 3, 3, 3, 3]` | 2030 | 71 | 75 | 210 | 244 | 1.0 | 2.0 | `ReferenceCatalogue.h:206` |
| 110 | Minor/Threat (28) | Talarian Warship | Warship | Destroyer/Raider | `[3, 3, 3, 3, 3, 3]` | 3230 | 91 | 135 | 410 | 350 | 1.0 | 1.0 | `ReferenceCatalogue.h:207` |
| 111 | Minor/Threat (29) | Tamarian Defender | Warship | Destroyer/Raider | `[3, 3, 5, 5, 3, 3]` | 3290 | 94 | 155 | 400 | 300 | 1.0 | 1.0 | `ReferenceCatalogue.h:208` |
| 112 | Minor/Threat (30) | Trill Warship | Warship | Destroyer/Raider | `[3, 3, 3, 3, 3, 3]` | 2740 | 91 | 135 | 330 | 310 | 1.0 | 1.0 | `ReferenceCatalogue.h:209` |
| 113 | Minor/Threat (32) | Vulcan ship | Research Vessel | Scout | `[4, 4, 4, 4, 4, 4]` | 3570 | 91 | 140 | 350 | 483 | 1.0 | 1.0 | `ReferenceCatalogue.h:210` |
| 114 | Minor/Threat (33) | Yridian Scout | Scout | Colony Ship | `[3, 3, 3, 3, 3, 3]` | 1120 | 26 | 25 | 120 | 171 | 2.0 | 2.0 | `ReferenceCatalogue.h:211` |
| 115 | Minor/Threat (36) | Borg Cube | Cube | Troop Transport | `[7, 7, 7, 7, 7, 7]` | 43674 | 638 | 785 | 6100 | 86699 | 0.0 | 1.0 | `ReferenceCatalogue.h:212` |
| 116 | Minor/Threat (36) | Gaseous Lifeform | Calamarain | Troop Transport | `[7, 7, 7, 7, 7, 7]` | 37020 | 124 | 185 | 1100 | 8839 | 0.0 | 1.0 | `ReferenceCatalogue.h:213` |
| 117 | Minor/Threat (36) | Dreadnought | Chodak | Troop Transport | `[5, 5, 5, 5, 5, 5]` | 21030 | 134 | 195 | 1000 | 4826 | 0.0 | 1.0 | `ReferenceCatalogue.h:214` |
| 118 | Minor/Threat (36) | Unknown Lifeform | Crystal Entity | Troop Transport | `[6, 6, 6, 6, 6, 6]` | 20390 | 356 | 1030 | 300 | 4545 | 0.0 | 1.0 | `ReferenceCatalogue.h:215` |
| 119 | Minor/Threat (36) | Combat Drone | Echo Papa 607 | Troop Transport | `[6, 6, 6, 6, 6, 6]` | 23290 | 76 | 80 | 550 | 6268 | 0.0 | 0.0 | `ReferenceCatalogue.h:216` |
| 120 | Minor/Threat (36) | Alien Entity | Edo Guardian | Troop Transport | `[7, 7, 7, 7, 7, 11]` | 4174 | 797 | 1035 | 6350 | 77448 | 0.0 | 0.0 | `ReferenceCatalogue.h:217` |
| 121 | Minor/Threat (36) | Sentient Starship | Gomtuu | Troop Transport | `[6, 6, 6, 6, 6, 6]` | 8500 | 517 | 1530 | 300 | 0 | 0.0 | 1.0 | `ReferenceCatalogue.h:218` |
| 122 | Minor/Threat (36) | Husnock vessel | 0 | Troop Transport | `[4, 4, 4, 4, 4, 5]` | 13580 | 91 | 140 | 650 | 3389 | 1.0 | 1.0 | `ReferenceCatalogue.h:219` |
| 123 | Minor/Threat (36) | Orbital Battery | 0 | Troop Transport | `[0, 0, 0, 0, 0, 0]` | 1680 | 85 | 100 | 400 | 133 | 0.0 | 1.0 | `ReferenceCatalogue.h:220` |
| 124 | Minor/Threat (36) | Tarellian vessel | 0 | Troop Transport | `[3, 3, 2, 3, 3, 2]` | 4940 | 91 | 130 | 600 | 741 | 1.0 | 1.0 | `ReferenceCatalogue.h:221` |

---

## 4. Minor Race Wonder Structures (30 Races)

Each minor race in vanilla BOTF provides a unique wonder structure when integrated into an empire. Sourced from `ReferenceCatalogue.h:284-309` and `MinorRaces.h:43-150`.

| Race Name | Structure Name | Tech Unlock | Industry Cost | Energy Cost | In-Game Effect | Catalogue Ref |
|---|---|---|---|---|---|---|
| *(Building 54)* | Starfleet Shipyard | Construction 0 | 500 | 25 | A Starfleet yard where ships are built, refitted and sent back out. Required for construction and a source of fleet supply. | `ReferenceCatalogue.h:278` |
| *(Building 55)* | Military Shipyard | Construction 0 | 500 | 25 | Klingon war docks where new ships are forged and battle damage is mended. Required for construction and a source of fleet supply. | `ReferenceCatalogue.h:279` |
| *(Building 56)* | Acamarian Clan Hall | Sociology 1 | 3000 | 50 | Neutral ground where Acamarian clans settle disputes with words instead of blood. Raises credits and morale; requires Acamarian residents. | `ReferenceCatalogue.h:280` |
| *(Building 57)* | Andorian War College | Sociology 1 | 5000 | 100 | Andorian officers teach the hard-won art of close combat. New ships gain experience; requires Andorian residents. | `ReferenceCatalogue.h:281` |
| *(Building 58)* | Super Soldier Academy | Weapons 1 | 3000 | 60 | Angosian veterans pass on the conditioning that made them the most dangerous soldiers of their war. Strengthens ground combat; requires Angosian residents. | `ReferenceCatalogue.h:282` |
| *(Building 59)* | Mustering Base | Sociology 1 | 400 | 40 | An Antican recruiting depot that never lacks volunteers while the Selay live. Strengthens ground defense; requires Antican residents. | `ReferenceCatalogue.h:283` |
| *(Building 60)* | Harvesting Complex | Biotech 1 | 2000 | 50 | Deep-water trawlers and processing ships working the Antedean seas. Raises food output; requires Antedean residents. | `ReferenceCatalogue.h:284` |
| *(Building 61)* | Bajoran Jalanda Forum | Sociology 1 | 10000 | 50 | A public amphitheater where vedeks and citizens debate the issues of the day. Guards against unrest; requires Bajoran residents. | `ReferenceCatalogue.h:285` |
| *(Building 62)* | Architectural Center | Construction 1 | 2500 | 100 | Bandi designers, famous for their hospitality architecture, share their craft with the empire. Boosts Construction research; requires Bandi residents. | `ReferenceCatalogue.h:286` |
| *(Building 63)* | Industrial Center | Construction 1 | 5000 | 100 | Benzite process engineers publish a new procedure manual every week, and are usually right. Raises the system's industry; requires Benzite residents. | `ReferenceCatalogue.h:287` |
| *(Building 64)* | Counseling Academy | Sociology 1 | 3000 | 50 | Betazoid telepaths train counselors who can feel resentment long before it turns to sabotage. Strengthens internal security; requires Betazoid residents. | `ReferenceCatalogue.h:288` |
| *(Building 65)* | Cosmetology Center | Sociology 1 | 2000 | 50 | Bolian beauty clinics with a discreet back room, where agents leave looking like someone else entirely. Improves espionage success; requires Bolian residents. | `ReferenceCatalogue.h:289` |
| *(Building 66)* | Planetary Computer | Computers 1 | 2500 | 150 | The central computer every Bynar pair is linked to, now working the empire's research problems alongside its own. Boosts Computers research; requires Bynar residents. | `ReferenceCatalogue.h:290` |
| *(Building 67)* | Research Think Tank | Sociology 1 | 3000 | 200 | A Caldonian institute where scholars pursue questions for their own sake, and find useful answers anyway. Raises empire research; requires Caldonian residents. | `ReferenceCatalogue.h:291` |
| *(Building 68)* | Gladiatorial Arena | Weapons 1 | 2500 | 50 | Chalnoth fight here with the newest weapons, and the survivors report which ones worked. Boosts Weapons research; requires Chalnoth residents. | `ReferenceCatalogue.h:292` |
| *(Building 69)* | Palace of Edo | Sociology 1 | 7000 | 70 | Gardens, games and festivities open to every visitor who keeps the rules. Raises morale; requires Edo residents. | `ReferenceCatalogue.h:293` |
| *(Building 70)* | Ktarian Game Studio | Computers 1 | 3500 | 100 | Designers of entertainment so compelling that enemy crews stop working to play it. Improves sabotage; requires Ktarian residents. | `ReferenceCatalogue.h:294` |
| *(Building 71)* | Kinetics Laboratory | Propulsion 1 | 3000 | 150 | Malcorian warp-field theorists, freed from secrecy, test radical engine concepts. Boosts Propulsion research; requires Malcorian residents. | `ReferenceCatalogue.h:295` |
| *(Building 72)* | Mintakan Farm | Biotech 1 | 2000 | 0 | Terraced village farms tended with patient Mintakan care. Raises food output; requires Mintakan residents. | `ReferenceCatalogue.h:296` |
| *(Building 73)* | Monument of Surrender | Sociology 1 | 5000 | 0 | A quiet memorial listing every conqueror the Mizarians have outlived. Steadies morale; requires Mizarian residents. | `ReferenceCatalogue.h:297` |
| *(Building 74)* | Recruitment Center | Sociology 1 | 2000 | 40 | A Nausicaan hiring hall where mercenaries sign on for garrison duty across the empire. Strengthens ground combat; requires Nausicaan residents. | `ReferenceCatalogue.h:298` |
| *(Building 75)* | Collection Facility | Propulsion 1 | 1500 | 150 | A Pakled warehouse of salvaged, traded and borrowed technology, some of which actually works. Raises research; requires Pakled residents. | `ReferenceCatalogue.h:299` |
| *(Building 76)* | Selay Mustering Base | Sociology 1 | 400 | 40 | A Selay recruiting depot that never lacks volunteers while the Anticans live. Strengthens ground defense; requires Selay residents. | `ReferenceCatalogue.h:300` |
| *(Building 77)* | Bioengineering Center | Biotech 1 | 3500 | 200 | Sheliak laboratories that reshape living systems with legal precision. Boosts Biotech research; requires Sheliak residents. | `ReferenceCatalogue.h:301` |
| *(Building 78)* | Physics Institute | Energy 1 | 2000 | 200 | Takaran physicists study the forces that act on matter, a subject close to their own unusual biology. Boosts Energy research; requires Takaran residents. | `ReferenceCatalogue.h:302` |
| *(Building 79)* | Defense Network | Energy 1 | 3000 | 150 | Layered Talarian bunkers, minefields and gun pits built to make any landing a slaughter. Greatly strengthens ground defense; requires Talarian residents. | `ReferenceCatalogue.h:303` |
| *(Building 80)* | Mythology Library | Sociology 1 | 1500 | 50 | The Tamarian store of legends, read at last as the technical record it partly is. A large research boost; requires Tamarian residents. | `ReferenceCatalogue.h:304` |
| *(Building 81)* | Research Committee | Sociology 1 | 5000 | 130 | A Trill council that reviews research with the memory of many past lifetimes. Raises empire research; requires Trill residents. | `ReferenceCatalogue.h:305` |
| *(Building 82)* | Psychohistorical Archive | Sociology 1 | 2000 | 80 | Ullian telepathic interviews, catalogued as a map of how other peoples think. Aids intelligence work; requires Ullian residents. | `ReferenceCatalogue.h:306` |
| *(Building 83)* | Science Academy | Sociology 1 | 7500 | 170 | Vulcan's ancient center of learning, where every hypothesis must survive logic. Raises empire research; requires Vulcan residents. | `ReferenceCatalogue.h:307` |
| *(Building 84)* | Intelligence Service | Sociology 1 | 2500 | 50 | Yridian data dealers who now sell mostly to one customer: the empire. Improves espionage and security; requires Yridian residents. | `ReferenceCatalogue.h:308` |
| *(Building 85)* | Military Academy | Sociology 1 | 8000 | 200 | Zakdorn war-gamers run crews through simulated campaigns until nothing surprises them. New ships enter service with considerable experience; requires Zakdorn residents. | `ReferenceCatalogue.h:309` |

---

## 5. Resolution of Victory Thresholds (Gap Closure for §1.12)

### 5.1 Domination Victory

> **Odo check, 8 Nov 2026:** the vanilla victory numbers in this section (turn 60, 0.67 allied multiplier) are secondhand. They come from a community post summarised in research/BOTF-RESEARCH.md:167, not from disassembly or the core's tables. They also conflict with the current core, which uses turn >= 20 and counts systems only (Victory.cpp:232-242). This is unresolved; WS01 decides which rule to follow.
- **The Conflict:** `BOTF-FEATURE-REFERENCE.md` §1.12 noted a conflict between sources: Wikipedia reported 60% population and systems solo, 75% allied; Armada Fleet Command reported 70% solo / 50% allied.
- **Executable Evidence:** Reverse engineering of retail `stbof.exe` (`Sub_4529F0`, AFC Topic 1331; documented in `research/BOTF-RESEARCH.md` lines 165-170):
  1. The victory check does not run during early turns; it first evaluates on **Turn 60**.
  2. **Solo Domination:** Requires the player empire to control strictly greater than **60% of total galaxy population** AND strictly greater than **60% of total star systems**.
  3. **Allied Domination:** The assembly logic applies an alliance threshold multiplier of **0.67** (two-thirds, or 66.7%), contrary to the printed manual's claim of 75%.
  4. **Vanilla Bug:** A community report (secondhand) states an engine bug where the retail 1.0.2 executable checked allied population against the 66.7% threshold but omitted the corresponding allied star system count check.
- **Remaster Implementation:**
  - Conquest: `Simulation.cpp:4995-5005` — `surviving.size() == 1` (last surviving empire controls all colonies).
  - Solo Domination: `Simulation.cpp:5001-5004` & `Victory.cpp:232-242` — Evaluated on `turn >= 20`; requires `controlled_colonies * 5 >= total_systems * 3` (**60% of galaxy star systems**).

### 5.2 Vendetta Victory
In Vendetta mode, each major empire must completely eliminate its two canonical racial rivals before being eliminated itself:

| Major Empire | Canonical Rival 1 | Canonical Rival 2 | Historical / Canon Basis |
|---|---|---|---|
| **United Federation of Planets** | Romulan Star Empire | Cardassian Union | Neutral Zone conflicts & Border Wars |
| **Klingon Empire** | Cardassian Union | Ferengi Alliance | DS9 Betreka Nebula skirmishes & territorial friction |
| **Romulan Star Empire** | United Federation of Planets | Ferengi Alliance | Cold war rivalry & commercial espionage |
| **Ferengi Alliance** | Romulan Star Empire | Klingon Empire | Trade embargoes & honor vs profit ideological conflict |
| **Cardassian Union** | United Federation of Planets | Klingon Empire | Bajor/Cardassia border wars & Klingon invasion of Cardassia |

### 5.3 Remaster-Exclusive Alternative Victories
- **Diplomatic Victory:** `Victory.cpp:97-120` (`diplomaticVictoryClaimed`):
  - Pacing guard: Available from **Turn 150** onward.
  - Minimum partners: At least **2 allies/member minors** (`DiplomaticVictoryMinimumPartners = 2`).
  - Supermajority: Alliance bloc population must comprise at least **two-thirds (66.7%) of total galactic population** (`blocPopulation * 3 >= galaxyPopulation * 2`).
  - Leadership predicate: Claimant must lead the bloc by holding more population than any allied partner (`leadsBloc`).
- **Scientific Victory:** `Victory.cpp:160-250` (`scientificVictoryThreshold` & `megaprojectBlocker`):
  - Tech threshold: All six research fields must reach level `min(10, eraBaseTechLevel + 3)`.
  - Megaproject construction: Pacing guard Turn 50; credit cost `(Reference ? 500 : 150) * startingEra`; industry cost `4000 + 1000 * (startingEra - 1)`. Progress rate: `30 + 5 * ConstructionLevel` work per turn.
