#pragma once
#include <ostream>
#include <string>
#include <vector>
#include "CostMapParameters.h"
#include "GridMap.h"
#include "Waypoints.h"

// Exporting the map and the plan as ROS 2 Jazzy messages in YAML (the shape `ros2 topic echo` prints), for use on the
// ROS side (ROS2_PLAN.md), not by this app. Plain C++ (no SFML, no ROS), like the rest of core/.
//
//   what               message                        topic (ROS2_PLAN.md)
//   elevation map      grid_map_msgs/msg/GridMap      elevation_map   (writeGridMapMessage, "elevation" layer)
//   cost map           grid_map_msgs/msg/GridMap      cost_map        (writeGridMapMessage, every layer)
//   cost map for Nav2  nav_msgs/msg/OccupancyGrid     cost_map_grid   (writeOccupancyGridMessage)
//   path               nav_msgs/msg/Path              planned_path    (writePathMessage)
//   waypoints          nav_msgs/msg/Path              waypoints       (writeWaypointsMessage: stamp = arrival time)
//   start, goal        geometry_msgs/msg/PoseStamped  start_pose, goal_pose (writePoseStampedMessage)
//
// Positions are in metres in the map frame, with the map centred on (0, 0) (GridMap::getPosition), and every stamp is
// 0 except a waypoint's, which is its arrival time in seconds from the first waypoint (the ROS node adds now()). A pose's heading (yaw, radians, counter-clockwise from +x) is written as the quaternion
// ( 0, 0, sin( yaw / 2 ), cos( yaw / 2 ) ).

/**
 * Get the OccupancyGrid value of a cost: -1 unknown, 100 lethal, 0 .. 99 scaled from the walkable cost range.
 *
 * @param cost        Cost of the cell (the "cost" layer).
 * @param parameters  Cost map parameters (lethalCost, traversabilityThreshold).
 * @return            OccupancyGrid value: round( cost / ( 1 - traversabilityThreshold ) * 99 ) clamped to 0 .. 99.
 */
int getOccupancyValue(float cost, const CostMapParameters& parameters);

/**
 * Get the heading of the path at one of its poses: toward the next cell, or from the previous one at the end.
 *
 * @param map    The map (cell positions).
 * @param path   Cell indices of the path, start to end.
 * @param index  Position in the path.
 * @return       Yaw in radians, counter-clockwise from +x; 0 for a path of one cell.
 */
double getPathYaw(const GridMap& map, const std::vector<int>& path, int index);

/**
 * Write the cost layer as a nav_msgs/msg/OccupancyGrid in YAML.
 *
 * @param file        Stream to write to.
 * @param map         The map (its "cost" layer).
 * @param parameters  Cost map parameters (for getOccupancyValue).
 * @param frameId     header.frame_id ("map").
 */
void writeOccupancyGridMessage(std::ostream& file, const GridMap& map, const CostMapParameters& parameters, const std::string& frameId = "map");

/**
 * Write a cell as a geometry_msgs/msg/PoseStamped in YAML.
 *
 * @param file     Stream to write to.
 * @param map      The map (the cell's position).
 * @param cell     Cell index (row * columns + column).
 * @param yaw      Heading in radians, counter-clockwise from +x.
 * @param frameId  header.frame_id ("map").
 * @param indent   Text put before every line (used to nest the pose in a Path).
 */
void writePoseStampedMessage(std::ostream& file, const GridMap& map, int cell, double yaw, const std::string& frameId = "map", const std::string& indent = "");

/**
 * Write a path as a nav_msgs/msg/Path in YAML, one PoseStamped per cell with the heading along the path.
 *
 * @param file     Stream to write to.
 * @param map      The map (cell positions).
 * @param path     Cell indices of the path, start to end (empty: no poses).
 * @param frameId  header.frame_id ("map").
 */
void writePathMessage(std::ostream& file, const GridMap& map, const std::vector<int>& path, const std::string& frameId = "map");

/**
 * Write waypoints as a nav_msgs/msg/Path in YAML: each pose at its position and heading, stamped with its arrival time.
 *
 * @param file       Stream to write to.
 * @param waypoints  Waypoints, start to end (makeWaypoints); empty: no poses.
 * @param frameId    header.frame_id ("map").
 */
void writeWaypointsMessage(std::ostream& file, const std::vector<Waypoint>& waypoints, const std::string& frameId = "map");

/**
 * Get the shell command that publishes a message file once on a topic.
 *
 * @param topic     Topic name, without the leading slash.
 * @param type      Message type, e.g. "grid_map_msgs/msg/GridMap".
 * @param filename  Absolute path of the YAML file.
 * @return          ros2 topic pub --once /<topic> <type> "$(cat '<filename>')".
 */
std::string getRosPublishCommand(const std::string& topic, const std::string& type, const std::string& filename);
