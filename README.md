# AStarTileMap: `costmap` branch

A* path planning on a **heuristic traversability cost map**, the planning side of the EECE 5554 project (terrain-aware elevation
mapping and A* path planning for a Unitree Go2 on sand). It mirrors the robot pipeline:

```
elevation map  ─▶  traversability features  ─▶  traversability  ─▶  heuristic cost map  ─▶  A*  ─▶  path
 ("elevation")      ("slope" "roughness" "step")  ("traversability")   ("cost", inflated)
```

All of these are **layers of one grid map** (as in grid_map): each stage reads layers and writes a new one. The planner only
reads `"cost"`, so a map can also start directly at the cost layer.

The pipeline and planners live in **`core/`**: plain C++ with no SFML and no ROS, so the folder can later be dropped into a
ROS 2 node. The SFML app in `src/` only loads, draws and plans by calling `core/`. **[ROS2_PLAN.md](ROS2_PLAN.md)** is the plan for that
ROS 2 package: nodes, topics, parameters, and what `core/` needs first.

This branch replaces `main`'s 20×20 tile map. The tile-map version, its history (TileMap / OriginalTileMap / the iD Tech
NavGraph) and the evidence of the original A* plans are on the `main` branch and in its README.

---

## Running it

### What you need
A C++17 compiler, CMake ≥ 3.16 and SFML 2.5 or 2.6. On Ubuntu:
```bash
sudo apt install build-essential cmake libsfml-dev
```

### Quickest: `run.sh`
```bash
./run.sh
```
This builds (only what changed) and opens the **menu**. `./run.sh rugged` opens it with "rugged" already in the map name box,
and `./run.sh --clean` rebuilds from scratch first.

### From the terminal, step by step
```bash
cmake -S . -B build
```
```bash
cmake --build build
```
```bash
cd build && ./AStarTileMap
```

### From VS Code
Open the folder, install the recommended **C/C++** and **CMake Tools** extensions, pick the **GCC** kit, then use **Build** and **▷ Run**.

### Tests
```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
`tests/test_costmap.cpp` tests `core/` only (no SFML):
- **The grid map:** indices, world positions, layers, and the file round trip (including unknown values).
- **Every pipeline stage on terrain with known answers:**
  - flat ground: no slope, roughness or step;
  - a ramp of known angle: that slope, and no roughness;
  - a step of known height: found at the edge only;
  - holes: unknown features.
- **Scoring rules:** the traversability scoring (ETH rule and the strict variant), Rajput's cost rule, the unknown-cell
  policies, and the inflation radius.
- **All four planners against brute force** on 400 random cost maps:
  - A* and Dijkstra's: the cheapest path;
  - BFS: the fewest steps;
  - DFS: a valid path;
  - none of them ever enters a lethal cell.
- **`costWeight`:** A* crosses a costly strip when cost doesn't matter and detours around it when it does.
- **The example maps:** each has a path, flat costs 0 everywhere, and hybrid has no route that avoids rugged terrain.

### Regenerating the example maps
```bash
cmake --build build && ./build/generate_maps
```
Run it from the repo folder. It rewrites `maps/flat.gridmap`, `rugged`, `dungeon` and `hybrid`, and prints a check of each.

---

## The maps (in `maps/`)

All are 72 × 72 cells at 0.1 m per cell (7.2 m × 7.2 m). On a screen up to 1440 px tall that is 10 px per cell (a 720 px map):
the 10 px wall sprite draws 1:1 and the floor sprites at exactly half size. An empty map name loads **flat**.

| Map | How it is made | What it shows |
|---|---|---|
| **flat** | elevation 0 everywhere, through the pipeline | cost 0 everywhere: the plain-distance baseline |
| **rugged** | dunes + ripples + five steep hills + fine bumps, through the full pipeline | hills become lethal (stone); the rest has costs between 0 and 0.5; A* trades distance against cost |
| **dungeon** | your old `maps/dungeon.txt` (20 × 20) scaled ×3.6 (each cell takes the old tile it falls in). **The cost layer is written directly**: walls lethal, floor 0. Elevation (walls 0.5 m) is for display only. | a map that starts at the cost layer; flat and lethal only |
| **hybrid** | a dungeon zone (two 0.5 m walls, a zig-zag corridor), then a rocky 0.8 m ridge across the whole map with **three rugged passes**, flat ground below; through the full pipeline | every route must cross rugged terrain (checked). The passes are three options: left roughest (mean cost 0.26), middle 0.19, right smoothest (0.10) but farthest. A* takes the middle one. |

The menu lists every `.gridmap` file in `maps/`. A name that doesn't exist starts a new flat map, and **SAVE** saves to
`maps/<name>.gridmap`. The four example maps are **protected**: SAVE on one of them asks for a name and saves a copy, so
only `generate_maps` changes them (the tests read them). (The old 20 × 20 `.txt` maps are still in `maps/` for the main branch; this branch doesn't list them.)

### File format: `grid_map_msgs/msg/GridMap` (ROS 2 Jazzy) in YAML
Each map is two files in `maps/`:

- **`<name>.gridmap`** is a [`grid_map_msgs/msg/GridMap`](https://github.com/ANYbotics/grid_map/tree/jazzy/grid_map_msgs/msg)
  written as YAML in the shape `ros2 topic echo` prints it, so the file *is* the message:
  ```bash
  ros2 topic pub --once /elevation_map grid_map_msgs/msg/GridMap "$(cat maps/rugged.gridmap)"
  ```
  ```
  header:       stamp 0, frame_id: map
  info:         resolution: 0.1, length_x: 7.2, length_y: 7.2, pose: centre at (0, 0, 0), no rotation
  layers:       [cost, elevation, roughness, slope, step, traversability]
  basic_layers: []
  data:         one std_msgs/Float32MultiArray per layer, in grid_map's storage order
  outer_start_index: 0, inner_start_index: 0
  ```
  **Storage order** (as `grid_map_ros` writes it): column-major, with `dim[0] = column_index` (size = rows) and
  `dim[1] = row_index` (size = columns). Index (0, 0) is the corner with the **largest x and y**, and i runs toward −x and
  j toward −y. So our cell (column, row) is at `data[row * columns + (columns − 1 − column)]` (`getMessageIndex()`).
  Unknown values are `.nan`. Each line of `data` is one of grid_map's matrix columns, which is one of our rows read from
  right to left. When reading, block or flow sequences, either storage order, and non-zero start indices are accepted.
- **`<name>.endpoints.yaml`** holds the start and end of the path. They are not part of the message, so they are kept next
  to it as world positions in metres in the `map` frame (each the centre of its cell). A missing file or entry means no
  start / end.

Every layer of the map is saved (`cost`, `elevation`, `slope`, `roughness`, `step`, `traversability`). A file that has an
`elevation` layer but no `cost` layer gets the whole pipeline run on it when it loads. The format was checked end to end
against ROS: the YAML loads into a real `grid_map_msgs/msg/GridMap`, and `grid_map_ros`'s `fromMessage` puts every
feature at the right world position. (The first format was a plain text `gridmap 1` file; the example maps were converted.)

---

## Screens and controls

**MENU:** the map name (text box), the algorithm (dropdown: A*, Dijkstra's, BFS, DFS), the **size of a new map** (cells per
side, 5–180, at 0.1 m per cell), **START** and **EXIT** (or Esc). The size is only used when the name is a new map; a saved map keeps its own size.
START both loads and creates: type a saved map's name to open it, or a new name to start one. *(TODO: this relies on
remembering map names; it could become a **Load** dropdown of the saved maps plus a **New map** button.)*
The default is **36** (3.6 m): it fits a 1080p screen at 20 px cells, so the 20 × 20 sprites are drawn 1:1.

**The window resizes to the map.** Each cell gets the biggest of 40, 20, 10 or 5 px that fits on your screen, so the sprites
are always at a clean scale (×2, ×1, ×½, ×¼); only a map too big even for 5 px cells is squeezed to fit. A map smaller than
the side panel is centred vertically. The window goes back to the menu's size on the way out.

**PLAYING:** the map with a 40 px margin (the window sized to it), the **colour-scale legend** of the current view (a
vertical bar between the map and the panel), and the side panel: map, size, the **algorithm**, **view**, **colours**
and **left click** dropdowns (plus **height** and **brush** with *Set terrain*), path length (cells and metres), path cost,
cells searched, and the controls.

| Input | Effect |
|---|---|
| **Algorithm** dropdown | Change the algorithm (A*, Dijkstra's, BFS, DFS); the path re-plans at once. The menu's choice is where it starts. |
| **Left click** dropdown | **Place start / end** (the rows below) or **Set terrain** |
| *Set terrain:* **left click** or **drag** | Adds a **tapered Gaussian** of the brush **height** to the elevation inside the brush box (outlined in cyan): `z ← z + height · w`, where `w` is 1 at the box's centre and falls to exactly 0 at its border, so nothing outside the box changes (a 1-cell brush changes only that cell). A negative height digs a hollow; clicks add up. With a height of **0** it **flattens** the box toward 0 m instead: `z ← z · (1 − w)`. It applies once per click and once each time a drag enters another cell. Only elevation is edited: the slope, roughness, step, traversability and cost are rebuilt from it (they reach a couple of cells past the box, and inflation spreads a lethal patch further) and the path re-plans. **Height** presets: −0.05, −0.02, 0 (flatten), +0.02, +0.05 m (walkable with every brush), +0.50 m (lethal). **Brush**: 1, 3 × 3, 5 × 5 or 9 × 9 cells; a wider brush is a gentler slope, so it can go higher before it turns lethal (`test_costmap` prints the measured table). Saved with the map. |
| *Set terrain:* **↑ / ↓** | Raise / lower the brush height by 1 cm (**Shift**: 5 cm). The panel shows the height. |
| *Place start / end:* **left click** a cell | Select it. A list appears beside it: **Start** / **End** on any walkable cell (shown as **Move start** / **Move end** when they already exist), or **Clear** on the start or end cell. Lethal cells can't be selected. |
| *Place start / end:* **↑ / ↓** | Choose; the cell previews it |
| **Left click** the same cell again, or **Enter** | Place it, and the path re-plans |
| **Right click**, **Backspace** or **Esc** | Unselect (Esc goes back to the menu when nothing is selected, like MENU) |
| **S**, or the **Searched** toggle | Show / hide the purple searched cells. Off, the planner still searches and the panel still counts them; they just don't light up. |
| **W**, or the **Waypoints** toggle | Draw the path as its **waypoints** (yellow lights on the cells they fall in) instead of every cell (blue). With Searched off too, only the waypoints and the start / end show. |
| **V**, or the view dropdown | Change the view: **Terrain**, **Elevation**, **Cost**, **Traversability**, **Slope**, **Roughness**, **Step** |
| **Colours** dropdown | The data views' colour map: **red-yellow-green** (ColorBrewer's RdYlGn, reversed), **viridis**, or **grayscale** |
| **SAVE** button, or **Ctrl+S** | Save to `maps/<name>.gridmap` (and its endpoints file) and keep editing. On an example map it asks for a name and saves a copy instead (a name that exists needs a second Save copy to overwrite). |
| **MENU** button, or **Esc** | Back to the menu. With unsaved edits (the panel shows **`*`** after the map's name) it first asks *Save changes?* **Save** / **Don't save** / **Cancel** (Enter = Save, Esc = Cancel). Closing the window asks the same. |
| **EXPORT** button | Export the map and the plan as ROS 2 messages (below): **Save file** writes `exports/<map>_<topic>.yaml`, **Copy** puts it on the clipboard. |

**The side panel** lists the controls with small sprites (`assets/Textures/controls`, drawn by
`tools/make_control_sprites.py`): a mouse with the left or right button lit, arrow keys, and blank keycaps with the key
written on them (V, S, Esc, Ctrl, Shift, Enter, Back). SAVE, EXPORT and MENU are stacked at the bottom, 12 px apart.

**Waypoints.** Each plan is also turned into the waypoints a controller would follow (`core/Waypoints.h`, shared with
`ros2/terrain_planner`): pruned to straight stretches (Rajput's thesis Algorithm 3, plus a rule that a straight line may
cost no more than the cells it replaces), then every corner plus equal pieces of at most 0.4 m, each with a heading and a
planned arrival time (0.5 m/s, slower on costly ground). The panel shows how many and how long; ROS2_PLAN.md §6 has the
details; `ROS1_REFERENCE_NOTES.md` describes how the ROS 1 (Noetic) projects did it end to end, Rohit Rajput's M4 and
husky-stable_ws (Frank Laubach and Aidan), for comparison: this project targets ROS 2 Jazzy.

**EXPORT (for ROS 2).** Each row is a ROS 2 Jazzy message in YAML (as `ros2 topic echo` prints it), on the topic
ROS2_PLAN.md gives it. The files go to `exports/` (git-ignored), named `<map>_<topic>.yaml`.

| Row | Message | Topic | Copy puts on the clipboard |
|---|---|---|---|
| Elevation map | `grid_map_msgs/msg/GridMap` (`elevation` layer) | `elevation_map` | a command for the saved file |
| Cost map | `grid_map_msgs/msg/GridMap` (every layer) | `cost_map` | a command for the saved file |
| Cost map (Nav2) | `nav_msgs/msg/OccupancyGrid`: −1 unknown, 0–99 cost, 100 lethal | `cost_map_grid` | a command for the saved file |
| Path | `nav_msgs/msg/Path`: cell centres, heading along the path | `planned_path` | the YAML |
| Waypoints | `nav_msgs/msg/Path`: the waypoints, heading along the path, stamp = planned arrival time | `waypoints` | the YAML |
| Waypoints (list) | plain text `[x,y] [x,y] ...` (metres), the husky-stable_ws scripts' arguments (`.txt`) | – | the list |
| Start pose / Goal pose | `geometry_msgs/msg/PoseStamped` | `start_pose` / `goal_pose` | the YAML |

The maps are up to ~300 KB of YAML, so their Copy writes the file and copies a ready command, to run in a terminal with
ROS 2 sourced:
```bash
ros2 topic pub --once /cost_map grid_map_msgs/msg/GridMap "$(cat '/path/to/AStarTileMap/exports/rugged_cost_map.yaml')"
```
Positions are metres in the `map` frame with the map centred on (0, 0); stamps are 0. The writers are in
`core/RosExport.h` (no SFML, no ROS), and every message type was checked against ROS 2 Jazzy's own definitions. On Linux
the clipboard belongs to the app, so paste before closing it (unless a clipboard manager keeps it).

**The views** follow the stages of the elevation-mapping pipeline (ETH's `elevation_mapping` / `grid_map` /
`traversability_estimation`, then Rajput's cost map); the legend beside the panel shows the current one's scale:

| Stage | View | Shows |
|---|---|---|
| 1. elevation map (on the robot: sensors fused into a 2.5D height grid; here painted with the brush) | **Elevation** | height, symmetric around 0 m (−max\|z\| … +max\|z\|) |
| | **Terrain** | the same elevation on the floor sprite: dark brown (low) → floor brown (0 m) → grey (high) |
| 2. terrain features, from the elevation over small windows | **Slope / Roughness / Step** | 0 → the critical value |
| 3. traversability, a weighted average of the feature scores | **Traversability** | 1 (ideal) → 0 |
| 4. cost: lethal below traversability 0.5, else 1 − t, then inflation (what the planner reads) | **Cost** | 0 → 0.5 (worst walkable); lethal cells (inflation included) in a darker red |

Every view colours each cell by its number as a retint of the floor sprite's brown (the data views with the chosen colour
map), so the tiles keep their texture; there are no wall / pit sprites for now
(the textures and `Board::updatePitCells()` are kept for when they come back). The lights stay on top as indicators
(green start, red end, blue path, purple searched, yellow waypoints).

Unknown cells are black.

Editing the map (painting terrain or costs) comes later. For now you place only the start and end.

---

## The pipeline (`core/`)

```
core/
  GridMap.h/.cpp            size, resolution, centred world coordinates, named float layers (NaN = unknown)
  GridMapFile.h/.cpp        save / load maps as grid_map_msgs/GridMap YAML (+ <name>.endpoints.yaml)
  RosExport.h/.cpp          EXPORT's ROS 2 messages: OccupancyGrid, Path, PoseStamped (and GridMap via GridMapFile)
  Waypoints.h/.cpp          path -> waypoints: prunePath (thesis Algorithm 3 + cost rule), makeWaypoints (spacing, yaw, time)
  CostMapParameters.h       every threshold, weight and radius in one struct (→ a ROS parameter file later)
  TerrainGenerator.h/.cpp   building blocks for test terrain: flat map, dunes, hills, noise, blocks
  TerrainFeatures.h/.cpp    computeSlope, computeRoughness, computeStepHeight
  Traversability.h/.cpp     computeTraversability
  CostMap.h/.cpp            buildCostMap, inflateCostMap, runCostMapPipeline
  NavGraph.h                walkable cells and their walkable neighbours (from the iD Tech template, via main)
  PathPlanner.h/.cpp        A* (pathfindingAstar), Dijkstra's, BFS, DFS on the cost layer
tools/generate_maps.cpp     builds the example maps
tools/make_control_sprites.py  draws assets/Textures/controls/*.png (keys, arrow keys, mouse buttons)
src/                        the SFML app (menu, Board = map view, Button, TextBox, Dropdown)
  Toggle.h/.cpp             a rectangular on / off switch with a label (the Searched toggle)
  Dialog.h/.cpp             a modal dialog: message, optional text box, buttons (Save changes? / Save a copy as)
  ExportPanel.h/.cpp        the EXPORT panel: one row per export with Save file / Copy
  ControlsList.h/.cpp       the panel's Controls list: rows of key / mouse sprites and words
  TerrainBrush.h/.cpp       the Set terrain mode: height / brush choices, Board::setTerrainHeight, Board::updateCostMap
```

1. **Features** (`TerrainFeatures`). Each one looks at a circular window around the cell:
   - **slope**: fit a plane `z = ax + by + c` to the window; slope = `atan(√(a² + b²))`, its angle from horizontal;
   - **roughness**: standard deviation of the heights from that plane;
   - **step**: highest minus lowest height in the window.
2. **Traversability** (`Traversability`), from **Wermelinger, Fankhauser, Diethelm, Krüsi, Siegwart & Hutter, "Navigation
   Planning for Legged Robots in Challenging Terrain" (IROS 2016)** and ETH's `traversability_estimation` package (built on
   grid_map and elevation_mapping):
   - each feature scores `1 − value / critical` below its critical value, and 0 at or above it;
   - `t = w_slope·s_slope + w_roughness·s_roughness + w_step·s_step`, with weights of 1/3 each as in ETH's default
     configuration;
   - `zeroIfAnyCritical = true` is a stricter variant in which any critical feature makes the whole cell 0.
3. **Cost map** (`CostMap`), from **Rajput, *Towards Autonomous Multi-Modal Mobility Morphobot (M4) Robot*, M.S. thesis,
   Northeastern 2023, Algorithm 1**, ground locomotion only:
   - `t < 0.5` → **lethal**. On the M4 it meant "fly here"; the Go2 can't, so lethal acts like the old walls.
   - Otherwise `cost = 1 − t`, between 0 and 0.5.
   - Unknown cells follow `UnknownPolicy` (lethal by default).
   - **Inflation:** every cell within `inflationRadius` (0.2 m: the Go2's half-width plus a margin) of a lethal cell becomes
     lethal.
4. **Planning** (`PathPlanner`), from **Rajput's Algorithm 2 / eq. 3.8**, ground nodes:
   - `g(n) = g(c) + h(c, n) + costWeight · cost(n)`;
   - `h` is the Euclidean distance in cells (eq. 3.1);
   - lethal cells are left out of the NavGraph. Since every cost is ≥ 0, h never overestimates, so A* stays optimal.
   - Dijkstra's, BFS and DFS use the same graph. BFS and DFS ignore cost apart from lethal, which shows why they are wrong here.

The grid map uses **grid_map**'s layer approach (Fankhauser & Hutter, "A Universal Grid Map Library", 2016). The elevation map
it stands in for is Fankhauser et al.'s robot-centric elevation mapping (2014, 2018).

### Parameters (`core/CostMapParameters.h`): starting values to tune
| Parameter | Value | Note |
|---|---|---|
| `slopeRadius`, `roughnessRadius` | 0.15 m | at least one cell; ETH uses 0.04–0.05 m on finer maps |
| `stepRadius` | 0.10 m | one cell around the centre, so a smooth slope isn't counted as a step |
| `criticalSlope` | 0.40 rad (≈ 23°) | lower than ETH's 1.0 rad, as a starting point for sand |
| `criticalRoughness`, `criticalStep` | 0.05 m, 0.12 m | ETH defaults |
| weights | 1/3 each | ETH default |
| `traversabilityThreshold`, `lethalCost` | 0.5, 60 | Rajput |
| `inflationRadius` | 0.2 m | Go2 half-width (~0.16 m) plus a margin |
| `costWeight` | 1.0 | how strongly A* avoids costly ground (Rajput uses 1) |

---

## Adding things

| To add | Do this |
|---|---|
| **A map** | Write a builder in `tools/generate_maps.cpp` from the `TerrainGenerator` pieces (or set the `cost` layer directly), save it with `saveGridMap`, and run `./build/generate_maps`. |
| **A terrain piece** | A function in `core/TerrainGenerator` that adds to the `elevation` layer |
| **A feature** | A `compute…()` in `core/TerrainFeatures` that writes a layer, then use it in `computeTraversability()` with a weight and critical value in `CostMapParameters` |
| **An algorithm** | Add it to `enum class PathAlgorithm` and `getPathAlgorithmNames()` in `core/PathPlanner.h`, write a `pathfinding…()` in `PathPlanner.cpp`, and add a case to `findPath()`. The menu and side panel dropdowns pick it up automatically. |
| **A view** | Add it to `enum class MapView` and `getMapViewNames()` in `src/Tile.h`, and a case in `Board::getViewLayer()`, `Board::getViewRange()` and the legend's labels in `Board::drawLegend()`. The view dropdown picks it up automatically. |

### Not on this branch
- **The character** (WASD movement) is a relic of TileMap. It is commented out in `main.cpp` and the CMake file, and its files
  are kept.
- **Map editing** (painting terrain or costs) is planned next.
