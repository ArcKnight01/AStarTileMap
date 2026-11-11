# AStarTileMap

A 20×20 tile-map editor in C++ with SFML. You draw walls, place a start and an end tile, and drive a small
character around the map. It is meant to become an **A\* path-planning visualizer**: start in green, goal in red,
and the planned path lit up in blue.

This repo merges my two old Visual Studio projects into one CMake project that builds on Linux (and Windows/macOS):

| Source | Date | What came from it |
|---|---|---|
| `TileMap` | Dec 2020 | The base: map editor, character, textures, maps. This is the newer version. |
| `OriginalTileMap` | Nov 2020 | Nothing extra. It differs from TileMap by only two lines. |
| iD Tech `C4-Template` / my `newProjectiles` | 2019–20 | `NavGraph` (walkable tiles + their neighbours), now in `src/NavGraph.h` and `Board::recalculateNav()` |

**A\* is not implemented yet.** `Board::pathfinding()` is still the original stub. See [Adding A\*](#adding-a-path-planning).

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
The program first asks **in the terminal** for a map name. Type `dungeon` (or `a`, `default`, `Unamed_Map`) and press
Enter, or press Enter alone to load `default`. Then the window opens.
A name that doesn't exist yet gives you a new empty map with a wall border. Press **E** to save it under that name.

### From VS Code
1. Open this folder in VS Code. Install the recommended **C/C++** and **CMake Tools** extensions if asked.
2. When CMake Tools asks for a *kit*, pick **GCC**.
3. Click **Build** in the bottom status bar, then **▷ Run** (or the 🐞 button to debug with breakpoints).
4. The map-name prompt appears in VS Code's **Terminal** panel. Type the name there.

Run it from inside `build/`, as both setups above do. CMake links `build/Textures` and `build/maps` back to this repo, so
maps you save end up in `maps/`.

---

## Controls

| Input | Effect |
|---|---|
| **Left click** | Select a tile (cyan frame + light cyan tint; the tile underneath stays visible) |
| **Space** | On the selected tile: ground ↔ wall. A start or end tile turns back into ground. |
| **Right click** | Select a tile and toggle it at once (like left click + Space) |
| **Enter** | On the selected ground tile: place the **start** (green), or the **end** (red) if a start already exists. On a start or end tile: remove it. |
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
  Tile.h/.cpp     class Board: the grid, drawing, editing, load/save, NavGraph, pathfinding() stub
  NavGraph.h      vertex + NavGraph types (from the iD Tech template)
  character.h/.cpp  class Character: the player you drive with WASD
assets/Textures/  tile sprites
maps/             saved maps
```

**`main.cpp`**: Opens a 1000×1000 window and builds a 20×20 `Board`, so each tile is 50 px. It asks for a map name and spawns
the `Character` on the start tile (or the first empty tile). Each frame it:
1. Reads input and sets flags on the board (`clickPos`, `spacePressed`, `enterPressed`…) or the character (`moveUp`…).
2. Calls `Board::convertTile()`, which applies the Space or Enter edit to the selected tile.
3. If the map changed (`navDirty`), calls `Board::recalculateNav()` to rebuild the navigation graph. This is where A\* should run.
4. Draws the board, then updates and draws the character.

**`Board` (Tile.h/.cpp)**
- `tileTypes`: one `tiletype` per tile (`wall`, `ground`, `startnode`, `endnode`, `checkpoint`). Tile `i` is at column `i % 20`, row `i / 20`.
- `getTileN(x, y)` converts pixels to a tile index; `getTilePos(i)` converts a tile index to the pixel position of its top-left corner.
- `drawBoard()` works in three passes: (1) every tile's own sprite, (2) **every tile in `pathTiles` lit blue**, (3) the selection
  (cyan) and hover (white) highlights, as see-through boxes with inward outlines.
- `importBoard()` / `exportBoard()` read and write `maps/<name>.txt`.
- `getTile(x, y)`, `calcNeighbors()`, `recalculateNav()`: the merged **NavGraph**. Every non-wall tile becomes a `vertex` holding
  the coordinates of its walkable neighbours (all 8 directions). Look one up with `nav.getVertex(x, y, v)` and read `v.adjacent`.
- `pathfinding(a, b)`: the original A\* stub. It only puts the start node in OPEN and prints it.

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
| `light_red/orange/yellow/green/blue/purple.png` | coloured lights | **`light_blue` = path.** The rest are free for the open set, closed set, a second agent, etc. |

---

## Adding A\* path planning

### Evidence of what I originally planned
- **The stub has been there since day one.** `Board::pathfinding()` is in the very first commit of both repos (OriginalTileMap on 2020‑11‑17,
  TileMap on 2020‑12‑08). It has `OPEN` / `CLOSED` lists and Lague-style costs: `distBtwnAdjTiles` (10 units straight) and
  `× sqrt(2)` ("~= 14 units") diagonal. The constructor calls it as a test (`pathfinding(getTilePos(1), getTilePos(9))`).
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

### Suggested steps in this code
1. **Per-tile node data.** Add arrays to `Board`, each of size `tileCount`: `gCost`, `hCost`, `parent` (tile index, -1 = none), and
   `inOpen` / `inClosed` flags. F is `gCost + hCost`.
2. **Neighbours.** Use the merged NavGraph: `nav.getVertex(x, y, v)`, then `v.adjacent`. Convert with `i = y*20 + x` and `x = i % 20`, `y = i / 20`.
3. **Fix the stub.** Its signature is `pathfinding(Vector2f a, Vector2f b)`, but `nodeB` is computed from `positionA`. It's simpler as
   `std::vector<int> pathfinding(int startIdx, int endIdx)` returning tile indices.
4. **Call it in the right place.** Remove the test call from the `Board` constructor (it runs before any map is loaded). In `main.cpp`,
   inside the `if (GameBoard.navDirty)` block, run it when both ends exist:
   ```cpp
   if (GameBoard.startExists && GameBoard.endExists)
       GameBoard.pathTiles = GameBoard.pathfinding(GameBoard.startPos, GameBoard.endPos);
   else
       GameBoard.pathTiles.clear();
   ```
   `drawBoard()` already lights `pathTiles` blue, so the path appears as soon as you place the end, and it updates as you edit walls.
5. **Retrace.** Follow `parent` from the end back to the start, then reverse the list.
6. **Visualize the search (optional).** Draw OPEN tiles with `light_green`/`light_orange` and CLOSED with `emptyTile` or
   `light_purple`. To animate it, run one loop iteration per frame instead of the whole search at once.
7. **Corner cutting.** `calcNeighbors()` allows diagonals even between two walls that touch at a corner. For a robot, skip a diagonal
   step when either of the two straight tiles beside it is a wall.
8. **Speed.** For 400 tiles a linear scan of OPEN is fine. Use a `std::priority_queue` (Lague's heap episode) for bigger maps.
9. **Follow the path.** Steer the `Character` toward the centre of the next path tile instead of reading WASD.

---

## Known issues (inherited from TileMap, left unchanged)
- `main.cpp` reads mouse buttons through `event.key.code` instead of `event.mouseButton.button`. It works only because both fields sit at
  the same memory offset. The compiler warns about it.
- `getTileN()` / `getTilePos()` use `tilesPerCol` where they should use `tilesPerRow`. This is fine while the map is square.
- `Character::updateNavigation()` doesn't bounds-check its 8 neighbours. On the outer row it reads outside `tileTypes`, which the wall border normally hides.
- `Character::move()` slows down leftward and rightward motion differently. The negative-x branch adds friction twice and checks `moveUp/moveDown`.
- There's no way to save without closing (**E** saves and quits).
