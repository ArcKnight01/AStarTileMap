// planner_node: cost map + robot pose + goal in, path / waypoints out (ROS2_PLAN.md section 3).
//
//   in   cost_map        grid_map_msgs/GridMap          only the "cost" layer is used (so a cost map from anywhere works)
//   in   TF <cost map frame> -> robot_frame              the robot's position = the start cell
//   in   goal_pose       geometry_msgs/PoseStamped      RViz's "2D Goal Pose"
//   out  planned_path    nav_msgs/Path                  every cell of the path
//   out  waypoints       nav_msgs/Path                  pruned, sampled poses with heading; stamp = planned arrival time
//   out  searched_cells  visualization_msgs/MarkerArray the cells the algorithm evaluated (purple, like the app)
//
// Paths are published in the cost map's frame. It re-plans when a goal or a new cost map arrives, at most replan_rate
// times a second. The search is the core's findPath(), the same as the SFML app.
#include <cmath>
#include <memory>
#include <optional>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <grid_map_msgs/msg/grid_map.hpp>
#include <grid_map_ros/grid_map_ros.hpp>
#include <nav_msgs/msg/path.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2_ros/buffer.hpp>
#include <tf2_ros/transform_listener.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include "CostMap.h"
#include "PathPlanner.h"
#include "terrain_planner/GridMapConversion.h"
#include "terrain_planner/ParameterLoading.h"
#include "Waypoints.h" //core/Waypoints.h (was terrain_planner/Waypoints.h, this package's own copy)

namespace terrain_planner {

static double getYaw(const geometry_msgs::msg::Quaternion& q) {
	return std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
}

static geometry_msgs::msg::Quaternion makeQuaternion(double yaw) {
	geometry_msgs::msg::Quaternion q;
	q.z = std::sin(yaw / 2);
	q.w = std::cos(yaw / 2);
	return q;
}

class PlannerNode : public rclcpp::Node {
public:
	explicit PlannerNode(const rclcpp::NodeOptions& options) : Node("planner_node", options) {
		parameters = declareCostMapParameters(*this); //lethal_cost and cost_weight matter here
		std::string algorithmName = declare_parameter("algorithm", std::string("astar"));
		if (!parsePathAlgorithm(algorithmName, algorithm)) {
			RCLCPP_WARN(get_logger(), "algorithm \"%s\" is not astar / dijkstra / bfs / dfs; using astar", algorithmName.c_str());
			algorithm = PathAlgorithm::AStar;
		}
		robotFrame = declare_parameter("robot_frame", std::string("base_link"));
		goalTolerance = declare_parameter("goal_tolerance", 0.2);
		startSearchRadius = declare_parameter("start_search_radius", 0.5);
		publishSearchedCells = declare_parameter("publish_searched_cells", true);
		waypointParameters.spacing = declare_parameter("waypoint_spacing", waypointParameters.spacing);
		waypointParameters.nominalSpeed = declare_parameter("nominal_speed", waypointParameters.nominalSpeed);
		waypointParameters.costSlowdown = declare_parameter("cost_slowdown", waypointParameters.costSlowdown);
		double replanRate = declare_parameter("replan_rate", 2.0);

		parameterCallback = add_on_set_parameters_callback([this](const std::vector<rclcpp::Parameter>& changed) {
			auto result = applyCostMapParameters(changed, parameters);
			if (!result.successful) return result;
			for (const rclcpp::Parameter& parameter : changed) {
				const std::string& name = parameter.get_name();
				if (name == "algorithm" && !parsePathAlgorithm(parameter.as_string(), algorithm)) {
					result.successful = false;
					result.reason = "algorithm must be astar, dijkstra, bfs or dfs";
				}
				else if (name == "waypoint_spacing") waypointParameters.spacing = parameter.as_double();
				else if (name == "nominal_speed") waypointParameters.nominalSpeed = parameter.as_double();
				else if (name == "cost_slowdown") waypointParameters.costSlowdown = parameter.as_double();
				else if (name == "goal_tolerance") goalTolerance = parameter.as_double();
				else if (name == "start_search_radius") startSearchRadius = parameter.as_double();
			}
			needsPlan = true;
			return result;
		});

		tfBuffer = std::make_unique<tf2_ros::Buffer>(get_clock());
		tfListener = std::make_unique<tf2_ros::TransformListener>(*tfBuffer);

		pathPublisher = create_publisher<nav_msgs::msg::Path>("planned_path", 1);
		waypointPublisher = create_publisher<nav_msgs::msg::Path>("waypoints", 1);
		searchedPublisher = create_publisher<visualization_msgs::msg::MarkerArray>("searched_cells", 1);

		costMapSubscription = create_subscription<grid_map_msgs::msg::GridMap>("cost_map", rclcpp::QoS(1).transient_local(),
			[this](grid_map_msgs::msg::GridMap::ConstSharedPtr message) { onCostMap(*message); });
		goalSubscription = create_subscription<geometry_msgs::msg::PoseStamped>("goal_pose", 1,
			[this](geometry_msgs::msg::PoseStamped::ConstSharedPtr message) {
				//log only a goal that moved (gridmap_file_publisher re-sends the same one with every map)
				bool moved = !goal || goal->header.frame_id != message->header.frame_id
					|| std::hypot(goal->pose.position.x - message->pose.position.x, goal->pose.position.y - message->pose.position.y) > 1e-6;
				goal = *message;
				needsPlan = true;
				if (moved) {
					RCLCPP_INFO(get_logger(), "new goal (%.2f, %.2f) in %s", goal->pose.position.x, goal->pose.position.y,
						goal->header.frame_id.c_str());
				}
			});
		replanTimer = create_wall_timer(std::chrono::duration<double>(1.0 / std::max(0.1, replanRate)), [this]() { plan(); });
	}

private:
	void onCostMap(const grid_map_msgs::msg::GridMap& message) {
		grid_map::GridMap costMap;
		if (!grid_map::GridMapRosConverter::fromMessage(message, costMap)) {
			RCLCPP_WARN(get_logger(), "could not read the cost map message");
			return;
		}
		GridMap converted;
		MapPlacement convertedPlacement;
		if (!fromGridMap(costMap, converted, convertedPlacement, { "cost" })) {
			RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "the cost map has no \"cost\" layer");
			return;
		}
		map = std::move(converted);
		placement = convertedPlacement;
		hasMap = true;
		needsPlan = true;
	}

	//the walkable cell nearest to `cell` within startSearchRadius (the robot's own cell is often inside the inflation of
	//something lethal next to it); -1 if there is none
	int findWalkableCell(int cell) const {
		const std::vector<float>& cost = map.getLayer("cost");
		if (!isLethal(cost[cell], parameters)) return cell;
		int radius = (int)std::ceil(startSearchRadius / map.getResolution());
		int column0 = map.getColumn(cell), row0 = map.getRow(cell);
		int best = -1;
		double bestDistance = 1e18;
		for (int row = row0 - radius; row <= row0 + radius; row++) {
			for (int column = column0 - radius; column <= column0 + radius; column++) {
				if (!map.isInside(column, row)) continue;
				double distance = std::hypot(column - column0, row - row0);
				if (distance > radius || distance >= bestDistance) continue;
				if (!isLethal(cost[map.getIndex(column, row)], parameters)) {
					best = map.getIndex(column, row);
					bestDistance = distance;
				}
			}
		}
		return best;
	}

	void publishEmpty() {
		nav_msgs::msg::Path empty;
		empty.header.frame_id = placement.frameId;
		empty.header.stamp = now();
		pathPublisher->publish(empty);
		waypointPublisher->publish(empty);
	}

	void plan() {
		if (!needsPlan || !hasMap || !goal) return;

		//the robot's position and the goal, in the cost map's frame
		geometry_msgs::msg::TransformStamped robot;
		geometry_msgs::msg::PoseStamped goalInMap;
		try {
			robot = tfBuffer->lookupTransform(placement.frameId, robotFrame, tf2::TimePointZero);
			geometry_msgs::msg::PoseStamped goalNow = *goal;
			goalNow.header.stamp = rclcpp::Time(0, 0, get_clock()->get_clock_type()); //latest transform
			goalInMap = tfBuffer->transform(goalNow, placement.frameId);
		}
		catch (const tf2::TransformException& error) {
			RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "no transform yet: %s", error.what());
			return; //try again next tick
		}
		needsPlan = false;

		double robotX = robot.transform.translation.x, robotY = robot.transform.translation.y;
		double goalX = goalInMap.pose.position.x, goalY = goalInMap.pose.position.y;
		if (std::hypot(goalX - robotX, goalY - robotY) <= goalTolerance) {
			RCLCPP_INFO(get_logger(), "goal reached");
			goal.reset();
			publishEmpty();
			return;
		}

		int startColumn, startRow, endColumn, endRow;
		if (!getCellAtWorld(map, placement, robotX, robotY, startColumn, startRow)) {
			RCLCPP_WARN(get_logger(), "the robot (%.2f, %.2f) is outside the cost map", robotX, robotY);
			publishEmpty();
			return;
		}
		if (!getCellAtWorld(map, placement, goalX, goalY, endColumn, endRow)) {
			RCLCPP_WARN(get_logger(), "the goal (%.2f, %.2f) is outside the cost map", goalX, goalY);
			publishEmpty();
			return;
		}
		int start = findWalkableCell(map.getIndex(startColumn, startRow));
		int end = map.getIndex(endColumn, endRow);
		if (start == -1) {
			RCLCPP_WARN(get_logger(), "no walkable cell within %.2f m of the robot", startSearchRadius);
			publishEmpty();
			return;
		}
		if (isLethal(map.getLayer("cost")[end], parameters)) {
			RCLCPP_WARN(get_logger(), "the goal is on a lethal cell");
			publishEmpty();
			return;
		}

		PathResult result = findPath(map, parameters, algorithm, start, end);
		if (publishSearchedCells) publishSearched(result.searchedCells);
		if (result.path.empty()) {
			RCLCPP_WARN(get_logger(), "no path (searched %zu cells)", result.searchedCells.size());
			publishEmpty();
			return;
		}

		const rclcpp::Time stamp = now();
		nav_msgs::msg::Path path;
		path.header.frame_id = placement.frameId;
		path.header.stamp = stamp;
		for (std::size_t i = 0; i < result.path.size(); i++) {
			geometry_msgs::msg::PoseStamped pose;
			pose.header = path.header;
			int cell = result.path[i];
			getWorldPosition(map, placement, map.getColumn(cell), map.getRow(cell), pose.pose.position.x, pose.pose.position.y);
			pose.pose.orientation.w = 1.0;
			path.poses.push_back(pose);
		}
		pathPublisher->publish(path);

		std::vector<Waypoint> waypoints = makeWaypoints(map, prunePath(map, parameters, result.path), waypointParameters,
			getYaw(goalInMap.pose.orientation));
		nav_msgs::msg::Path waypointPath;
		waypointPath.header = path.header;
		for (const Waypoint& waypoint : waypoints) {
			geometry_msgs::msg::PoseStamped pose;
			pose.header.frame_id = placement.frameId;
			pose.header.stamp = stamp + rclcpp::Duration::from_seconds(waypoint.time); //planned arrival time
			pose.pose.position.x = waypoint.x + placement.centerX;
			pose.pose.position.y = waypoint.y + placement.centerY;
			pose.pose.orientation = makeQuaternion(waypoint.yaw);
			waypointPath.poses.push_back(pose);
		}
		waypointPublisher->publish(waypointPath);
		RCLCPP_DEBUG(get_logger(), "path: %zu cells, %zu waypoints, cost %.1f", result.path.size(), waypoints.size(), result.cost);
	}

	void publishSearched(const std::vector<int>& cells) {
		visualization_msgs::msg::Marker marker;
		marker.header.frame_id = placement.frameId;
		marker.header.stamp = now();
		marker.ns = "searched_cells";
		marker.id = 0;
		marker.type = visualization_msgs::msg::Marker::CUBE_LIST;
		marker.action = visualization_msgs::msg::Marker::ADD;
		marker.pose.orientation.w = 1.0;
		marker.scale.x = marker.scale.y = map.getResolution();
		marker.scale.z = 0.01;
		marker.color.r = 0.6f;
		marker.color.g = 0.2f;
		marker.color.b = 0.8f;
		marker.color.a = 0.4f;
		for (int cell : cells) {
			geometry_msgs::msg::Point point;
			getWorldPosition(map, placement, map.getColumn(cell), map.getRow(cell), point.x, point.y);
			marker.points.push_back(point);
		}
		visualization_msgs::msg::MarkerArray markers;
		markers.markers.push_back(marker);
		searchedPublisher->publish(markers);
	}

	CostMapParameters parameters;
	PathAlgorithm algorithm = PathAlgorithm::AStar;
	WaypointParameters waypointParameters;
	std::string robotFrame;
	double goalTolerance = 0.2;
	double startSearchRadius = 0.5;
	bool publishSearchedCells = true;

	GridMap map;
	MapPlacement placement;
	bool hasMap = false;
	std::optional<geometry_msgs::msg::PoseStamped> goal;
	bool needsPlan = false;

	std::unique_ptr<tf2_ros::Buffer> tfBuffer;
	std::unique_ptr<tf2_ros::TransformListener> tfListener;
	OnSetParametersCallbackHandle::SharedPtr parameterCallback;
	rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr pathPublisher;
	rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr waypointPublisher;
	rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr searchedPublisher;
	rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr costMapSubscription;
	rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr goalSubscription;
	rclcpp::TimerBase::SharedPtr replanTimer;
};

} // namespace terrain_planner

RCLCPP_COMPONENTS_REGISTER_NODE(terrain_planner::PlannerNode)
