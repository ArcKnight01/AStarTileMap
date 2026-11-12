# Plan: turning `core/` into a ROS 2 C++ package

How the `costmap` branch's planning core becomes the **traversability → cost map → A\* → waypoints** part of the Go2 stack
(EECE 5554). Nothing here is built yet; this is the plan.

Where it sits, from the project's low-level dataflow diagram (`go2_5554_lowlevel_dataflow`):

```
VINS estimator ──── pose (SE(3) + covariance) ─────────────────────────────┐
                                                                            ▼
Elevation mapping ── filled elevation map (2.5D: height / variance) ──▶ [THIS PACKAGE] ──▶ waypoints (x, y, yaw + time)
 (own implementation)                                                    traversability       ──▶ quadruped_ros2_control (Go2)
                                                                         cost map, A*
```

Target: **ROS 2 Jazzy** (what is installed in `/opt/ros/jazzy`), C++17, `ament_cmake`.

---

## 1. The rule: `core/` stays plain C++

`core/` has no SFML and no ROS, and that stays true. The ROS side is a thin layer that:
1. converts messages into a `GridMap` (and back),
2. reads parameters into a `CostMapParameters`,
3. calls the same functions the SFML app calls (`runCostMapPipeline`, `findPath`),
4. publishes the results.

So the planner can still be developed and tested in the SFML app and in `tests/test_costmap.cpp`. The ROS package just uses
the same files. No renaming: the code stays in this repo's C++ style and names.

---

## 2. Package layout

One package to start, `terrain_planner`, with the core as its own library target (it can be split into a separate
`terrain_planner_core` package later if another package needs the core without ROS).

```
ros2_ws/src/terrain_planner/
  package.xml
  CMakeLists.txt
  core/                         ← this repo's core/, copied in or added as a git submodule / symlink (see §9)
  include/terrain_planner/
    GridMapConversion.h         ← grid_map_msgs/GridMap ↔ GridMap
    ParameterLoading.h          ← ROS parameters ↔ CostMapParameters
    Waypoints.h                 ← path (cells) → poses with heading
  src/
    CostMapNode.cpp             ← elevation map in, cost map out
    PlannerNode.cpp             ← cost map + robot pose + goal in, path / waypoints out
    GridMapFilePublisher.cpp    ← publishes a maps/*.gridmap file as if it were the elevation mapper (testing)
    GridMapConversion.cpp
    ParameterLoading.cpp
    Waypoints.cpp
  config/
    cost_map.yaml               ← every CostMapParameters field (same defaults as core/CostMapParameters.h)
    planner.yaml
  launch/
    planner.launch.py           ← both nodes (on the robot)
    test_with_file.launch.py    ← file publisher + both nodes + RViz, no robot needed
  rviz/terrain_planner.rviz
  maps/                         ← the example .gridmap files (flat, rugged, dungeon, hybrid)
  test/
    test_costmap.cpp            ← the existing core tests, run by colcon test
    test_conversion.cpp         ← GridMap ↔ message round trips, frames and indexing
    test_waypoints.cpp
```

### `CMakeLists.txt` (outline)
```cmake
cmake_minimum_required(VERSION 3.16)
project(terrain_planner)
set(CMAKE_CXX_STANDARD 17)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(grid_map_ros REQUIRED)      # grid_map_msgs + the converter
find_package(nav_msgs REQUIRED)          # Path, OccupancyGrid
find_package(geometry_msgs REQUIRED)     # PoseStamped (goal)
find_package(tf2_ros REQUIRED)           # robot pose: map -> base_link

# the core: no ROS dependencies at all
add_library(costmap_core STATIC core/GridMap.cpp core/GridMapFile.cpp core/TerrainFeatures.cpp
  core/Traversability.cpp core/CostMap.cpp core/TerrainGenerator.cpp core/PathPlanner.cpp)
target_include_directories(costmap_core PUBLIC core)
set_target_properties(costmap_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# the ROS layer, as components (can run as separate processes or in one)
add_library(terrain_planner_nodes SHARED src/CostMapNode.cpp src/PlannerNode.cpp src/GridMapFilePublisher.cpp
  src/GridMapConversion.cpp src/ParameterLoading.cpp src/Waypoints.cpp)
target_include_directories(terrain_planner_nodes PUBLIC include)
target_link_libraries(terrain_planner_nodes costmap_core)
ament_target_dependencies(terrain_planner_nodes rclcpp rclcpp_components grid_map_ros nav_msgs geometry_msgs tf2_ros)
rclcpp_components_register_node(terrain_planner_nodes PLUGIN "terrain_planner::CostMapNode" EXECUTABLE cost_map_node)
rclcpp_components_register_node(terrain_planner_nodes PLUGIN "terrain_planner::PlannerNode" EXECUTABLE planner_node)
rclcpp_components_register_node(terrain_planner_nodes PLUGIN "terrain_planner::GridMapFilePublisher" EXECUTABLE gridmap_file_publisher)

install(TARGETS terrain_planner_nodes costmap_core LIBRARY DESTINATION lib ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY config launch rviz maps DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_conversion test/test_conversion.cpp)
  target_link_libraries(test_conversion terrain_planner_nodes)
  # the existing core tests are a plain executable that returns non-zero on failure
  add_executable(test_costmap test/test_costmap.cpp)
  target_link_libraries(test_costmap costmap_core)
  add_test(NAME test_costmap COMMAND test_costmap WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
ament_package()
```

Dependencies: `sudo apt install ros-jazzy-grid-map` (grid_map is released for Jazzy).

---

## 3. Nodes and topics

Two nodes, written as **components** so they can run in one process (no copy of the map between them) or separately while
debugging.

### `cost_map_node`: elevation map → cost map
| | Topic | Type | Notes |
|---|---|---|---|
| in | `elevation_map` | `grid_map_msgs/GridMap` | from the elevation mapper; layers `elevation` (+ `variance`) |
| out | `cost_map` | `grid_map_msgs/GridMap` | every layer: `elevation`, `slope`, `roughness`, `step`, `traversability`, `cost` |
| out | `cost_map_grid` | `nav_msgs/OccupancyGrid` | the cost layer for RViz (0–100, lethal = 100, unknown = −1) |

On each new elevation map: convert → `runCostMapPipeline(map, parameters)` → publish. If maps arrive faster than the pipeline
runs, only the newest is processed.

### `planner_node`: cost map + pose + goal → path
| | Topic / source | Type | Notes |
|---|---|---|---|
| in | `cost_map` | `grid_map_msgs/GridMap` | uses only the `cost` layer, so a cost map from anywhere works (as in the app) |
| in | TF `map → base_link` | | robot position = the start cell (from the VINS pose) |
| in | `goal_pose` | `geometry_msgs/PoseStamped` | RViz's "2D Goal Pose" publishes this |
| out | `planned_path` | `nav_msgs/Path` | every cell of the A\* path (for RViz) |
| out | `waypoints` | `nav_msgs/Path` | pruned and sampled poses with heading and time (§6): what goes to the controller |
| out | `searched_cells` | `visualization_msgs/MarkerArray` | optional: the purple "searched" cells, like the app |

It re-plans when a goal arrives, and when a new cost map arrives (rate-limited by `replan_rate`, e.g. 2 Hz).

Parameters: `algorithm` (`astar` default; `dijkstra`, `bfs`, `dfs` for comparisons), `replan_rate`, `map_frame`,
`robot_frame`, `waypoint_spacing`, `goal_tolerance`.

### `gridmap_file_publisher`: testing without the robot
Loads a `.gridmap` file (`loadGridMap`) and publishes it on `elevation_map` (or straight onto `cost_map` for maps like
`dungeon` that start at the cost layer), plus a fixed `map → base_link` TF at the file's start cell. This lets the whole ROS
side run on the maps from `tools/generate_maps.cpp` before the elevation mapper exists.

---

## 4. Changes needed in `core/` first

These are small and keep `core/` ROS-free. Each one also makes the SFML app and its tests better.

1. **Where the map is in the world.** Today `GridMap` is centred on (0, 0). The elevation map is robot-centric and moves
   with the robot, so add the map centre's position in the map frame (`setPosition(x, y)` / `getPosition()`, as in
   grid_map). `getPosition(column, row, …)` and `getCell(x, y, …)` then add / subtract it.
2. **The uncertainty feature.** The diagram includes `Uncertainty (variance)` in the traversability features. Add a
   `computeUncertainty()` (variance → score, with a `criticalVariance` parameter and a weight) to `Traversability`, used
   only when the map has a `variance` layer.
3. **A start inside an inflated area.** When the robot stands within `inflationRadius` of something lethal, its own cell is
   lethal and A\* finds no path. Allow the start cell (and optionally its neighbours) even when lethal, or move the start to
   the nearest walkable cell.
4. **Path pruning and waypoints** (Rajput Algorithm 3) as a core function, so the app can show them too (§6).
5. **Timing.** A `std::chrono` measurement around the pipeline and the planner in the tests, to know the budget per map
   size.

---

## 5. Converting `grid_map_msgs/GridMap` ↔ `GridMap`

This is the part most likely to have bugs, so it gets its own tests.

- grid_map stores layers as Eigen matrices with its own index order (index (0, 0) is the corner with the largest x and y,
  and the map can be a circular buffer after it moves). Our `GridMap` is row-major with row 0 on top.
- Plan: use `grid_map::GridMapRosConverter::fromMessage()` to get a `grid_map::GridMap`, then fill our cells **by
  position**: for every cell, `getPosition(column, row, x, y)` → `grid_map.atPosition(layer, {x, y})`. This doesn't depend
  on either library's index order, and it handles the circular buffer.
- The reverse (`toMessage`) does the same: set each grid_map cell from our cell at the same position.
- Units: metres and the map frame on both sides. NaN stays NaN (unknown).
- **Tests** (`test_conversion.cpp`): put a known feature at a known world position (one raised cell at (+1 m, 0)), convert
  both ways, and check it lands in the same place. Check that "up" in the `y` direction matches. Check non-square maps,
  a moved map, and NaN cells.

---

## 6. From path to waypoints (SE(2) poses)

The controller wants `(x, y, yaw)` waypoints with times, not a list of 10 cm cells.

1. **Prune** (Rajput Algorithm 3): drop cells that lie on a straight line between their neighbours, and shortcut corners
   where the straight segment crosses no lethal cell and costs no more than the original path.
2. **Sample** every `waypoint_spacing` metres (e.g. 0.3–0.5 m, about the Go2's body length).
3. **Heading:** `yaw = atan2(y[i+1] − y[i], x[i+1] − x[i])`; the last waypoint takes the goal's yaw.
4. **Time:** distance / a nominal speed, slower on costlier cells (a scaling that can be tuned later).
5. Publish as `nav_msgs/Path` in `map_frame`.

How exactly `quadruped_ros2_control` takes the waypoints (a path topic, an action, or velocity commands from a follower) is
still open (§10). The `waypoints` topic is the interface either way.

---

## 7. Parameters

`config/cost_map.yaml` holds every field of `CostMapParameters` with the same defaults, so the app, the tests and the robot
agree:

```yaml
cost_map_node:
  ros__parameters:
    slope_radius: 0.15
    roughness_radius: 0.15
    step_radius: 0.10
    critical_slope: 0.40          # rad (ETH default 1.0; lower for sand)
    critical_roughness: 0.05      # m (ETH default)
    critical_step: 0.12           # m (ETH default)
    weight_slope: 0.333
    weight_roughness: 0.333
    weight_step: 0.333
    zero_if_any_critical: false
    traversability_threshold: 0.5 # Rajput
    lethal_cost: 60.0             # Rajput
    inflation_radius: 0.2         # Go2 half-width + margin
    unknown_policy: lethal        # lethal | fixed_cost | free
    unknown_cost: 0.5
    cost_weight: 1.0
```

`ParameterLoading` declares each one, reads it into a `CostMapParameters`, and registers a parameter callback, so values can
be tuned live (`ros2 param set /cost_map_node critical_slope 0.5`) and the next map uses them.

---

## 8. Testing, in order

1. **Core tests as now** (`colcon test`): the pipeline and planners against brute force.
2. **Conversion tests** (§5).
3. **Files through ROS:** `ros2 launch terrain_planner test_with_file.launch.py map:=hybrid`. In RViz, set a goal, check the
   path goes through the middle pass like in the app, and compare the path cost with the app's panel.
4. **A rosbag** of the elevation mapper's output from the Go2 on sand, replayed into `cost_map_node`, to tune the critical
   values on real data.
5. **On the robot:** first only publish and watch the path in RViz while driving by hand, then hand the waypoints to the
   controller.

---

## 9. Sharing `core/` between this repo and the ROS package

Pick one:
- **Git submodule / subtree** of this repo in the package (one source of truth; needs `git submodule update`).
- **Symlink** `core/` into the workspace while developing (quickest; doesn't survive a fresh clone).
- **Move `core/` into the ROS package** and have this repo's CMake point at it (the app becomes a viewer for the package's
  core).

Recommended: start with a symlink while building the ROS side, then switch to a submodule once the interface settles.

---

## 10. Open questions

- **The elevation map's message:** does the own-implementation elevation mapper publish `grid_map_msgs/GridMap`
  (recommended; the cost map node expects it), and with which layer names (`elevation`, `variance`)?
- **Frames:** the names of the map and robot frames from the VINS estimator (`map` / `odom` / `base_link`?), and whether
  the elevation map is in `map` or `odom`.
- **Resolution and size** of the elevation map (this sets the feature radii: they must cover at least one cell).
- **The controller's interface** for the waypoints (`quadruped_ros2_control`): topic, action, or a path follower in between.
- **Nav2:** this could instead be a Nav2 global-planner plugin (`nav2_core::GlobalPlanner`) plus a costmap layer, which
  brings recovery behaviours and controllers. But Nav2 costmaps are 8-bit (0–254), so the cost map would be quantised.
  Plan: standalone nodes first, then a plugin wrapper if Nav2 is used.

---

## 11. Steps

| # | Step | Done when |
|---|---|---|
| 1 | `core/` changes from §4 (map position, start-in-inflation, uncertainty, pruning / waypoints), with tests | `test_costmap` passes; the app still works |
| 2 | Package skeleton: `package.xml`, CMake, `costmap_core` builds with `colcon build` | `colcon test` runs the core tests |
| 3 | `GridMapConversion` + its tests | round trips put features in the same place |
| 4 | `gridmap_file_publisher` + `cost_map_node` + RViz config | the `rugged` map's cost layer shows in RViz |
| 5 | `planner_node` (goal from RViz, start from TF) | a path appears in RViz and matches the app's |
| 6 | Waypoints with heading | `waypoints` topic shows arrows in RViz |
| 7 | Hook up the real elevation mapper and VINS (rosbag first) | cost map follows the robot |
| 8 | Hand the waypoints to the Go2 controller | the robot walks the path |
