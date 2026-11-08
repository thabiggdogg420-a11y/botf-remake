# Star Trek: Birth of the Federation (1999) - Feature Reference

Research only, compiled 8 Nov 2026 by Barclay. Private; not for distribution.
Every claim carries a source tag. Where a source was blocked or silent, the gap is marked **[GAP]**. Nothing here was verified by running the game.

Source key:
- AFC-OV = https://www.armadafleetcommand.com/botf/game-overview
- AFC-STR = https://www.armadafleetcommand.com/botf/information-on-structures
- AFC-MIN = https://www.armadafleetcommand.com/botf/minor-races-structures
- AFC-GAL = https://www.armadafleetcommand.com/botf/galaxy-size-and-how-many-minor-races
- AFC-MAP = https://www.armadafleetcommand.com/botf/large-map-generation-chart-and-tutorial-for-ue
- AFC-SHIP = https://www.armadafleetcommand.com/botf/starships
- AFC-CAL = https://www.armadafleetcommand.com/botf/calladices-faq-birth-of-the-federation-a-strategy-guide (Calladice's FAQ; read first 100k of 195k chars)
- AFC-XAV = https://www.armadafleetcommand.com/botf/xavori-strategy-guide
- AFC-BSG = https://www.armadafleetcommand.com/botf/basic-strategy-guide
- WIKI = https://en.wikipedia.org/wiki/Star_Trek:_Birth_of_the_Federation
- TREKCORE = https://gaming.trekcore.com/birthofthefederation/faq.html
- LPA = https://lparchive.org/Star-Trek-Birth-of-the-Federation

Note on conflicts: sources disagree on victory thresholds (see section 1.12). The vanilla number is unsettled; test against the real game before copying it.

---

## 1. Vanilla BOTF by system

### 1.1 Galaxy generation

| Feature | Detail | Source |
|---|---|---|
| Sizes | Small / medium / large (Wikipedia says "small, medium and huge"). Modded/UE chart examples: 30x22, 40x30, 52x39 cells. | WIKI, AFC-MAP |
| Shapes | Irregular, spiral, ring (ring = spiral with inner and middle arc values close). Vanilla spiral arc values 1.5 / 1.0 / 0.5. | AFC-OV, AFC-MAP |
| Minor-race density | None / Few / Some / Many. Small: 0/4/4/6. Medium: 0/7/12/17. Large: 0/10/16/24. Hard max 29 minors. | AFC-GAL, AFC-MAP |
| Tech level at start | Selectable; "advanced" start puts all six research fields at 8. | AFC-OV, AFC-CAL |
| Difficulty | Selectable; top level is "impossible". | AFC-OV |
| Random events | On/off at setup. A later patch added a Borg-only disable. | AFC-OV, LPA |
| Start | Three systems: home plus two advanced colonies. | AFC-OV |
| Map features | Stars of different types (red giants can nova), anomalies/phenomena, wormholes, nebulae shown as icons on a 2D map. Star/anomaly ratio is editable. | WIKI, AFC-MAP, TREKCORE |
| Hard limits | Above 52x39 the engine becomes unstable and may crash. | AFC-MAP |
| Starting "eras" | **[GAP]** No source found for era selection beyond the tech-level setting. |  |

### 1.2 Systems and planets

| Feature | Detail | Source |
|---|---|---|
| Unit of control | The whole star system (population is pooled per system, in millions). | TREKCORE, AFC-CAL |
| Population jobs | Five work areas: food, industry, energy, intelligence, research. Unfed or unemployed population is lost ("dead people = no cash"). | TREKCORE, AFC-CAL |
| Planet classes | Terraform until all planets are class M. Good targets: large systems, arctic, barren, oceanic, terran, dilithium; small volcanic and jungle are low priority. | TREKCORE |
| Terraforming | Done by colony ships; raises max population. | TREKCORE, AFC-BSG |
| Credits | Earned from industry and trade; pays for structures, ships, upkeep, gifts and bribes. One purchase per system per turn. | AFC-CAL, WIKI |
| Dilithium | Refineries cap how many ships can be under construction (roughly one per refinery); dilithium systems also boost shipyard output (Klingons). | AFC-CAL, AFC-XAV, WIKI |
| Deuterium / raw materials | In vanilla the raw-materials idea is folded into "industry". **[GAP]** Confirm; deuterium appears only in the Supremacy/Rise of the UFP designs. | Supremacy: spacesector link below |
| Morale tiers | Fanatic, Loyal, Pleased, Content, Apathetic, Disgruntled, Defiant, Rebellious. | AFC-CAL |
| Morale levers | Gain colonies/members, morale structures, emergency programs, winning battles. | TREKCORE, AFC-CAL |
| Emergency programs | Cardassian Inquisition, Romulan Tribunal, Klingon Police State, Federation Martial Law, Ferengi Festival of Fun. | TREKCORE |
| Food / energy | Structures need outside energy to run; unpowered structures stop. | AFC-STR, AFC-CAL |

Structures (all sourced from AFC-STR unless noted):

| Category | Cardassian | Federation | Ferengi | Klingon | Romulan |
|---|---|---|---|---|---|
| Defense (intel) | Shadow teams | Databanks | Data brokerages | Security centers | Intel centers |
| Development (industry) | Assembly yards | Mass replicators | Machining guilds | Foundries | Fabricators |
| Energy | Fusion plants | Plasma reactors | Power exchanges | Matter furnaces | Gravitic turbines |
| Food | Hydroponics farms | Automated farms | Grower's markets | Hunting grounds | Organics plants |
| Research | Science centers | Universities | Research labs | Halls of learning | Laboratories |

Special structures (no population needed, need energy): Dilithium Refinery, Orbital Battery, Scanner, Shield Generator, Shipyard (AFC-STR). Buildable by all: Aquaculture Center, Charge Collectors, Communications Grid, Listening Post, Subspace Scanner, Wind Turbines (AFC-STR).
Extra defence: bunker networks (ground defence). Scanner tiers: subspace +2, listening post +3, isolinear +4 (Cardassian covert array +6) (AFC-CAL).
Sample costs: Trade Center 500cr/+50% credits; Federation Replicator 500cr/+50 food; Genesis Research Lab 2,500cr/+100 biotech (AFC-CAL).
Wonders: each empire has five race-specific structures (e.g. Utopia Planitia Shipyard, Romulan Astrophysics Academy) (AFC-STR). The minor-race structures below act as the "wonders".

### 1.3 Research

| Feature | Detail | Source |
|---|---|---|
| Fields | Six: Biotech, Energy, Computers, Propulsion, Construction, Weapons (named in the Supremacy design article; AFC-CAL confirms "six research fields"). | AFC-CAL, https://www.spacesector.com/blog/2010/01/star-trek-supremacy-open-source-free-game-in-development/ |
| Levels | Advanced starts at 8 per field; top structures need 9; many ships need 10 in one or more fields. Level 1-10 in total. | AFC-CAL |
| Unlocks | New ship classes, industry/espionage/food structures. Ships and ships under construction auto-upgrade between turns once requirements are met (extra cost for in-progress ones). | TREKCORE, AFC-SHIP |
| Speed-ups | Population-staffed labs, energy-powered stimulators, empire structures, minor-race buildings (Vulcan +35%, Trill +30%, Caldonian +25%, Bandi/Bynars/Chalnoth/Malconians/Sheliak/Takarans +100% in one field). | AFC-CAL, AFC-MIN |
| Empire rank | Federation strongest, then Romulan, Klingon, Cardassian, Ferengi weakest. | AFC-CAL |
| Tree per level | **[GAP]** Per-level unlock lists not found in readable sources. The manual (manualmachine.com) returned 403. |  |

### 1.4 Ship design and classes

Vanilla has no ship designer; every ship is a fixed class that upgrades by tech level (WIKI, AFC-OV).

| Role | What it does | Source |
|---|---|---|
| Colony | Terraforms; medium range, unarmed, needs escort | AFC-SHIP |
| Command | Leads fleet, enables targeting individual enemy ships; short range | AFC-SHIP |
| Raider | Fast medium-range escort (destroyers count here) | AFC-SHIP |
| Scout | Long range; reveals enemy ship stats in tactical | AFC-SHIP |
| Strike cruiser | Long-range torpedoes, one-hit kills, bombardment; short range | AFC-SHIP |
| Troop transport | Carries troops, builds outposts and starbases | AFC-SHIP |
| Warship | Best weapons/armour/shields; heavy destroyers and cruisers | AFC-SHIP |

| Empire | Colony | Raider | Scout | Transport | Warship | Source |
|---|---|---|---|---|---|---|
| Federation | Edward | Miranda | Oberth | London | Ambassador, Constellation, Galaxy-X | AFC-SHIP |
| Klingon | LI'wI | May'Duj | B'rel | ChorghwI' | K'Vort BoP, K'Tinga | AFC-SHIP |
| Romulan | D'retex | D'ridren | D'renet | D'retex | D'dredar, R'drex | AFC-SHIP |
| Cardassian | Ranol | Netel | Nerok | Ranol | Neterok, Telok | AFC-SHIP |
| Ferengi | Tomax | Ngort, Tokorn | Bronta | Tomax | Goront | AFC-SHIP |

Strategy-guide names (Dreadnought 2, Defiant heavy escort, Warbird 2, Marauder 2, etc.) show the upgraded line (AFC-CAL). Cloaking: Romulan non-colony/non-transport classes and the Klingon Bird of Prey (AFC-SHIP). Crew ranks: Green, Regular, Veteran, Elite, Legendary (only Klingons can train to Legendary) (AFC-SHIP). Training: Starfleet Academy 50/turn, Klingon Tactical College 45, Cardassian Central Command 40, Romulan Naval Academy 35; Ferengi none (AFC-CAL). Ship upkeep is per turn (AFC-CAL).

### 1.5 Fleet commands (strategic)

| Order | Notes | Source |
|---|---|---|
| Move / Engage / Evade / Avoid | Colony ships set to avoid; scouts to evade | AFC-CAL, AFC-XAV |
| Intercept | Fast attack ships auto-intercept intruders within range (4 sectors for Defiants) | AFC-CAL, AFC-BSG |
| Raid | Used to plunder defeated systems for income (Ferengi) | AFC-XAV |
| Terraform / Build outpost-starbase / Invade | Colony and transport functions | AFC-SHIP, AFC-BSG |
| Task forces | Ships grouped into task forces; keep cloaked and uncloaked separate | AFC-XAV |
| Added in Rise of the UFP | Defend, Idle, Redeploy, general mission order | https://raw.githubusercontent.com/BotF2/BotF2/master/_ChangeLog_for_Rise_Of_The_UFP.txt |
| Full order list | **[GAP]** Complete list (blockade, etc.) not confirmed from any source. |  |

### 1.6 Tactical combat

| Feature | Detail | Source |
|---|---|---|
| Model | Turn-based; orders given to groups or individual ships, then both sides execute simultaneously (~15 s per tactical turn) | WIKI, AFC-CAL |
| Group orders | Assault (long range), Charge (close), Harry/Circle, Strafe/Flyby, Evade, Ram, Retreat, Open hailing | AFC-CAL |
| Engagement choice | Fight, Open hailing frequencies, Auto-resolve | AFC-CAL, WIKI |
| Replay | Battles can be recorded and replayed from several camera angles | WIKI |
| Presentation | Falcon 4.0-based 3D engine | WIKI |
| Cloak | Romulan/Klingon cloaked fleets get a free first strike | LPA, AFC-CAL |
| Planetary assault | Defences: fleet, orbital batteries (50 energy), shield generators, bunker networks, minor-race structures. Roughly one transport per 100 ground defence; two strong warships per battery | AFC-CAL |
| Outcome | After conquest: liberate, subjugate, or exterminate and recolonise | AFC-CAL |

### 1.7 Diplomacy

| Feature | Detail | Source |
|---|---|---|
| Attitude scale | enraged, icy, uncooperative, neutral, receptive, cordial, enthusiastic, worshipful | AFC-CAL |
| Treaty ladder | Receptive -> friendship (trade); cordial/enthusiastic -> affiliation (shared yards/range); worshipful -> membership | AFC-CAL |
| Treaty types | Non-aggression, friendship, affiliation, alliance, war pact | AFC-CAL |
| Gifts / bribes | Credits via Propose/Gift; rivals can bribe your minors; Communications Grid gives 500% bribe resistance | AFC-CAL |
| Demands | Tribute demands; may be accepted, rejected or ignored; can provoke war | AFC-CAL |
| Membership | Absorbs a developed minor world | AFC-BSG |
| Empire flavour | Federation best, Romulans and Klingons poor; Ferengi trade without friendship | AFC-OV, AFC-XAV |
| Alliance victory | Shared victory share | AFC-CAL |

### 1.8 Intelligence

| Feature | Detail | Source |
|---|---|---|
| Inputs | Intelligence structures and population on the intel job | AFC-CAL, TREKCORE |
| Defence | Poor internal security lets enemy agents bomb structures and steal cash | TREKCORE |
| Actions | Espionage (steal research), sabotage (bomb structures, poison population, steal/destroy ships), frame another empire | AFC-OV, AFC-CAL |
| Best spies | Cardassians and Romulans | AFC-OV, AFC-CAL |
| Opinion | Sabotage rarely worth it; AI rebuilds faster | AFC-XAV |

### 1.9 The five majors

| Empire | Distinct mechanics | Source |
|---|---|---|
| Federation | Best diplomacy and research; morale hurt by aggression and bombing; Defiant heavy escort | AFC-XAV, AFC-CAL |
| Klingons | Morale rises with war; doubled ground defence/population counts; legendary crews; cloaking BoP; poor diplomacy | AFC-XAV, AFC-CAL |
| Romulans | Cloaked fleets; cannot bomb planets (must invade); strong spies | AFC-XAV |
| Cardassians | Hardest start; espionage can out-produce research; strong subjugation, faster production; Inquisition | AFC-XAV, WIKI |
| Ferengi | Trade with minors without treaties; weakest research; no training; cash and swarm tactics | AFC-XAV, WIKI |

Ease ranking (easiest first): Romulan, Federation, Klingon, Cardassian, Ferengi (AFC-OV; AFC-CAL puts Cardassian/Ferengi last similarly).

### 1.10 Minor races (30 in vanilla; each gives one buildable structure) - source AFC-MIN

| Race | Structure | Bonus |
|---|---|---|
| Acamarians | Clan hall | +100% credits, +1 morale |
| Andorians | War college | +25 ship experience |
| Angosians | Super soldier academy | +50% ground combat |
| Antedeans | Harvesting complex | +100% food |
| Anticans / Selay | Mustering base | +50% ground defence |
| Bajorans | Jalanda forum | +1 morale empire-wide |
| Bandi | Architectural center | +100% construction research |
| Benzites | Industrial center | +50% industry |
| Betazoids | Counseling academy | +50% security |
| Bolians | Cosmetology center | +50% espionage |
| Bynars | Planetary computer | +100% computer research |
| Caldonians | Research think tank | +25% research empire-wide |
| Chalnoth | Gladiatorial arena | +100% weapon research |
| Edo | Palace of Edo | +1 morale empire-wide |
| Ktarians | Game studio | +50% sabotage |
| Malconians | Kinetics lab | +100% propulsion research |
| Mintakans | Farm | +100% food |
| Mizarians | Monument of surrender | +5 morale |
| Nausicaans | Recruitment center | +40% ground combat |
| Pakleds | Collection facility | +100 research |
| Sheliak | Bioengineering center | +100% biotech research |
| Takarans | Physics institute | +100% energy research |
| Talarians | Defense network | +100% ground defence |
| Tamarians | Mythology library | +300 research |
| Trills | Research committee | +30% research empire-wide |
| Ullians | Psychohistorical archive | +25% intel |
| Vulcans | Science academy | +35% research empire-wide |
| Yridians | Intelligence service | +40% espionage |
| Zakdorn | Military academy | +100 ship experience |

Minors can build ships but cannot colonise or conquer; they stay in one system (LPA). Mostly weak ships, scrap them; Yridian scout is the exception (AFC-CAL).

### 1.11 Random events

| Event | Source |
|---|---|
| Plagues, earthquakes, energy meltdowns, comet strikes, red giant novas, assassinations, subspace anomalies that disable warp, climate change, viral hull infection | AFC-CAL |
| Entities: Calamarain, Chodak Dreadnought, combat drones, Husnock Raider, Gomtuu, Tarrellian ship, Borg Cubes, crystalline entity, sentient ship, gaseous life form, sentinel | AFC-CAL, AFC-XAV |
| Borg: most dangerous; can wipe out small galaxies; patch added Borg-only disable | AFC-XAV, LPA |

### 1.12 Victory conditions

| Mode | Detail | Source |
|---|---|---|
| Domination | Control 60% of galaxy population and systems alone, 75% allied (Wikipedia) **or** 70% alone / 50% allied (AFC-OV). Conflict: unresolved. | WIKI, AFC-OV |
| Vendetta | Defeat two named rivals (Federation: Romulans and Cardassians) | WIKI |

### 1.13 The AI

| Observation | Source |
|---|---|
| Builds many strike cruisers and light Ferengi raiders; rarely builds the strongest ships | AFC-CAL |
| Sends beaten ships piecemeal into the same fight | AFC-CAL |
| Declares war on strong players, often simultaneously | AFC-CAL |
| Does not behave in character (Klingons seek peace when losing) | WIKI (CNN review) |
| Difficulty levels up to "impossible" (cheating to compete) | AFC-OV |

### 1.14 UI screens

Galaxy map (2D, right-click menu to everything), system/colony management, research, intelligence, diplomacy, shipbuilding, fleet/task force, tactical combat, encyclopedia (WIKI, TREKCORE). Reviews: menus clunky as empires grow; turns up to 15 minutes late game (IGN via WIKI).

### 1.15 Multiplayer and encyclopedia

Multiplayer over internet, LAN and modem, with AI for empty slots (AFC-OV). Encyclopedia in-game; exact contents **[GAP]**. Rise of the UFP has expanded its encyclopedia (changelog URL above).

---

## 2. Mods and remakes

| Project | What it added or changed | Why players valued it | Source |
|---|---|---|---|
| **Ultimate Editor (UE)**, by DCER | Java tool that edits most game values: ship models, shiplists, galaxy sizes/shapes, minor-race counts and tech levels. | Foundation of nearly all BOTF mods. | https://www.armadafleetcommand.com/botf/ultimate-editor-basics ; https://www.armadafleetcommand.com/botf/large-map-generation-chart-and-tutorial-for-ue |
| **Large-map tutorial** | Custom galaxy sizes, spirals, rings (unstable above 52x39). | Bigger maps. | AFC-MAP |
| **Resolution patch / GUI mods** | Raise resolution to 1024x768 and 1366x768 widescreen. | Playable on modern monitors. | https://www.armadafleetcommand.com/botf (via botfrespatch in AFC-MAP) |
| **All in One / Multi installer** | Bundled installer with a modern-system fix and a selection of mods; 800x600 base required first. | Easiest way to run it on current Windows. | https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=16019 ; Unity One wiki search result |
| **Ultimate Dominion Mod III (3.0.1ddb)** | Dominion replaces the Ferengi, with its own fleet, structures and shapeshifter sabotage building; Breen, Son'a, Maquis, Miradorn minors; Bajor replaces Edo; DS9 replaces the Edo Guardian; Borg sphere; species 8472; Protectorate HQ for subjugated systems; eight-phaser models; armed transports/colony ships; reworked shared morale; adjusted gifts (costlier minors); standardised AI buy priorities; AI random events off; every minor has dilithium. | "Hardest AI yet", DS9-era content, balance work. | https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=16019 |
| **Dominion mod criticisms** | Single Borg sphere unbeatable; Galaxy-X removed; one user wants fewer phasers per ship and stronger torpedoes. | Shows balance disputes. | https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=48107 |
| **Supremacy (BotF2), Mike Strobel, later "Iceman"** | Open-source rebuild. Alpha: six research fields, one shared tech tree, fog of war, labour units (10 pop), dilithium/deuterium/raw materials/personnel, denounce/commend/threaten/propose diplomacy, 5 races incl. Dominion; no ship design. Later patches: threats and demands affect relations, embargoes, Borg AI, improved raiding, Construction Ship III. Combat engine unfinished: a 2012 comment mentions trying Unity. **I found no source confirming 3D combat shipped.** | Closest spiritual successor; actively patched to Oct 2020. | https://www.spacesector.com/blog/2010/01/star-trek-supremacy-open-source-free-game-in-development/ ; https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=53054 |
| **Rise of the UFP (BotF2 repo, "Version of Supremacy")** | C#/WPF/XNA. Random galaxies, 5 empires, terraforming, minor annexation/conquest, wormholes, Borg invasion events, multiplayer goal. Changelog: summary dialogs, zoom levels, hotkeys, shipyard queue, system-position finder, new fleet orders, deuterium rules, encyclopedia. MS-RL licence, 2,290 commits, no releases listed. | Free and open, active into 2026. Assets by email request. | https://github.com/BotF2/BotF2 ; https://raw.githubusercontent.com/BotF2/BotF2/master/_ChangeLog_for_Rise_Of_The_UFP.txt |
| **Birth of the Empires (BotE)** | Non-Trek, free, open source (MPL 2.0). 6 majors, 142 minors, alliances, war pacts, trade, bribing anyone, scalable AI including non-cheating level, multiple victory paths, TCP/IP multiplayer, sandbox mode, "continue after win", Android port, Windows/Linux. Combat visualisation noted as incomplete in the wiki. | Described as strong AI; works as a polished MoO2/BotF hybrid. | https://github.com/bote-team/bote/wiki ; https://blotunga.itch.io/birth-of-the-empires ; https://www.spacesector.com/blog/?p=3176 |
| **"Galaxies" mods; "Ultimate Mod"** | **[GAP]** Not found by name in any source. Rise of the UFP's galaxy features are only generic. Ask Simon for the exact name or link. |  | searches returned nothing |
| **Ages of the Federation** | Appears on Moddb as a mod for another game (not BOTF). Excluded. | n/a | https://www.moddb.com/mods/ages-of-the-federation |

---

## 3. Community verdict

| Theme | Evidence | Source |
|---|---|---|
| Combat | "Somewhat lacking" (PC Zone); mod users want fewer phasers per ship, stronger torpedoes, longer fights | WIKI, https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=48107 |
| Late game / pacing | Turns up to 15 minutes; Borg event can end small games; steep learning curve | WIKI, AFC-XAV |
| AI | Limited, rarely builds top ships, piecemeal attacks, out-of-character diplomacy | AFC-CAL, WIKI |
| Micromanagement | Menus clunky as empire grows; only one buy per system per turn | WIKI, AFC-CAL |
| Bugs / engine | 800x600 only, crashes on big maps, needs install fixes | AFC-OV, AFC-MAP |
| Requested | Real remake, bigger map, Dominion and DS9 races, stronger torpedoes, ship variety | https://steamcommunity.com/app/1622900/discussions/0/3882725182506664392 (Star Trek Infinite thread, 2023); AFC thread above |
| What a modern remake needs | No source found that states a single list. Compiled from the above: fixed AI that builds top ships and keeps character; readable combat; no per-system buy lock; automation for colonies; scalable galaxy; moddability; multiplayer; keep the Borg threat tunable | synthesis, not a quote |

Review scores: GameRankings 69%, "average" (WIKI). Praise went to presentation, menus, music (WIKI). Reddit and GOG/Steam reviews of BOTF itself were not retrievable in this pass **[GAP]** (GameSpot user reviews blocked with 403).

---

## 4. Modern 4X comparators (brief)

| Game | Does well | Relevance | Source |
|---|---|---|---|
| Stellaris | Deep empire management, grand-strategy RTWP, ongoing development | Base for Star Trek: Infinite | https://www.thesixthaxis.com/2023/10/12/star-trek-infinite-review/ ; search result summaries |
| Star Trek: Infinite | Trek flavour, exploration, four distinct majors; OpenCritic 71, Steam Mixed. Criticised for bugs, complexity, warfare "underwhelming", ship unlock order ignoring canon, starting majors too close, no course change while warping | Cautionary tale: a Trek skin on a generic engine disappointed fans | https://opencritic.com/game/15636/star-trek-infinite ; https://steamcommunity.com/app/1622900/discussions/0/3882725182506664392 |
| Endless Space 2 | Turn-based, strongly divergent factions with unique mechanics | Matches BOTF's five distinct empires | https://www.cbr.com/stellaris-vs-endless-space-2/ |
| Sins of a Solar Empire 2 | Real-time tactical fleet combat, co-op; weak UI | Combat benchmark | https://forums.sinsofasolarempire2.com/ (search result summary) |
| Distant Worlds 2 | Pausable real-time, living galaxy, automation, heavy customisation | Model for automating colonies, a BOTF pain point | https://nodal.gg/game/distant-worlds-2-1531540 |

Only search-result summaries were available for the last three; treat them as player impressions, not reviews.

---

## Gaps to close if Simon wants a second pass
1. Full tech tree and per-level unlocks (manual was blocked).
2. Exact vanilla victory percentages (60/75 vs 70/50 conflict).
3. Whether Supremacy ever shipped 3D combat.
4. The named "Ultimate Mod" and "Galaxies" mods.
5. Steam/GOG/Reddit review text and YouTube let's-play descriptions.
6. Calladice's FAQ remaining 95k chars (multiplayer part).
