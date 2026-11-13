// gridmap_file_publisher: plays a maps/*.gridmap file as if it were the elevation mapper, so the ROS side can run with
// no robot (ROS2_PLAN.md section 3).
//
//   out  elevation_map or cost_map   grid_map_msgs/GridMap     the file's layers, every 1 / rate seconds
//   out  TF frame_id -> robot_frame                             static, at the file's start cell (publish_tf)
//   out  goal_pose                   geometry_msgs/PoseStamped  the file's end cell, with each map (publish_goal)
//
// publish_as: "elevation" -> elevation_map (the cost map node computes everything from "elevation");
//             "cost"      -> cost_map, straight to the planner (for maps drawn as a cost layer, like dungeon);
//             "auto"      -> "elevation" if the file has the pipeline's layers (traversability), else "cost".
#include <chrono>
#include <cmath>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <grid_map_msgs/msg/grid_map.hpp>
#include <grid_map_ros/grid_map_ros.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <tf2_ros/static_transform_broadcaster.hpp>
#include "GridMapFile.h"
#include "terrain_planner/GridMapConversion.h"

namespace terrain_planner {

class GridMapFilePublisher : public rclcpp::Node {
public:
	explicit GridMapFilePublisher(const rclcpp::NodeOptions& options) : Node("gridmap_file_publisher", options) {
		std::string file = declare_parameter("file", std::string(""));
		std::string publishAs = declare_parameter("publish_as", std::string("auto"));
		placement.frameId = declare_parameter("frame_id", std::string("map"));
		placement.centerX = declare_parameter("center_x", 0.0);
		placement.centerY = declare_parameter("center_y", 0.0);
		std::string robotFrame = declare_parameter("robot_frame", std::string("base_link"));
		bool publishTf = declare_parameter("publish_tf", true);
		publishGoal = declare_parameter("publish_goal", true);
		double rate = declare_parameter("rate", 1.0);

		int start = -1;
		if (file.empty() || !loadGridMap(file, map, start, end)) {
			throw std::runtime_error("gridmap_file_publisher: could not load the map file \"" + file + "\" (parameter file)");
		}
		if (publishAs == "auto") publishAs = map.hasLayer("traversability") ? "elevation" : "cost";
		std::vector<std::string> layers;
		std::string topic;
		if (publishAs == "elevation") {
			if (!map.hasLayer("elevation")) throw std::runtime_error("gridmap_file_publisher: the file has no elevation layer");
			layers = { "elevation" };
			topic = "elevation_map";
		}
		else if (publishAs == "cost") {
			if (!map.hasLayer("cost")) throw std::runtime_error("gridmap_file_publisher: the file has no cost layer");
			layers = map.getLayerNames();
			topic = "cost_map";
		}
		else {
			throw std::runtime_error("gridmap_file_publisher: publish_as must be auto, elevation or cost");
		}
		outputLayers = layers;
		mapPublisher = create_publisher<grid_map_msgs::msg::GridMap>(topic, rclcpp::QoS(1).transient_local());
		goalPublisher = create_publisher<geometry_msgs::msg::PoseStamped>("goal_pose", 1);
		RCLCPP_INFO(get_logger(), "%s: %d x %d cells at %.2f m, publishing on %s", file.c_str(), map.getColumns(),
			map.getRows(), map.getResolution(), mapPublisher->get_topic_name());

		if (publishTf) {
			geometry_msgs::msg::TransformStamped transform;
			transform.header.stamp = now();
			transform.header.frame_id = placement.frameId;
			transform.child_frame_id = robotFrame;
			transform.transform.rotation.w = 1.0;
			if (start >= 0) {
				getWorldPosition(map, placement, map.getColumn(start), map.getRow(start),
					transform.transform.translation.x, transform.transform.translation.y);
			}
			else {
				RCLCPP_WARN(get_logger(), "the file has no start cell; the robot is put at the map centre");
				transform.transform.translation.x = placement.centerX;
				transform.transform.translation.y = placement.centerY;
			}
			tfBroadcaster = std::make_unique<tf2_ros::StaticTransformBroadcaster>(*this);
			tfBroadcaster->sendTransform(transform);
		}

		timer = create_wall_timer(std::chrono::duration<double>(1.0 / std::max(0.01, rate)), [this]() { publish(); });
		publish();
	}

private:
	void publish() {
		placement.timestampNs = now().nanoseconds();
		mapPublisher->publish(*grid_map::GridMapRosConverter::toMessage(toGridMap(map, placement, outputLayers)));
		if (publishGoal && end >= 0) {
			geometry_msgs::msg::PoseStamped goal;
			goal.header.frame_id = placement.frameId;
			goal.header.stamp = now();
			getWorldPosition(map, placement, map.getColumn(end), map.getRow(end), goal.pose.position.x, goal.pose.position.y);
			goal.pose.orientation.w = 1.0;
			goalPublisher->publish(goal);
		}
	}

	GridMap map;
	MapPlacement placement;
	std::vector<std::string> outputLayers;
	int end = -1;
	bool publishGoal = true;
	rclcpp::Publisher<grid_map_msgs::msg::GridMap>::SharedPtr mapPublisher;
	rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goalPublisher;
	std::unique_ptr<tf2_ros::StaticTransformBroadcaster> tfBroadcaster;
	rclcpp::TimerBase::SharedPtr timer;
};

} // namespace terrain_planner

RCLCPP_COMPONENTS_REGISTER_NODE(terrain_planner::GridMapFilePublisher)
