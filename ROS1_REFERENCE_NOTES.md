# ROS 1 reference notes: husky-stable_ws and M4_Simulation

How two earlier ROS 1 (Noetic) projects went from the robot's sensing to its motors: elevation map → cost map → A* →
waypoints → controller. They are notes for comparison while building **this project's ROS 2 Jazzy** pipeline
(`ROS2_PLAN.md`); no code is taken from either repository. Read from the source on 2026-11-12.

| | Where | ROS |
|---|---|---|
| **husky-stable_ws** (Frank Laubach and Aidan; Aidan wrote code for the scripts and sent the changes to Frank over Slack, without push access, so git credits every commit to FrankSLaubach) | `~/Repos/ROS1/husky-stable_ws` | ROS 1 Noetic, Gazebo |
| **M4_Simulation** (Rohit Rajput; thesis "Towards Autonomous Multi-Modal Mobility Morphobot (M4) Robot", 2023, arXiv 2308.13972, `RohitThesis.pdf` in that folder) | `~/Repos/ROS1/M4_Simulation`, package `grid_3d_nav` | ROS 1 Noetic, Gazebo + ArduPilot SITL |
| **This project** (for comparison) | `core/`, `ros2/terrain_planner` | **ROS 2 Jazzy** |

## At a glance

| Stage | husky-stable_ws | M4_Simulation | This project (ROS 2 Jazzy) |
|---|---|---|---|
| Elevation map | none (Gazebo terrain only) | `elevation_mapping_cupy`: 4 depth cameras + lidar → `grid_map_msgs/GridMap`, 0.12 m, 25 m, 5 Hz | from the elevation mapper on `elevation_map` (`grid_map_msgs/msg/GridMap`) |
| Traversability | none | `elevation_mapping_cupy`'s learned filter (`weights.dat`) | slope / roughness / step from the elevation (Wermelinger et al. 2016) |
| Cost map | none | thesis Algorithm 1 → `/elevation_mapping/costmap` at 1 Hz (publisher not in the checkout) | Algorithm 1, ground-only (`runCostMapPipeline`) → `cost_map` |
| Planner | none | `3d_astar.py`: 8-connected A* with a walk / fly flag per cell | 8-connected A* (eq. 3.8 step cost), also Dijkstra / BFS / DFS |
| Path message | – | custom `grid_3d_nav/GlobalPath` (pose + `flag`), every cell | `nav_msgs/msg/Path` on `planned_path` |
| Waypoints | typed on the command line: `[x,y]` walk, `[x,y,z]` fly | every A* cell (pruning written but not called); no heading or time | pruned (thesis Algorithm 3 + cost rule), ≤ 0.4 m apart, heading, arrival time → `waypoints` |
| Follower | bang-bang heading controller → `/cmd_vel`; ArduCopter for flight | bang-bang turn-then-drive → wheel `cmd_vel`; MAVROS mission for flight | open (ROS2_PLAN §10) |
| Low-level control | OCS2 nonlinear MPC + whole-body control (`legged_control` fork) | `diff_drive_controller` (wheels); ArduPilot (flight) | `quadruped_ros2_control` (Go2) |
| Robot position | `/gazebo/model_states` (exact, from the simulator) | `/my_odom` (Gazebo p3d plugin, exact, 50 Hz) | TF `map → base_link` (VINS pose) |

Neither ROS 1 project uses a standard `nav_msgs/Path` end to end, and neither gives its waypoints headings or times.

---

## husky-stable_ws (Frank Laubach and Aidan)

A legged robot with thrusters (the Husky), in Gazebo. Walking uses Frank's fork of Qiayuan Liao's `legged_control`
(OCS2 MPC + whole-body control); flying uses ArduCopter through MAVLink. **There is no perception or planning**: no
elevation mapping, no cost map, no A*. The chain starts at hand-written waypoints.

Packages (`src/`): `mpc_legged_control` (FrankSLaubach's fork of `legged_control`), `husky-gazebo/husky_description`
(the robot, worlds and the control scripts below; written by Frank and Aidan, with Aidan's changes committed by Frank), `gazebo_terrain_tutorial_` (Aidan's terrain world), and the libraries
`ocs2`, `pinocchio`, `hpp-fcl`, `ocs2_robotic_assets`, `roboticsgroup_gazebo_plugins`.

### 1–3. Elevation map, cost map, planner

None. The terrain is a Gazebo world; the scripts never look at it.

### 4. Waypoints: the command line

```bash
rosrun husky_description unified_waypoint_control.py [3,0] [3,3,1.5] [0,0] [0,1.5,0]
```

- Each argument is a waypoint in the Gazebo world frame (metres): `[x,y]` is walked to, `[x,y,z]` is flown to at height
  `z`.
- The **last** argument is not a waypoint: it is one marker height per waypoint, used only to spawn Gazebo markers
  (`spawn_marker.py`: red on the ground, blue in the sky).
- The waypoints are also published one by one on `/husky/waypoints` (`geometry_msgs/Point`).
- `Test Code/waypoint_walker.py` (an earlier version) hard-codes `[[3, 0], [3, 3], [0, 0]]`.

This project's EXPORT → **Waypoints (list)** writes the same `[x,y] [x,y] ...` form (append the marker heights yourself).

### 5. Mode logic: `overall_control` in `unified_waypoint_control.py`

A state machine over the waypoints: **Standby**, **WBC** (walking) and **Ardupilot** (flying).

- Next waypoint is 2-D: warm up the whole-body controller (`warmup_WBC`, as `control_wbc_start.py`), mode WBC, walk.
- Next waypoint is 3-D: cool the WBC down (`cooldown_WBC`) if walking, mode Ardupilot, take off, fly, land.

### 6. Walking follower: `walking_control.go_to_waypoint()`

A **bang-bang** heading controller (fixed speeds switched by thresholds, no proportional term), at 100 Hz:

| Heading error | Forward | Turn |
|---|---|---|
| > `turn_window` 0.5 rad | 0.09 m/s (`null_speed`) | 0.1 rad/s |
| < `safe_window` 0.26 rad | 0.19 m/s | 0.05 rad/s toward the target |
| between | 0.19 m/s | 0.05 rad/s (same as inside `safe_window`: the middle band changes nothing) |

- Arrived when `|dx| < 0.4` and `|dy| < 0.4` m (a box, not a circle).
- Position and yaw come from `/gazebo/model_states`: the simulator's exact pose, not an estimate.
- Output: `/cmd_vel` (`geometry_msgs/Twist`).

### 7. Gait: `gait_switcher.py`

`/keyboard/gait` (`std_msgs/String`, e.g. from `keyboard_control.py` or the waypoint script) → an OCS2 mode schedule on
`/legged_robot_mpc_mode_schedule` (`ocs2_msgs/mode_schedule`), which OCS2's `GaitReceiver` in the controller applies.

### 8. Legged controller: `mpc_legged_control/legged_controllers`

- `TargetTrajectoriesPublisher` turns commands into a target trajectory for the MPC:
  - `/cmd_vel`: the velocity is rotated into the world frame and integrated over a fixed look-ahead time
    (`timeToTarget`) to a target pose.
  - `/move_base_simple/goal` (`geometry_msgs/PoseStamped`, as RViz's goal tool publishes): a target pose at standing height,
    reached in max(distance / `targetDisplacementVelocity`, turn / `targetRotationVelocity`).
  - The Husky's `reference.info`: `targetDisplacementVelocity` **0.05 m/s**, `targetRotationVelocity` **0.1 rad/s**,
    `comHeight` 0.40 m (the A1 / Aliengo configs use 0.5 m/s and 1.57 rad/s).
- `LeggedController` runs OCS2's nonlinear MPC (gait from the mode schedule) and the whole-body controller
  (`legged_wbc`), which computes the joint torques; `legged_gazebo` / `legged_hw` connect it to Gazebo.

### 9. Flying: `arducopter_control`

pymavlink to ArduCopter (SITL, through MAVProxy):

- Take off: GUIDED, arm, `MAV_CMD_NAV_TAKEOFF` to the waypoint height (at least 1 m), retried until the robot has risen
  0.5 m.
- Move: the waypoint's `dx, dy` from the start are turned into latitude / longitude through UTM, then sent as a GUIDED
  target; arrived within **2 m**.
- Land: LAND, wait until the height stops changing, force-disarm.

---

## M4_Simulation (Rohit Rajput)

The M4 morphobot (wheels that become rotors) in Gazebo with ArduPilot. Launched by `grid_3d_nav/launch/nav.launch`:
`3d_astar.py` → `mulitimodal_path_publisher.py` → `nav_path_republisher.py` → `simple_follower_2d.py` (ground) and
`aerial_follower.py` (flight).

### 1. Elevation map: `elevation_mapping_cupy`

- GPU elevation mapping (Miki et al., ETH), configured in `elevation_mapping_cupy/config/parameters.yaml`.
- Input: point clouds from four depth cameras (`/depth_camera_{front,rear,left,right}/depth/color/points`) and a lidar
  (`/robot_self_filter/bpearl_front/point_cloud`). The thesis: point clouds at 30 Hz, map out at 5 Hz.
- Map: 0.12 m cells, 25 m square, in the `odom` frame, centred on the robot's `base` frame.
- Published as `grid_map_msgs/GridMap`: `elevation_map_raw` (layers `elevation`, `traversability`, `variance`, 5 Hz), plus a
  recordable copy and a filtered map (`min_filter`, `smooth`, `inpainting`, `upper_bound`).
- Traversability: `elevation_mapping_cupy`'s learned filter (`weights.dat`, PyTorch), not computed by the M4 code.

### 2. Cost map: thesis Algorithm 1

From the elevation and traversability layers, published at 1 Hz on `/elevation_mapping/costmap`
(`grid_map_msgs/GridMap`). The code that publishes it is **not in this checkout** (no publisher of that topic in `src/`);
the thesis describes it:

- traversability < 0.5 → cost `Le + elevation · 0.01` with `Le = 60` (the locomotion-mode energy ratio): a "fly" cell,
  slightly costlier where higher;
- otherwise → cost `1 − traversability`;
- inflation: around every cell above 60, the neighbourhood takes the larger of its value and that cell's value.

(This project's cost map follows the same algorithm, ground-only: below the threshold is lethal, never "fly".)

### 3. Planner: `3d_astar.py`

- Subscribes to `/elevation_mapping/costmap`, `/my_odom` (start = the robot's position) and `move_base_simple/goal`
  (RViz's goal tool); plans once per new goal, on the next cost map.
- Reads the message's layers **by position**: `data[0]` as elevation, `data[1]` as cost (it breaks if the layer order
  changes).
- 8-connected A*, `h` = straight-line distance. Step cost:
  - walk: `g + h(current, neighbour) + cost(neighbour)`;
  - fly (only where the cost is **exactly 60**): `g + h + 60 · Δelevation_cells + 1 + 60`, at height
    `max elevation around the cell + 1 m` clearance.
- Because fly cells cost `60 + elevation · 0.01`, the exact `== 60` test only matches fly cells at elevation 0.
- Goal reached within 0.2 m; unknown (NaN) elevation counts as 100 m.
- Unused: `robot_radius = 24`, and `change_in_elevation()` (it computes the same as the heuristic).

### 4. Path and waypoints

- Every A* cell is a waypoint, as a custom `grid_3d_nav/GlobalPath` on `/global/path`: a header and a list of
  `PoseStampedwithflag` (a `Header`, a `Pose` and `bool flag`: 0 walk, 1 fly).
- `take_off_landing_addition()` inserts a point straight above (takeoff) or below (landing) where the flag changes.
- `prune_path()` (removes collinear points with a determinant test) exists, but its call is commented out; the thesis
  describes pruning as Algorithm 3 (line-of-sight shortcuts after Noreen et al. 2019).
- No heading (the `atan2` loop is commented out, orientations stay empty) and no times.
- `test/path_waypoint_simplification.py` (Ramer–Douglas–Peucker) and `test/test_smoothing.py` are unfinished experiments.

### 5. Mode split: `mulitimodal_path_publisher.py`

Cuts the path where the flag changes and publishes each piece to `/ground/path` or `/aerial/path`, switching the robot's
mode in between (the hips folded through 90° for flight; `/m4nav/transform_state`). It waits for the robot to reach
the end of a piece before sending the next (thesis Algorithm 4). The arrival check compares `abs(goal) − abs(robot)`
per axis against 0.1 m, not the distance between them.

### 6. For RViz: `nav_path_republisher.py`

Copies the three custom paths into `nav_msgs/Path`, dropping the flag: `/m4nav/full_path`, `/m4nav/ground_path`,
`/m4nav/flight_waypoints`. It creates a new publisher inside every callback.

### 7. Ground follower: `simple_follower_2d.py`

Bang-bang, at 10 Hz, toward the next cell of `/ground/path`:

- heading error > 0.1 rad: stop and turn in place at ±0.8 rad/s;
- otherwise drive straight at 0.9 m/s; the next cell when within 0.1 m in x and in y.

Output: `/m4assembly/wheel_velocity_controller/cmd_vel` → a `diff_drive_controller`. With a waypoint every 0.12 m on an
8-connected path, it stops to turn at almost every staircase step.

### 8. Flight: `aerial_follower.py`

Listens to `/m4nav/flight_waypoints` (`nav_msgs/Path`) and MAVROS: sets mode `AUTO.MISSION`, arms, takes off to 10 m,
pushes every pose as a mission waypoint (`FRAME_LOCAL_NED`, `NAV_WAYPOINT`), and lands and disarms at the end.
Positions go in as they are: the map's x / y / z (east-north-up) are not converted to north-east-down, and
`AUTO.MISSION` is PX4's mode name (ArduPilot's is `AUTO`).

### 9. Robot position: `/my_odom`

A Gazebo p3d plugin in the URDF (`m4assembly.urdf`) publishes the exact pose of `base_link` in the `world` frame at
50 Hz; `odom2tf` turns it into TF `world → base_link`.

---

## What carries over to this project (ROS 2 Jazzy)

- **Kept as ideas, from the thesis (not the code):** the cost map (Algorithm 1, ground-only) and pruning
  (Algorithm 3, with its index slips fixed and a cost rule added).
- **Done differently:** standard messages throughout (`grid_map_msgs/msg/GridMap`, `nav_msgs/msg/Path`), so RViz and
  other tools read them with no republisher; layers read by name; waypoints pruned, spaced, with headings and arrival
  times; the robot's pose from TF (a real estimate), not the simulator.
- **The controller interface is still open** (ROS2_PLAN §10). Both ROS 1 followers are bang-bang turn-then-drive;
  husky-stable_ws's legged controller also accepts a goal pose (`/move_base_simple/goal`) or a velocity (`/cmd_vel`)
  and turns either into an MPC target trajectory, which is the kind of interface `quadruped_ros2_control` may offer.
