# AStarTileMap

A 20×20 tile-map editor in C++ with SFML. You draw walls, place a start and an end tile, and drive a small
character around the map. It is a **path-planning visualizer**: start in green, goal in red, every tile the algorithm searched
in purple, and the planned path in blue. The path is re-planned every time you change the map. You choose the map and the
algorithm (**A\***, **Dijkstra's**, **BFS** or **DFS**) on the menu screen.

This repo merges my two old Visual Studio projects into one CMake project that builds on Linux (and Windows/macOS):

| Source | Date | What came from it |
|---|---|---|
| `TileMap` | Dec 2020 | The base: map editor, character, textures, maps. This is the newer version. |
| `OriginalTileMap` | Nov 2020 | Nothing extra. It differs from TileMap by only two lines. |
| iD Tech `C4-Template` / my `newProjectiles` | 2019–20 | `NavGraph` (walkable tiles + their neighbours), now in `src/NavGraph.h` and `Board::recalculateNav()` |

A\* is implemented in `Board::pathfinding()` as the simplest version of Sebastian Lague's algorithm, with Dijkstra's, BFS and DFS
next to it in `src/Pathfinding.cpp`. See [Path planning](#path-planning).

---

## Running it

### What you need
A C++17 compiler, CMake ≥ 3.16 and SFML 2.5 or 2.6. On Ubuntu:
```bash
sudo apt install build-essential cmake libsfml-dev
```
(On this machine all of these are already installed.)

### Quickest: `run.sh`
```bash
./run.sh
```
This builds (only what changed) and opens the **menu**. `./run.sh dungeon` opens it with "dungeon" already typed in the map name
box, and `./run.sh --clean` wipes `build/` and rebuilds from scratch first.

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
You can also pass a map name: `./AStarTileMap dungeon` pre-fills the menu's text box.

### From VS Code
1. Open this folder in VS Code. Install the recommended **C/C++** and **CMake Tools** extensions if asked.
2. When CMake Tools asks for a *kit*, pick **GCC**.
3. Click **Build** in the bottom status bar, then **▷ Run** (or the 🐞 button to debug with breakpoints).

Run it from inside `build/`, as both setups above do. CMake links `build/Textures`, `build/Font` and `build/maps` back to this
repo, so maps you save end up in `maps/`.

### Testing the path planning
```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
`tests/test_astar.cpp` compares all four algorithms with a brute-force shortest-path search. It runs on 2,000 random maps, a map
split by a solid wall (no path), the same map with one gap, and `dungeon`. It checks:
- Every algorithm finds a path exactly when one exists, using only legal steps.
- **A\*** and **Dijkstra's** find the cheapest path; **BFS** finds the fewest steps; **DFS** finds a valid path.
- Every path tile is also marked as searched, and no wall ever is.
- The board margin: tile ↔ pixel conversion includes the board's origin, and points outside the board give −1.
- Map names typed in the menu are cleaned (`""` → `default`, `maps/dungeon.txt` → `dungeon`).

---

## Screens

The program is a small state machine (`enum GameState` in `main.cpp`, in the template's `DEFINE GAMESTATES HERE` spot). It
**starts on the menu**:

```
MENU ──START / Enter──▶ PLAYING ──SAVE & MENU / E──▶ MENU   (saves the map first)
                           └─────────Esc───────────▶ MENU   (without saving)
```

**MENU**
- **Map name** (text box): click it and type. Empty loads `default`. `dungeon`, `dungeon.txt` and `maps/dungeon` all load the
  same map, and a name that doesn't exist yet starts a new empty map with a wall border. The saved maps are listed underneath.
  Note that `a` has no start or end, so no path shows until you place them.
- **Algorithm** (dropdown): A\*, Dijkstra's, BFS or DFS.
- **START** (or Enter): loads the map with that algorithm. **Esc** closes the program.

**PLAYING**: the map sits inside an outline with a 40 px margin. The panel on the right shows the map name, the algorithm, the
result (path length in tiles, path cost, tiles searched, or "No path!"), a controls reminder, and the **SAVE & MENU** button.

---

## Controls

| Input | Effect |
|---|---|
| **Left click** a tile | Select it. A list appears beside it: **Ground**, **Wall**, **Start**, **End**. Start and End are only listed while they don't exist yet (or on the tile that already is the start / end), so there's never more than one of each. |
| **↑ / ↓** | Choose from the list. The tile **previews** the highlighted type under its cyan frame. |
| **Left click** the same tile again, or **Enter** | Place the previewed type. Choosing Ground on the start or end removes it. |
| **Left click** another tile | Move the selection there |
| **Right click**, **Backspace** or **Esc** | Unselect without changing anything (Esc only goes back to the menu when nothing is selected) |
| *(automatic)* | Whenever both a start and an end exist, the chosen algorithm runs. Tiles it searched light **purple** and the path lights **blue**. Editing any tile re-plans. If there is no path, the purple shows how far it got and the panel says "No path!". |
| **W A S D** | Move the blue character. It accelerates, slides and collides with walls. |
| **SAVE & MENU** button, or **E** | Save the map to `maps/<name>.txt` and go back to the menu |
| **Esc** (nothing selected) | Back to the menu **without** saving |
| Close window | Quit **without** saving |

Hovering the mouse over a tile gives it a white frame. Both highlights are drawn on top of the tile's own sprite, so you can always see
whether it is ground, wall, start, end or path.

The previous controls (Space toggled wall/ground, Enter placed the start and then the end, right click toggled a wall) are still in
the code, **commented out** and marked `OLD controls` in `main.cpp` and `Tile.cpp`. `Board::convertTile()`, which applied them, is
still there but no longer called.

---

## What the code does

```
src/
  main.cpp          window, the MENU / PLAYING game states, input handling and the game loop (SFML template layout)
  Tile.h/.cpp       class Board: the grid, drawing, editing, load/save, NavGraph
  Pathfinding.h     enum PathAlgorithm and the names shown in the menu's dropdown
  Pathfinding.cpp   the four algorithms: A* (pathfinding), Dijkstra's, BFS, DFS, plus findPath() and getDistance()
  TilePlacement.cpp placing tiles: select a tile, choose its type with Up / Down, confirm or unselect
  NavGraph.h        vertex + NavGraph types (from the iD Tech template)
  character.h/.cpp  class Character: the player you drive with WASD
  Button.h/.cpp     class Button: a clickable rectangle with centred text
  TextBox.h/.cpp    class TextBox: a one-line text input (the map name)
  Dropdown.h/.cpp   class Dropdown: a click-to-open list of options (the algorithm)
tests/
  test_astar.cpp    checks all four algorithms against a brute-force search
assets/Textures/    tile sprites
assets/Font/        Candal.ttf (the same font as PingPong)
maps/               saved maps
```

**`main.cpp`**: Opens a 1100×800 window and starts on the **menu**. Pressing START creates a fresh 20×20 `Board` (720×720 px,
so each tile is 36 px) at a 40 px margin, loads the map named in the text box, and spawns the `Character` on the start tile (or
the first empty tile). Each frame on the PLAYING screen it:
1. Reads input. Clicks and the arrow keys call the board's placement functions (`beginPlacement`, `movePlacementChoice`,
   `confirmPlacement`, `cancelPlacement`); W A S D set flags on the character (`moveUp`…).
2. If the map changed (`navDirty`), calls `Board::recalculateNav()` to rebuild the navigation graph. Then, if a start and an end
   both exist, it runs the chosen algorithm (`Board::findPath(algorithm, startPos, endPos)`), which fills `pathTiles` and
   `searchedTiles`.
3. Draws the board, the character, the option list beside a selected tile, and the side panel.

**`Board` (Tile.h/.cpp)**
- `tileTypes`: one `tiletype` per tile (`wall`, `ground`, `startnode`, `endnode`, `checkpoint`). Tile `i` is at column `i % 20`, row `i / 20`.
- `origin`: where the board's top-left corner sits in the window (the margin). `getTileN(x, y)` converts window pixels to a tile
  index (−1 outside the board); `getTilePos(i)` converts a tile index to the window position of its top-left corner.
- `drawBoard()` draws an outline around the map, then works in three passes:
  1. every tile's own sprite;
  2. the search result: **`searchedTiles` lit purple**, then **`pathTiles` lit blue** on top;
  3. the selected tile, drawn as the type being previewed, with a cyan frame; then the hover highlight (white).
- **Placing tiles** (`TilePlacement.cpp`): `beginPlacement(tile)` selects a tile and builds `placementOptions` (ground, wall, plus
  start / end when allowed). `movePlacementChoice(±1)` moves `placementChoice`, and `confirmPlacement()` writes it to `tileTypes`,
  updating `startPos` / `endPos` / `startExists` / `endExists` and setting `navDirty` so the path re-plans. `cancelPlacement()`
  unselects. To add a new tile type, add it to `enum tiletype`, `getTileTypeName()`, `getSpriteFor()` and the list in
  `beginPlacement()`.
- `importBoard()` / `exportBoard()` read and write `maps/<name>.txt`. `cleanMapName()` tidies a typed name, and
  `getSavedMapNames()` lists `maps/`; the menu uses it, and it could later fill a dropdown of saved maps.
- `getTile(x, y)`, `calcNeighbors()`, `recalculateNav()`: the merged **NavGraph**. Every non-wall tile becomes a `vertex` holding
  the coordinates of its walkable neighbours (all 8 directions). Look one up with `nav.getVertex(x, y, v)` and read `v.adjacent`.
- The path planning functions are in `Pathfinding.cpp` (see [Path planning](#path-planning)).

**UI classes**: `Button`, `TextBox` and `Dropdown` each have a `setup()` and a `draw()`, and are fed mouse clicks or events from
`main.cpp`. They know nothing about maps or algorithms, so they can be reused (for example, a dropdown of saved maps that fills
the text box).

**`Character` (character.h/.cpp)**: Moves by velocity and acceleration with friction, and is clamped to the board's edges.
`updateNavigation()` checks the 8 tiles around it and stops movement into walls. As it moves, it turns the ground tile under it into a
`checkpoint`, which lights **yellow**. It already stores `startPosition` / `endPosition` from the board, but nothing uses them yet.

### Map file format
`maps/<name>.txt` holds 400 numbers, one per line, in tile order (left→right, top→bottom):
`0` ground, `1` wall, `2` start, `3` end. The file has no size header, so maps are always 20×20.

### Sprites (`assets/Textures`)
Every floor tile has a "light" in the middle:

| File | Look | Used for |
|---|---|---|
| `floorTile.png` | unlit | ground |
| `wall.png` | stone | wall |
| `startTile.png` | green | start |
| `endTile.png` | red | end |
| `checkPointTile.png` | yellow | the character's breadcrumb (tile it's standing on) |
| `emptyTile.png` | dark/black | unused. It would suit "explored" tiles. |
| `light_blue.png` | blue | **the A\* path** |
| `light_purple.png` | purple | **tiles A\* searched** (CLOSED set) |
| `light_red/orange/yellow/green.png` | coloured lights | free, e.g. for the OPEN set or a second agent |

---

## Path planning

All four algorithms work on the same navigation graph, return the path start → end (empty if there is none), and record the tiles
they evaluated in `searchedTiles`, so the purple/blue display works for each.

| Algorithm | Function | How it picks the next tile | Path it finds | Searches |
|---|---|---|---|---|
| **A\*** | `pathfinding()` | lowest F = G (cost so far) + H (estimate to the end) | cheapest | few tiles, heads toward the end |
| **Dijkstra's** | `pathfindingDijkstra()` | lowest G only (A\* without H) | cheapest | more, spreads evenly in every direction |
| **BFS** | `pathfindingBFS()` | oldest tile found (a queue) | fewest steps (a diagonal counts as one step, so it can cost more) | many, in rings around the start |
| **DFS** | `pathfindingDFS()` | newest tile found (a stack) | some path, usually long and winding | depends on the map |

On `dungeon`, for example: A\* finds a 31-tile path costing 356 after searching 91 tiles. BFS finds a 31-tile path costing 380
after searching 186 tiles.

**To add an algorithm:**
1. Add it to `enum class PathAlgorithm` and `getPathAlgorithmNames()` in `Pathfinding.h`, in the same order.
2. Write a `Board::pathfinding…()` function in `Pathfinding.cpp`, following the others.
3. Add a `case` for it in `Board::findPath()`.

The dropdown picks it up automatically.

## A\* details

### Evidence of what I originally planned
- **The stub was there from day one.** `Board::pathfinding()` was in the very first commit of both repos (OriginalTileMap on 2020‑11‑17,
  TileMap on 2020‑12‑08). It had `OPEN` / `CLOSED` lists and Lague-style costs: `distBtwnAdjTiles` (10 units straight) and
  `× sqrt(2)` ("~= 14 units") diagonal. The constructor called it as a test (`pathfinding(getTilePos(1), getTilePos(9))`).
  It never got past adding the start node to OPEN, and its target was computed from the start position by mistake.
  The current implementation replaces it.
- **Start/end tiles exist for a reason.** The `startnode` / `endnode` tile types, `startPos` / `endPos`, and the empty
  `createStartNode()` / `createEndNode()` were added so the path has endpoints.
- **The character was meant to follow the path.** `Character` stores `startPosition` / `endPosition` "get from board", and its
  `updateNavigation()` already builds the same 8-neighbour list A\* needs.
- **The lit tiles were meant to show the path.** The sprites are sockets that light up green/red/yellow, and the `checkpoint` type marks tiles along a route.
- **The NavGraph came from the iD Tech template.** The C4-Template's `GameMap` builds a `NavGraph` from 8-neighbour walkable tiles. My
  `newProjectiles` copy of it adds `start` and `end` tile types, which is the step that became TileMap's start/end nodes.
- **The real-world goal was a grocery store.** The READMEs say the project was meant to map a grocery store for customer navigation (the EZ-Shop startup), and
  the `TileMapProjectTemplate*` projects add an unused `Item` class for things placed on tiles.

### Reference
Sebastian Lague's A\* series: **[A\* Pathfinding (E01: algorithm explanation)](https://www.youtube.com/watch?v=-L-WgKMFuhE)**.
It explains G cost (distance from the start), H cost (estimated distance to the end), F = G + H, and the open and closed sets.
The later episodes of the series build the node grid, the implementation and a heap optimisation.

### The algorithm (Lague's version, in short)
```
OPEN  = {start}          // tiles still to evaluate
CLOSED = {}              // tiles already evaluated
loop:
    current = tile in OPEN with the lowest F (ties: lowest H)
    move current from OPEN to CLOSED
    if current == end: done, retrace the path through parents
    for each neighbour of current:
        if neighbour is a wall or in CLOSED: skip
        newG = current.G + distance(current, neighbour)
        if newG < neighbour.G or neighbour not in OPEN:
            neighbour.G = newG
            neighbour.H = distance(neighbour, end)
            neighbour.parent = current
            add neighbour to OPEN if it isn't there
distance(a, b): dx, dy = |difference| in tiles
                14*min(dx,dy) + 10*(max(dx,dy) - min(dx,dy))   // diagonal 14, straight 10
```

### How it maps onto this code
`Board::pathfinding()` in `src/Pathfinding.cpp` follows the pseudocode above line by line, using the names from my original stub:
- **Node data:** per-tile arrays `gCost`, `hCost`, `parent` (-1 = none), and `inOpen` / `inClosed` flags. F is `gCost + hCost`.
- **OPEN:** `openNodes`, a plain list. Each step scans it for the lowest F, with ties going to the lowest H. This is Lague's simplest
  version, and plenty fast for 400 tiles.
- **Neighbours:** read from the merged NavGraph (`nav.getVertex(x, y, v)`, then `v.adjacent`), so walls are never considered.
- **Retrace:** follows `parent` from the end back to the start, then reverses the list.
- **Searched tiles:** each tile moved into CLOSED is also added to `searchedTiles`, which is drawn purple.

### Possible next steps
1. **Stop corner cutting.** Like Lague's simple version, a diagonal step is allowed even between two walls that touch at a corner
   (you can see the path squeeze through such gaps in `dungeon`). For a robot, skip a diagonal step in `calcNeighbors()` when
   either of the two straight tiles beside it is a wall.
2. **Show the OPEN set too.** Collect `openNodes` when the search ends and draw them with `light_orange`, so the edge of the search shows.
3. **Animate the search.** Run one loop iteration per frame instead of the whole search at once, so you can watch the purple spread.
4. **Bigger maps.** Replace the linear scan of OPEN with a `std::priority_queue`, as in Lague's heap episode.
5. **Follow the path.** Steer the `Character` toward the centre of the next path tile instead of reading WASD.

---

## Fixed
- Mouse buttons used to be read through `event.key.code` instead of `event.mouseButton.button`. That only worked because both fields
  sit at the same memory offset. Now `event.mouseButton.button` (also fixed in TileMap and Checkers).

## Known issues (inherited from TileMap, left unchanged)
- `getTileN()` / `getTilePos()` use `tilesPerCol` where they should use `tilesPerRow`. This is fine while the map is square.
- `Character::updateNavigation()` doesn't bounds-check its 8 neighbours. On the outer row it reads outside `tileTypes`, which the wall border normally hides.
- `Character::move()` slows down leftward and rightward motion differently. The negative-x branch adds friction twice and checks `moveUp/moveDown`.
