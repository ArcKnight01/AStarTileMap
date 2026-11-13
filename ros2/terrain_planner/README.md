# terrain_planner (ROS 2 wrapper, not used yet)

The ROS 2 Jazzy side of [ROS2_PLAN.md](../../ROS2_PLAN.md): elevation map → traversability → cost map → A\* → waypoints.
The planning itself is the repo's `core/` (built from `../../core`, unchanged). This package only converts messages
and parameters. Nothing in the SFML app's build uses it.

| Node | In | Out |
|---|---|---|
| `cost_map_node` | `elevation_map` (`grid_map_msgs/GridMap`) | `cost_map` (GridMap, every layer), `cost_map_grid` (`OccupancyGrid`) |
| `planner_node` | `cost_map`, `goal_pose`, TF cost-map frame → `base_link` | `planned_path`, `waypoints` (`nav_msgs/Path`), `searched_cells` |
| `gridmap_file_publisher` | a `maps/*.gridmap` file | `elevation_map` or `cost_map`, a static TF at the start cell, `goal_pose` at the end cell |

## Build

```bash
sudo apt install ros-jazzy-grid-map        # grid_map_ros, grid_map_msgs, grid_map_rviz_plugin
mkdir -p ~/ros2_ws/src && ln -s ~/Repos/CppRepos/AStarTileMap/ros2/terrain_planner ~/ros2_ws/src/
cd ~/ros2_ws && source /opt/ros/jazzy/setup.bash
colcon build --packages-select terrain_planner && colcon test --packages-select terrain_planner
```

With conda active, colcon picks conda's Python and fails with `No module named 'catkin_pkg'`: run `conda deactivate`
first (or `conda config --set auto_activate_base false`).

The symlink keeps `core/` in one place (CMake follows it back to the repo). Elsewhere: `-DTERRAIN_PLANNER_REPO_DIR=<repo>`.

## Run

```bash
ros2 launch terrain_planner test_with_file.launch.py map:=hybrid   # no robot: a map file + RViz
ros2 launch terrain_planner planner.launch.py elevation_map:=<the mapper's topic>
```

## Different from the plan

- `core/` is untouched (plan §4 not done). The wrapper handles the map's position in the world itself (`MapPlacement`),
  and moves a start that lies in a lethal inflation to the nearest walkable cell (`start_search_radius`).
  Pruning and waypoints live here in `Waypoints.cpp` (plain C++) instead of in `core/`.
- No `map_frame` parameter: paths are published in the cost map's own frame.
