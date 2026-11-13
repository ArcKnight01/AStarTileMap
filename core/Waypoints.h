#pragma once
#include <limits>
#include <string>
#include <vector>
#include "CostMapParameters.h"
#include "GridMap.h"

// From a path of cells to (x, y, yaw, time) waypoints for the controller (ROS2_PLAN.md section 6).
// Plain C++ (no SFML, no ROS), shared by the app and ros2/terrain_planner's planner_node.
//
//   1. prune   (Rajput 2023, thesis Algorithm 3, after Noreen et al. 2019): keep a cell only where the straight line from
//              the last kept cell stops being allowed. Allowed (our rule, ROS2_PLAN.md): it passes through no lethal or
//              unknown cell, and it costs no more than the cells it replaces (eq. 3.8 terms), so it never cuts across
//              costly ground that A* went around. Touching a lethal cell's corner is allowed, as for A*'s diagonal steps.
//   2. sample  every corner of the pruned path is a waypoint; each straight stretch is split into equal pieces of at
//              most `spacing` metres.
//   3. heading toward the next waypoint; the last one takes the goal's yaw (or keeps the last heading).
//   4. time    each piece takes length / speed, speed = nominalSpeed / ( 1 + costSlowdown * cost ).
//
// Positions are the core's map positions (metres, map centred on (0, 0)); the ROS node adds the map's placement.
// (Moved from ros2/terrain_planner/src/Waypoints.cpp, where it was in namespace terrain_planner. Changes: the cells on
//  a segment are found exactly (they were sampled 8 times per cell, which could miss a clipped corner), and the
//  corners of the pruned path are always waypoints (they were sampled past, so a line between two waypoints could cut
//  a corner the pruned path went around).)
//
// Thesis Algorithm 3 as printed has two index slips, fixed here: when x_{i+1}, x_i and the anchor x are collinear it is
// x_{i+1} that is redundant (it prints delete node(x_i)), and the new anchor is x_{i+1} (it prints x = x_i + 1). It
// also stops before checking x_0. This version walks forward from the start instead of back from the goal; both keep a
// cell exactly where the straight line from the last kept cell stops being clear.

// One waypoint for the controller.
struct Waypoint {
	double x = 0, y = 0; //m
	double yaw = 0;      //rad, direction of travel (the last one: the goal's yaw)
	double time = 0;     //s from the first waypoint
};

// How the waypoints are spaced and timed.
struct WaypointParameters {
	double spacing = 0.4;      //m, longest piece between two waypoints (about the Go2's body length)
	double nominalSpeed = 0.5; //m/s on a cost-0 cell
	double costSlowdown = 1.0; //speed = nominalSpeed / (1 + costSlowdown * cost)
};

/**
 * Get every cell a straight line between two cell centres touches, in order (Amanatides & Woo 1987 grid traversal).
 *
 * @param map               The map.
 * @param fromCell          First cell (row * columns + column).
 * @param toCell            Last cell.
 * @param onlyCornerTouched If given, filled with one flag per returned cell: true for the side cells of a grid corner.
 * @return                  Cells from fromCell to toCell; where the line passes exactly through a grid corner, both side cells too.
 */
std::vector<int> getCellsOnSegment(const GridMap& map, int fromCell, int toCell, std::vector<bool>* onlyCornerTouched = nullptr);

/**
 * Compute the cost of going straight from one cell to another, in the terms of eq. 3.8.
 *
 * @param map         The map (its "cost" layer).
 * @param parameters  Cost map parameters (lethalCost, costWeight).
 * @param fromCell    First cell.
 * @param toCell      Last cell.
 * @return            Distance in cells + costWeight * cost of every cell it passes through; infinity if one is lethal or unknown.
 *                    (A cell only touched at a grid corner neither blocks nor costs, the same as A*'s own diagonal steps.)
 */
double computeSegmentCost(const GridMap& map, const CostMapParameters& parameters, int fromCell, int toCell);

/**
 * Prune a path (thesis Algorithm 3): replace runs of cells by straight lines wherever the line is allowed.
 *
 * @param map         The map (its "cost" layer).
 * @param parameters  Cost map parameters.
 * @param path        Cells from start to end (an A* path).
 * @return            The kept cells, the first and last always among them.
 */
std::vector<int> prunePath(const GridMap& map, const CostMapParameters& parameters, const std::vector<int>& path);

/**
 * Get the waypoint positions along a pruned path: every corner, plus equal pieces of at most spacing between them.
 *
 * @param map      The map (cell positions).
 * @param path     Pruned cells, start to end.
 * @param spacing  Longest piece between two waypoints, in metres.
 * @return         Positions (x, y) in metres, start and end included.
 */
std::vector<Waypoint> computeWaypointPositions(const GridMap& map, const std::vector<int>& path, double spacing);

/**
 * Set each waypoint's yaw toward the next one; the last takes goalYaw, or keeps the previous heading if goalYaw is NaN.
 *
 * @param waypoints  Waypoints with their positions; their yaw is filled in.
 * @param goalYaw    Heading at the goal in radians, or NaN.
 */
void computeWaypointYaws(std::vector<Waypoint>& waypoints, double goalYaw);

/**
 * Set each waypoint's time: the sum of piece length / speed, with the speed set by the cost where each piece ends.
 *
 * @param waypoints           Waypoints with their positions; their time is filled in.
 * @param map                 The map (its "cost" layer, if any).
 * @param waypointParameters  nominalSpeed and costSlowdown.
 */
void computeWaypointTimes(std::vector<Waypoint>& waypoints, const GridMap& map, const WaypointParameters& waypointParameters);

/**
 * Build the waypoints of a pruned path: positions, then headings, then times.
 *
 * @param map                 The map.
 * @param path                Pruned cells, start to end (prunePath).
 * @param waypointParameters  Spacing, nominal speed and cost slowdown.
 * @param goalYaw             Heading at the goal in radians; NaN keeps the last stretch's heading.
 * @return                    The waypoints, start to end (empty for an empty path).
 */
std::vector<Waypoint> makeWaypoints(const GridMap& map, const std::vector<int>& path,
	const WaypointParameters& waypointParameters, double goalYaw = std::numeric_limits<double>::quiet_NaN());

/**
 * Get the waypoints as a command-line list of positions, "[x,y] [x,y] ...", as husky-stable_ws's waypoint scripts read them.
 *
 * @param waypoints  Waypoints, start to end.
 * @param decimals   Decimals of each coordinate (metres).
 * @return           The positions in order, separated by spaces (no yaw or time).
 */
std::string getWaypointListText(const std::vector<Waypoint>& waypoints, int decimals = 2);
