# A* comparison

Three versions of A* side by side:

1. **Sebastian Lague's pseudocode**, the reference this project's A* follows.
2. **The pasted Python A\***, Rajput-style: world coordinates, a heap, and a goal radius.
3. **This project's A\***: `pathfindingAstar()` in `core/PathPlanner.cpp`. The `terrain_planner` ROS 2 package
   (`ros2/terrain_planner`) and the SFML app both call it.

The notation is the same throughout: `g` = cost from the start, `h` = estimated cost to the goal, `f = g + h`.

---

## 1. Sebastian Lague: reference pseudocode

From "A* Pathfinding (E01: algorithm explanation)", <https://www.youtube.com/watch?v=-L-WgKMFuhE>. This is a paraphrase
of the video's pseudocode, with the structure and names kept.

```
OPEN   = set of nodes to be evaluated
CLOSED = set of nodes already evaluated
add start to OPEN

loop
    current = node in OPEN with the lowest f            (tie: lowest h)
    remove current from OPEN
    add current to CLOSED

    if current == target
        return                                          (retrace parents from target back to start)

    for each neighbour of current
        if neighbour is not traversable or neighbour is in CLOSED
            skip to the next neighbour

        if new path to neighbour is shorter or neighbour is not in OPEN
            g( neighbour ) = g( current ) + distance( current, neighbour )
            h( neighbour ) = distance( neighbour, target )
            parent( neighbour ) = current
            if neighbour is not in OPEN
                add neighbour to OPEN
```

In the video's code:
- `distance` is the integer **octile** distance on an 8-connected grid: 10 per straight step, 14 per diagonal,
  `14·min(dx, dy) + 10·(max − min)`.
- Cells are either walkable or not. A movement penalty per cell comes in a later episode (weights).
- OPEN starts as a list that is searched each step. A heap replaces it later (E04).

---

## 2. Pasted Python A* (Rajput-style)

```
open_list = heap of ( f, node ),  holding ( 0, start )
g( start ) = 0

while open_list is not empty
    current = pop the lowest f from open_list           (no CLOSED set, no check for an outdated entry)

    if euclidean( current, goal ) <= distance_threshold
        return the parents retraced from current         (stops near the goal; the goal itself is not added)

    for each neighbour of current                       (get_neighbors: 8-connected, on the map, cost != 60)
        g_new = g( current ) + euclidean( current, neighbour ) + costmap( neighbour )     (metres + cost)

        if neighbour has no g yet or g_new < g( neighbour )
            parent( neighbour ) = current
            g( neighbour ) = g_new
            f( neighbour ) = g_new + euclidean( neighbour, goal )
            push ( f( neighbour ), neighbour ) onto open_list

return []                                               (no path)
```

Nodes are **world positions** (tuples of metres). Each one is converted to a grid cell with `world_to_grid()` to read
the cost map, and dictionaries keyed by the tuple hold `g`, `f` and the parents.

---

## 3. This project's A* (`core/PathPlanner.cpp`)

```
nav = graph of the walkable cells, each with its 8 walkable neighbours      (lethal: cost >= lethalCost, or NaN)
if start or end is off the map or lethal
    return no path

OPEN   = [ start ],   h( start ) = euclidean( start, end )                  (cells)
CLOSED = { }

while OPEN is not empty
    current = cell in OPEN with the lowest f = g + h      (tie: lowest h; a linear scan of OPEN)
    remove current from OPEN, add it to CLOSED, record it as searched (purple)

    if current == end
        path = parents retraced from end, reversed
        cost = sum of the step costs along the path
        return path, cost, searched cells

    for each neighbour of current in nav
        if neighbour is in CLOSED
            skip

        g_new = g( current ) + step( current, neighbour )
              step( a, b ) = euclidean( a, b ) + costWeight · cost( b )      (Rajput eq. 3.8; cells, costWeight = 1)

        if g_new < g( neighbour ) or neighbour is not in OPEN
            g( neighbour ) = g_new
            h( neighbour ) = euclidean( neighbour, end )
            parent( neighbour ) = current
            if neighbour is not in OPEN
                add neighbour to OPEN

return no path
```

This is Lague's loop line for line. The only change is the step cost: distance plus the terrain cost of the cell
stepped into.

---

## 4. Differences

| | Lague | Pasted Python | This project |
|---|---|---|---|
| Node | grid cell | world position (metres), converted to a cell on every lookup | grid cell index |
| OPEN | list searched each step (heap later) | binary heap, with duplicate entries | list searched each step |
| CLOSED | yes | **no** | yes |
| Outdated heap entries | n/a | popped and expanded again | n/a |
| Tie-break on equal f | lowest h | the heap compares the node tuples (coordinates) | lowest h |
| Goal test | `current == target` | **within `distance_threshold`** of the goal | `current == end` |
| Step cost | octile distance (10 / 14) | `euclidean (m) + cost` | `euclidean (cells) + costWeight · cost` |
| Heuristic | octile | euclidean (m) | euclidean (cells) |
| Blocked cell | not walkable | `cost == 60` (exact) | `cost >= lethalCost` or unknown (NaN) |
| Start / end on a blocked cell | n/a | not checked | returns no path |
| Output | path | list of world points | path, path cost, searched cells |

### What matters most

1. **The units of distance change how much terrain cost counts.** The pasted code measures distance in metres, so one
   0.1 m step costs 0.1 of distance plus up to 0.5 of terrain cost. This project counts in cells: 1 per step plus up to
   0.5. So for the same map, the Python planner weighs terrain cost **10 times more** relative to distance, and takes
   longer detours around costly ground. To match it here, set `costWeight = 1 / resolution`, which is 10 for 0.1 m
   cells. Neither is wrong; the weight is a tuning choice. Just keep in mind that the two don't give the same paths.
2. **No CLOSED set and no check for outdated entries.** When a cell's `g` improves, the Python version pushes it again,
   and the old entry is popped and expanded later anyway. The path is still optimal, because the euclidean heuristic is
   consistent. But cells get expanded more than once, which is wasted work. The usual fix is
   `if current in closed: continue` right after the pop.
3. **Goal radius versus exact goal.** The Python version stops at the first cell within `distance_threshold` of the
   goal, so the path ends near the goal, not on it. That suits a waypoint follower with a tolerance. This project
   plans exactly to the end cell and leaves the tolerance to the waypoint follower (`core/Waypoints.h`).
4. **`cost == 60` versus `cost >= lethalCost`.** An exact equality test misses lethal values that aren't exactly 60,
   for example after interpolation or with a different `lethalCost`, and it lets unknown (NaN) cells through.
5. **Speed.** The heap is O(log n) per pop. This project's linear scan is O(n) per pop, which is fine at 72 × 72 cells
   and faithful to Lague. Switching to a heap (with a CLOSED check) is the efficient alternative if maps get large.
6. **Heuristic.** On an 8-connected grid, Lague's octile distance is a tighter lower bound than euclidean distance, so
   A* expands fewer cells. Both are admissible, so the paths are still optimal.

What all three have in common: 8-connected neighbours, and diagonal steps allowed between two blocked cells (corner
cutting).

---

## 5. Is `terrain_planner` a good name?

It's reasonable. The package builds a terrain cost map from elevation (`CostMapNode`) and plans on it
(`PlannerNode`), and the name says both. It also follows the ROS 2 `snake_case` package naming in `code_format.md`
section 6.2.1.

Alternatives, if you want the name to say more:

| Name | When it fits better |
|---|---|
| `terrain_planner` (keep) | the package stays cost map + planner + waypoints |
| `traversability_planner` | to stress that it plans on traversability / cost, not raw terrain |
| `terrain_navigation` | if path following or the gait / mode logic (as in the ROS 1 stack) moves into it |
| `terrain_costmap` + `terrain_planner` | if the cost-map node and the planner node are split into two packages |

`terrain_planner` is fine as long as the package doesn't also start following the path.
