# Remaster Event Cards

**Summary of Coverage:**
- **Total Event Cards Written:** 17
- **Covered Section 1.11 Items (17):**
  - **Random Disasters / Events (6):** earthquakes (`evt_earthquake`), energy meltdowns (`evt_energy_meltdown`), comet strikes (`evt_comet_strike`), assassinations (`evt_assassination`), climate change (`evt_climate_change`), viral hull infection (`evt_viral_hull_infection`).
  - **Entities (11):** Calamarain (`evt_calamarain`), Chodak Dreadnought (`evt_chodak_dreadnought`), combat drones (`evt_combat_drones`), Husnock Raider (`evt_husnock_raider`), Gomtuu (`evt_gomtuu`), Tarrellian ship (`evt_tarrellian_ship`), Borg Cubes (`evt_borg_cubes`), crystalline entity (`evt_crystalline_entity`), sentient ship (`evt_sentient_ship`), gaseous life form (`evt_gaseous_life_form`), sentinel (`evt_sentinel`).
- **Section 1.11 Items Excluded (Already in `events.json`):**
  - Plagues (`events.json` id: `Plague`)
  - Red giant novas (`events.json` id: `SupernovaWarning`)
  - Subspace anomalies that disable warp (`events.json` id: `SubspaceRift`)
- **Duplicate Check:** Zero duplicates of `events.json` IDs (`Plague`, `SupernovaWarning`, `SubspaceRift`, `TradeBoom`, `DerelictVessel`, `LaborUnrest`, `ScientificBreakthrough`).

---

## Random Disasters

### evt_earthquake
- **ID:** `evt_earthquake`
- **Title:** Tectonic Cataclysm
- **Body:** Severe seismic upheaval has shattered subterranean infrastructure across the colony. Power grids have severed, industrial assembly complexes have collapsed, and civilian districts report catastrophic devastation. Planetary administrators plead for emergency aid before civil collapse accelerates.
- **Choices:**
  - **Deploy emergency relief:** Spends colonial funds to restore essential infrastructure and stabilize local morale.
  - **Impose martial order:** Maintains industrial output and curbs looting through military force while harming morale.
  - **Mandate self-reliance:** Colony repairs damage independently without state expenditure, suffering prolonged productivity losses.
- **Eras:** tos, movie, tng, voy, late

### evt_energy_meltdown
- **ID:** `evt_energy_meltdown`
- **Title:** Core Containment Breach
- **Body:** Catastrophic magnetic failure has destabilized the primary planetary power generators. Cascading radiation leaks threaten metropolitan centers, and engineers warn that total thermal runaway will destroy surrounding orbital facilities unless immediate containment protocols are authorized.
- **Choices:**
  - **Flood containment chambers:** Diverts heavy emergency water reserves to neutralize radiation, temporarily disabling planetary industry.
  - **Vent plasma atmosphere:** Vents superheated radiation into upper orbital layers, sparing surface factories while damaging orbiting craft.
  - **Enforce controlled overload:** Scuttles the generating facility completely, preventing broader civilian casualties at severe replacement cost.
- **Eras:** tos, movie, tng, voy, late

### evt_comet_strike
- **ID:** `evt_comet_strike`
- **Title:** Cometary Impact Warning
- **Body:** Long-range sensors track a massive rogue cometary fragment falling along an unpredicted orbital trajectory toward our settlement. Without defensive deflection or subterranean shelter procedures, surface ecosystems and industrial sectors will suffer devastating atmospheric impact damage.
- **Choices:**
  - **Disrupt with orbital fire:** Fleet and orbital batteries pulverize the comet into harmless debris, expending defensive munitions.
  - **Subterranean shelter protocol:** Moves population underground to eliminate civilian casualties, leaving surface installations vulnerable to blast damage.
  - **Tractor beam deflection:** Requires heavy engineering energy expenditure to alter the trajectory, diverting local research capacity.
- **Eras:** tos, movie, tng, voy, late

### evt_assassination
- **ID:** `evt_assassination`
- **Title:** High Official Assassinated
- **Body:** Hostile operatives have assassinated a prominent colonial governor during an official tour of the administrative sector. The killing has triggered civilian panic, widespread rioting, and mutual accusations between regional factions, destabilizing government authority across the system.
- **Choices:**
  - **Launch security crackdown:** Intelligence units suppress civilian disorder and unmask foreign spies, temporarily suppressing civil liberties.
  - **State funeral and unity rally:** Restores public confidence and elevates national resolve, funded through substantial treasury expenditure.
  - **Blame rival empire:** Directs domestic fury against a neighboring rival, increasing war readiness while provoking diplomatic outrage.
- **Eras:** tos, movie, tng, voy, late

### evt_climate_change
- **ID:** `evt_climate_change`
- **Title:** Atmospheric Destabilization
- **Body:** Unchecked industrial exhaust and solar magnetic fluctuations have induced severe meteorological instability across the hemisphere. Violent electrical storms, ocean temperature shifts, and prolonged droughts threaten agricultural output and jeopardize ongoing terraforming milestones across the colony.
- **Choices:**
  - **Deploy climate control arrays:** Invests treasury and biotech expertise to stabilize global weather patterns and secure harvests.
  - **Ration agricultural reserves:** Protects industrial factory output while food supplies tighten and colonial morale drops.
  - **Enforce ecological scrubbers:** Directs factories to reduce emissions, slowing industrial production while cleansing the atmosphere.
- **Eras:** tos, movie, tng, voy, late

### evt_viral_hull_infection
- **ID:** `evt_viral_hull_infection`
- **Title:** Biometallic Hull Infection
- **Body:** Maintenance teams report an aggressive microscopic silicon-eating organism spreading across starship hulls and orbital dock bulkheads. The pathogen metabolizes duranium plating into brittle porous slag, degrading defensive shielding and threatening vessel structural integrity throughout the sector.
- **Choices:**
  - **Plasma purge bath:** Burns away the infestation using high-energy plasma, slightly singeing outer hulls while halting infection.
  - **Biochemical counter-agent:** Scientists develop an enzymatic neutralizer, requiring biotech lab focus while preserving ship armor.
  - **Quarantine affected vessels:** Isolates contaminated ships at remote moorings, taking them off active duty until contamination subsides.
- **Eras:** tng, voy, late

---

## Space Entities

### evt_calamarain
- **ID:** `evt_calamarain`
- **Title:** Calamarain Cloud Incursion
- **Body:** A swirling vortex of ionized plasma has manifested near our shipping lanes. Sensor telemetry confirms the entity possesses rudimentary collective intelligence, generating deadly electromagnetic pulse waves that tear into starship deflector shields and disable delicate computer systems.
- **Choices:**
  - **Harmonic shield dampening:** Modulates deflector shields to absorb pulse energy, allowing safe transit while draining engine power.
  - **High-yield phaser discharge:** Drives the lifeform away through directed energy barrage, incurring minor structural wear on weapons.
  - **Scientific sensor probe:** Collects valuable electromagnetic research data while enduring continuous shield degradation.
- **Eras:** tng, voy, late

### evt_chodak_dreadnought
- **ID:** `evt_chodak_dreadnought`
- **Title:** Chodak Dreadnought Sighted
- **Body:** A massive armored juggernaut belonging to the ancient Chodak has emerged from uncharted deep space. Heavily armed and contemptuous of peaceful overtures, the alien behemoth targets sovereign infrastructure and warns that all lesser species must yield territory immediately.
- **Choices:**
  - **Engage with heavy fleet:** Deploys frontline warships to confront the dreadnought in battle, risking combat casualties for salvaged tech.
  - **Surrender border outpost:** Evacuates the contested outpost to avert conflict, suffering territorial and morale humiliation.
  - **Deploy electronic decoy drones:** Lures the juggernaut away from populated colonies into uninhabited space at moderate credit cost.
- **Eras:** tng, voy, late

### evt_combat_drones
- **ID:** `evt_combat_drones`
- **Title:** Autonomous Combat Drone Strike
- **Body:** An ancient military testing drone has activated and begun hunting starships within the sector. Each engagement causes its adaptive computer matrix to counter our tactics, making subsequent encounters increasingly dangerous for patrols operating near its perimeter.
- **Choices:**
  - **Coordinated tactical volley:** Concentrates maximum simultaneous fleet firepower to vaporize the drone before its shielding adapts.
  - **Subspace shutdown signal:** Task force computer specialists attempt to override command subroutines, advancing computer science research.
  - **Evasive route diversion:** Reroutes all commercial and military traffic around the hazard, increasing travel times across the sector.
- **Eras:** tng, voy, late

### evt_husnock_raider
- **ID:** `evt_husnock_raider`
- **Title:** Husnock Raider Intercept
- **Body:** A predatory Husnock warship has entered territorial space, raiding commerce lanes and demanding unconditional tribute. Renowned for boundless aggression and destructive pride, the raider opens fire upon any vessel failing to surrender immediate cargo manifests.
- **Choices:**
  - **Repel the raider:** Dispatches war fleet to destroy the marauder, protecting local trade and demonstrating imperial resolve.
  - **Pay demanded tribute:** Hands over treasury credits to buy safe passage, preserving shipping while denting military reputation.
  - **Set an ambush decoy:** Uses armed Q-ships disguised as transports to cripple the raider, capturing valuable enemy intelligence.
- **Eras:** tng, voy, late

### evt_gomtuu
- **ID:** `evt_gomtuu`
- **Title:** Living Starship Sighting
- **Body:** A vast organic vessel of ancient origin drifts silently near a distant star system. Sensors reveal the colossal bio-mechanical entity is sentient, grieving the extinction of its makers, and passively broadcasting melancholic telepathic resonance across surrounding subspace channels.
- **Choices:**
  - **Establish telepathic communion:** Sends empathetic scientific envoys to comfort the entity, gaining profound propulsion and biotech insights.
  - **Escort beyond borders:** Assigns diplomatic escorts to guide the creature into deep space, preventing interference with shipping.
  - **Harpass defensive scans:** Probes its organic skin with intense active sensors, triggering a concussive shockwave that repels our ships.
- **Eras:** tng, voy, late

### evt_tarrellian_ship
- **ID:** `evt_tarrellian_ship`
- **Title:** Tarellian Plague Vessel
- **Body:** A sub-light refugee ship carrying the last survivors of the deadly Tarellian virus approaches our planetary system. The desperate passengers plead for sanctuary, yet planetary medical officers warn that contact will unleash an incurable biological pathogen across our worlds.
- **Choices:**
  - **Enforce strict quarantine orbit:** Provides food and energy supplies at a safe distance, maintaining morale without risking viral outbreak.
  - **Quarantine on isolated moon:** Settles refugees on an unpopulated world, risking eventual biosecurity breach to uphold humanitarian duty.
  - **Turn vessel away:** Directs defensive batteries to deny system entry, preserving total planetary safety while harming moral standing.
- **Eras:** tng, voy, late

### evt_borg_cubes
- **ID:** `evt_borg_cubes`
- **Title:** Borg Cube Incursion
- **Body:** A colossal cybernetic cube has torn through subspace into sovereign space. The collective vessel systematically ignores diplomatic overtures, scans all planetary defenses, and begins deploying tractor beams and cutting lasers to assimilate population centers and technology.
- **Choices:**
  - **Total fleet mobilization:** Summons every available armada into coordinated defensive assault, suffering heavy ship losses to repel the invader.
  - **Planetary shield lockdown:** Overcharges surface deflector domes to prevent orbital beaming, shielding citizens while surface facilities fall.
  - **Subspace frequency disruption:** Discharges experimental pulse emitters to disrupt hive communications, slowing assimilation at huge energy cost.
- **Eras:** tng, voy, late

### evt_crystalline_entity
- **ID:** `evt_crystalline_entity`
- **Title:** Crystalline Entity Approaches
- **Body:** A titanic snowflake-shaped spaceborne lattice approaches an inhabited colony world. Sensor logs reveal the creature feeds upon electromagnetic and biological energy, converting lush planetary biospheres and organic populations into barren lifeless stone as it passes.
- **Choices:**
  - **Graviton resonance pulse:** Modulates graviton beams from orbital stations to shatter the lattice matrix, slaying the cosmic menace.
  - **Acoustic communication attempt:** Transmits complex musical algorithms to reason with the entity, risking delayed evacuation for scientific enlightenment.
  - **Emergency planetary evacuation:** Ferries as many colonists as possible onto fleeing transports, abandoning surface infrastructure to destruction.
- **Eras:** tos, movie, tng, voy, late

### evt_sentient_ship
- **ID:** `evt_sentient_ship`
- **Title:** Sentient Vessel Detected
- **Body:** An enigmatic autonomous artificial starship of immense proportions has halted near colonial boundaries. Guided by an awakened artificial intelligence seeking knowledge of its creators, the vessel analyzes our defense databanks and demands comprehensive archives of galactic history.
- **Choices:**
  - **Transmit historical archives:** Shares comprehensive scientific and cultural databases, earning the machine's gratitude and advanced computer knowledge.
  - **Sever communication links:** Erects cybersecurity firewalls and scrambles frequencies, guarding military secrets while provoking alien hostility.
  - **Attempt computational infiltration:** Uploads invasive logic algorithms to disable its propulsion, risking retributive energy blasts for structural salvage.
- **Eras:** tos, movie, tng, voy, late

### evt_gaseous_life_form
- **ID:** `evt_gaseous_life_form`
- **Title:** Hostile Gaseous Lifeform
- **Body:** A predatory ionized gas cloud with rudimentary consciousness has drifted into an inhabited system. Drawn by organic neural energy, the entity descends into lower atmosphere and preys upon planetary settlements, asphyxiating populations while baffling standard kinetic weaponry.
- **Choices:**
  - **Deploy radioactive bait:** Uses irradiated decoys to lure the cloud into empty space, expending industrial resources to save lives.
  - **Antiproton beam dispersal:** Fires synchronized high-energy particle bursts into the cloud, dissipating its cellular structure safely.
  - **Hermetic habitat lockdown:** Seals colonists inside environmental bunkers until the cloud departs, halting planetary commerce and mining.
- **Eras:** tos, movie, tng, voy, late

### evt_sentinel
- **ID:** `evt_sentinel`
- **Title:** Ancient Guardian Awakens
- **Body:** A godlike orbital defense construct has emerged above an untouched alien world. The ancient sentinel broadcasts an uncompromising decree prohibiting orbital intrusion, raising impenetrable forcefields and charging destructive energy beams against any vessel approaching planetary orbit.
- **Choices:**
  - **Withdraw to treaty perimeter:** Orders all survey and exploration ships to retreat, preserving peace and respecting sovereign alien isolation.
  - **Transmit diplomatic overture:** Offers cultural tributes and philosophical dialogue, seeking peaceful observation rights through diplomatic patience.
  - **Probing sensor reconnaissance:** Launches covert reconnaissance drones to map alien defenses, risking immediate retaliatory strikes for sensor data.
- **Eras:** tng, voy, late
