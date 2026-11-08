# Workstream 02: Galaxy and Strategic Map

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*, the galaxy map was represented as a flat 2D sector grid [AFC-OV, WIKI].
- **Sizes and Topologies:** Standard sizes included Small (roughly 20×15), Medium (30×22), and Large (40×30) sectors, with modded charts supporting up to 52×39 cells before engine memory bleed caused crashes [AFC-MAP]. Map shapes were selectable: Irregular, Spiral, and Ring [AFC-OV, AFC-MAP].
- **Minor Race Density:** Selectable at campaign generation: None, Few, Some, Many (e.g., Small map supported 0/4/4/6 minors; Large supported 0/10/16/24; hard-capped at 29 minors) [AFC-GAL, AFC-MAP].
- **Map Interaction:** Navigation used a 2D star chart with mouse pan and zoom. Left-clicking selected a sector or system; right-clicking opened a context menu permitting orders: Set Course, Attack, Terraform, Build Outpost, and Invade [WIKI, TREKCORE, AFC-BSG].
- **Hazards & Features:** Nebulae slowed ship movement and impaired shields; wormholes provided instant transit between pairs of sectors; stars of varying classes were scattered across sectors [WIKI, AFC-MAP].

Mods and Remakes:
- **Ultimate Editor (UE):** Allowed customizing spiral arm angle distributions, star density ratios, and wormhole counts [AFC-MAP].
- **Rise of the UFP & Supremacy:** Introduced multi-level strategic zoom, distinct territory influence borders, and dynamic route calculations [Rise of the UFP, Supremacy].

## 2. Modern Target

Folding Simon's explicit decision (§7 of `MASTER-PLAN.md`): The 2D star chart is replaced with a fully modernized, zoomable **3D Galaxy Map**.

1. **3D Coordinate Space & Topologies:**
   - Systems are positioned in true 3D space `(X, Y, Z)` with galactic disc thickness.
   - Map shapes:
     - *Spiral:* Multi-arm logarithmic spirals (`r = a * e^(b*θ)`) with vertical Gaussian star dispersion.
     - *Ring:* Annular star density ring with central void.
     - *Cluster / Irregular:* 3D Perlin noise spatial clusters.
     - *Elliptical:* Dense galactic core with tapering spheroidal halo.
2. **Smooth Multi-Tier Zoom:**
   - Seamless continuous zoom from Galactic Overview (disc, quadrant boundaries, territory volumes) -> Sector Tier (star clusters, fleet models, sensor spheres) -> System View (star, orbital planes).
3. **Volumetric Map Features & Hazards:**
   - Real volumetric nebulae that obscure sensor vision and disable shields in combat (`GalaxyFeatures.cpp:80-140`).
   - Drifting ion storms with animated wavefronts that damage unshielded ships (`GalaxyFeatures.cpp:160-210`).
   - Gravitational lensing around black holes and neutron star pulsar beams (`GalaxyFeatures.cpp:95-125`).
   - Dynamic 3D wormhole tunnels with stable or unstable traversal characteristics.
4. **Territory Volumes & Logistics Visuals:**
   - Translucent 3D Voronoi territory bubbles surrounding colonies, outposts (radius 3), and starbases (radius 5) (`Stations.cpp:120-150`).
   - Glowing 3D spline lines indicating active commercial trade routes, fleet flight vectors with ETA waypoints, and supply borders.
5. **Right-Click Strategic Flow:**
   - Retain the rapid right-click context menu workflow: Set Course, Attack, Intercept, Patrol, Colonize, Invade, Build Outpost, Upgrade Starbase, Terraform (`remaster/godot/scenes/galaxy/orders.gd:40-120`).
6. **Live Setup Preview:**
   - Real-time 3D rotating galaxy preview in the New Campaign screen responding immediately to seed, shape, and density sliders.

## 3. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/galaxy/galaxy_screen.gd`:
  - Master strategic HUD, quadrant mini-map, strategic filter toggles (Political, Military, Supply, Economy, Exploration).
- `remaster/godot/scenes/galaxy/galaxy_map.gd`:
  - Migrated from 2D `Control` drawing to a full `Node3D` scene containing `Camera3D` orbital rig, star meshes, particle clouds, and spline renderers.
- `remaster/godot/scenes/galaxy/orders.gd`:
  - Contextual 3D raycast target selector and floating LCARS order radial menu.
- `remaster/godot/scenes/ui/screens/new_campaign_screen.gd`:
  - Interactive 3D viewport rendering live procedural galaxy generation preview.

## 4. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Simulation.h`:
  - Extend `System` struct with `int32_t z` coordinate (scaled fixed-point integer for cross-platform determinism).
  - Add `GalaxyShape` enum (`SHAPE_SPIRAL`, `SHAPE_RING`, `SHAPE_IRREGULAR`, `SHAPE_ELLIPTICAL`).
  - Add `MinorDensity` enum (`DENSITY_NONE`, `DENSITY_FEW`, `DENSITY_SOME`, `DENSITY_MANY`).
- `remaster/game/Source/BotfSim/Simulation.cpp`:
  - Rewrite `generate()` (lines 3900–4150) to calculate 3D system positions based on mathematical shape formulas rather than a flat 2D grid shuffle.
  - Implement 3D Chebyshev/Euclidean distance metrics for sensor range, fleet warp travel, and territory borders (`Simulation.cpp:1800-1850`).
- `remaster/game/Source/BotfSim/GalaxyFeatures.cpp`:
  - Upgrade anomaly hazard checks (`isHazardAt`, `driftIonStorms`) to evaluate 3D spherical and ellipsoidal bounding volumes (`GalaxyFeatures.cpp:110-175`).
- `remaster/game/Source/BotfSim/Stations.cpp`:
  - Extend outpost and starbase supply coverage to 3D radii (`Stations.cpp:130-165`).
- `remaster/tests/scenarios/SimulationTests.cpp`:
  - Add tests validating 3D coordinate distribution, collision bounds between stars, and deterministic generator seeding.

## 5. Client Changes

- `remaster/godot/scenes/galaxy/galaxy_map.gd`:
  - Reconstruct scene hierarchy under `Node3D`.
  - Implement smooth interpolation `Camera3D` rig with pitch, yaw, and zoom clamping.
  - Instantiate billboarded star meshes with spectral class shader parameters (O, B, A, F, G, K, M).
- `remaster/godot/scenes/galaxy/orders.gd`:
  - Raycast from mouse screen coordinates to 3D sector planes to resolve system and fleet selections.
  - Draw 3D bezier curves for fleet flight paths with animated pulses showing direction.
- Shaders:
  - Fix blocky artifacts in `remaster/godot/scenes/galaxy/shaders/ion_storm.gdshader` (`CURRENT-STATE-AUDIT:245`).
  - Implement raymarched volumetric cloud shader for nebulae.

## 6. Content and Art Needs (by Asset Key)

- `vfx.star.spectral_o`: Blue hypergiant star billboard/shader.
- `vfx.star.spectral_g`: Yellow dwarf star billboard/shader (e.g., Sol).
- `vfx.star.spectral_m`: Red dwarf/giant star billboard/shader.
- `vfx.star.neutron`: Pulsar star with rotating 3D particle jet beams.
- `vfx.star.black_hole`: Accretion disc with gravitational lensing distortion shader.
- `vfx.anomaly.nebula_cloud`: Volumetric particulate cloud (gas and dust).
- `vfx.anomaly.ion_storm`: Drifting electrical wavefront with lightning flashes.
- `vfx.anomaly.wormhole`: Swirling subspace vortex mesh.
- `vfx.route.trade_active`: Glowing amber spline conduit.
- `vfx.route.fleet_course`: Green/Blue directional dashed pulse spline.

## 7. Acceptance Tests

1. **3D Galaxy Setup Preview:**
   - Open New Campaign screen, cycle galaxy shapes between Spiral, Ring, and Irregular.
   - Verify the 3D preview viewport updates smoothly, displaying distinct geometric topologies without stuttering.
2. **Camera Navigation & Multi-Tier Zoom:**
   - Launch game; rotate camera in 3D around galactic core; zoom in from galaxy-wide view down to Sol system.
   - Verify transition is seamless, star labels scale appropriately, and frame rate remains ≥60 FPS.
3. **Right-Click Order Dispatch:**
   - Select a fleet in Sector 02-04; right-click a system in Sector 05-07; select "Set Course".
   - Verify a 3D spline trajectory appears with accurate turn-arrival ETA waypoints calculated by the core.
4. **Volumetric Hazard Interaction:**
   - Plot course through a drifting ion storm; verify pathfinding highlights hazard warnings and fleet takes shield/hull damage if transit is forced.
5. **Territory Volume Projection:**
   - Construct an Outpost in a neutral sector; advance turn; verify a 3D translucent influence sphere (radius 3) expands around the outpost, updating empire sensor coverage.

## 8. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Small galaxy map (16 systems), fixed Spiral shape.
- 3D camera pan/orbit/zoom over the 16 systems.
- 1 Volumetric Nebula and 1 Wormhole pair active.
- Territory influence volumes for Federation (Sol) and Klingon (Qo'noS).
- Complete 3D fleet course splines and right-click order flow.
