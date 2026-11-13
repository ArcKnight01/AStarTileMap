#pragma once
#include <string>
#include <vector>
#include <rclcpp/rclcpp.hpp>
#include "CostMapParameters.h"
#include "PathPlanner.h"

// ROS parameters <-> CostMapParameters. The names are the snake_case forms of the struct's fields (config/cost_map.yaml),
// and the defaults are the struct's own defaults, so the app, the core tests and the robot agree.
namespace terrain_planner {

//declares every CostMapParameters field on the node and returns the values it was started with
CostMapParameters declareCostMapParameters(rclcpp::Node& node);
//applies one changed parameter; false (with a reason) if it isn't valid. Unknown names are ignored (true).
bool applyCostMapParameter(const rclcpp::Parameter& parameter, CostMapParameters& parameters, std::string& reason);
//applies a whole set (from an on_set_parameters callback) to a copy; only copies it back if all are valid
rcl_interfaces::msg::SetParametersResult applyCostMapParameters(const std::vector<rclcpp::Parameter>& changed,
	CostMapParameters& parameters);

//"astar" | "dijkstra" | "bfs" | "dfs" -> PathAlgorithm; false if it isn't one of them
bool parsePathAlgorithm(const std::string& name, PathAlgorithm& algorithm);
//"lethal" | "fixed_cost" | "free" -> UnknownPolicy
bool parseUnknownPolicy(const std::string& name, UnknownPolicy& policy);

} // namespace terrain_planner
