# AStarTileMap

A 20×20 tile-map editor in C++ with SFML. You draw walls, place a start and an end tile, and drive a small
character around the map. It is an **A\* path-planning visualizer**: start in green, goal in red, every tile A\* searched
in purple, and the planned path in blue. The path is re-planned every time you change the map.

This repo merges my two old Visual Studio projects into one CMake project that builds on Linux (and Windows/macOS):

| Source | Date | What came from it |
|---|---|---|
| `TileMap` | Dec 2020 | The base: map editor, character, textures, maps. This is the newer version. |
| `OriginalTileMap` | Nov 2020 | Nothing extra. It differs from TileMap by only two lines. |
| iD Tech `C4-Template` / my `newProjectiles` | 2019–20 | `NavGraph` (walkable tiles + their neighbours), now in `src/NavGraph.h` and `Board::recalculateNav()` |

A\* is implemented in `Board::pathfinding()` as the simplest version of Sebastian Lague's algorithm. See [A\* path planning](#a-path-planning).

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
./run.sh dungeon
```
This builds (only what changed) and starts the program with `maps/dungeon.txt`. Use `./run.sh` alone to be asked for the map name,
or `./run.sh --clean dungeon` to wipe `build/` and rebuild from scratch first.

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
Unless you pass a map name (`./run.sh dungeon`), the program first **waits in the terminal** for one, and the window only opens
after you answer. It lists the saved maps:
```
Saved maps (20x20): Unamed_Map a default dungeon
Type a map name and press Enter (Enter alone = default, a new name = new empty map):
```
`dungeon`, `dungeon.txt` and `maps/dungeon` all work. A name that doesn't exist yet gives you a new empty map with a wall border;
press **E** to save it under that name. Note that `a` has no start or end, so no path shows until you place them.

### From VS Code
1. Open this folder in VS Code. Install the recommended **C/C++** and **CMake Tools** extensions if asked.
2. When CMake Tools asks for a *kit*, pick **GCC**.
3. Click **Build** in the bottom status bar, then **▷ Run** (or the 🐞 button to debug with breakpoints).
4. The map-name prompt appears in VS Code's **Terminal** panel. Type the name there.

Run it from inside `build/`, as both setups above do. CMake links `build/Textures` and `build/maps` back to this repo, so
maps you save end up in `maps/`.

### Testing the A\*
```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
`tests/test_astar.cpp` compares `pathfinding()` with a brute-force shortest-path search (Dijkstra). It runs on 2,000 random maps, a map
split by a solid wall (no path), the same map with one gap, and `dungeon`. Each case checks four things:
- A\* finds a path exactly when one exists.
- The path is as short as possible.
- Every step is a legal move.
- Every path tile is also marked as searched, and no wall ever is.

---

## Controls

| Input | Effect |
|---|---|
| **Left click** | Select a tile (cyan frame + light cyan tint; the tile underneath stays visible) |
| **Space** | On the selected tile: ground ↔ wall. A start or end tile turns back into ground. |
| **Right click** | Select a tile and toggle it at once (like left click + Space) |
| **Enter** | On the selected ground tile: place the **start** (green), or the **end** (red) if a start already exists. On a start or end tile: remove it. |
| *(automatic)* | Whenever both a start and an end exist, A\* runs. Tiles it searched light **purple** and the path lights **blue**. Editing any tile re-plans. If there is no path, the purple shows how far it got and the terminal prints `A*: no path from start to end`. |
| **W A S D** | Move the blue character. It accelerates, slides and collides with walls. |
| **E** | Save the map to `maps/<name>.txt` and close |
| **Esc** / close window | Quit **without** saving |

Hovering the mouse over a tile gives it a white frame. Both highlights are drawn on top of the tile's own sprite, so you can always see
whether it is ground, wall, start, end or path.

---

## What the code does

```
src/
  main.cpp        window, input handling and the game loop
  Tile.h/.cpp     class Board: the grid, drawing, editing, load/save, NavGraph, A* (pathfinding)
  NavGraph.h      vertex + NavGraph types (from the iD Tech template)
  character.h/.cpp  class Character: the player you drive with WASD
tests/
  test_astar.cpp  checks A* against a brute-force search
assets/Textures/  tile sprites
maps/             saved maps
```

**`main.cpp`**: Opens a 1000×1000 window and builds a 20×20 `Board`, so each tile is 50 px. It asks for a map name and spawns
the `Character` on the start tile (or the first empty tile). Each frame it:
1. Reads input and sets flags on the board (`clickPos`, `spacePressed`, `enterPressed`…) or the character (`moveUp`…).
2. Calls `Board::convertTile()`, which applies the Space or Enter edit to the selected tile.
3. If the map changed (`navDirty`), calls `Board::recalculateNav()` to rebuild the navigation graph. Then, if a start and an end
   both exist, it runs A\* (`Board::pathfinding(startPos, endPos)`), which fills `pathTiles` and `searchedTiles`.
4. Draws the board, then updates and draws the character.

**`Board` (Tile.h/.cpp)**
- `tileTypes`: one `tiletype` per tile (`wall`, `ground`, `startnode`, `endnode`, `checkpoint`). Tile `i` is at column `i % 20`, row `i / 20`.
- `getTileN(x, y)` converts pixels to a tile index; `getTilePos(i)` converts a tile index to the pixel position of its top-left corner.
- `drawBoard()` works in three passes:
  1. every tile's own sprite;
  2. the A\* result: **`searchedTiles` lit purple**, then **`pathTiles` lit blue** on top;
  3. the selection (cyan) and hover (white) highlights, as see-through boxes with inward outlines.
- `importBoard()` / `exportBoard()` read and write `maps/<name>.txt`.
- `getTile(x, y)`, `calcNeighbors()`, `recalculateNav()`: the merged **NavGraph**. Every non-wall tile becomes a `vertex` holding
  the coordinates of its walkable neighbours (all 8 directions). Look one up with `nav.getVertex(x, y, v)` and read `v.adjacent`.
- `pathfinding(nodeA, nodeB)`: **A\***. It returns the tile indices from start to end (both included), or an empty list if there is
  no path. It also records every tile it evaluated (its CLOSED set, in order) in `searchedTiles`. `getDistance(a, b)` is its cost
  function: 10 per straight step, 14 per diagonal step.

**`Character` (character.h/.cpp)**: Moves by velocity and acceleration with friction, and is clamped to the window.
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

## A\* path planning

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
`Board::pathfinding()` in `src/Tile.cpp` follows the pseudocode above line by line, using the names from my original stub:
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
- There's no way to save without closing (**E** saves and quits).
