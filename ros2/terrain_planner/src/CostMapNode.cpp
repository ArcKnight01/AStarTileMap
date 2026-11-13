// cost_map_node: elevation map in, cost map out (ROS2_PLAN.md section 3).
//
//   in   elevation_map  grid_map_msgs/GridMap   layer "elevation" (from the elevation mapper)
//   out  cost_map       grid_map_msgs/GridMap   elevation, slope, roughness, step, traversability, cost
//   out  cost_map_grid  nav_msgs/OccupancyGrid  the cost layer for RViz
//
// Each map runs through the same runCostMapPipeline() the SFML app uses. The subscription keeps only the newest map, so
// if maps arrive faster than the pipeline runs, the old ones are dropped. The parameters can be changed while it runs
// (ros2 param set /cost_map_node critical_slope 0.5); the next map uses them.
#include <chrono>
#include <grid_map_msgs/msg/grid_map.hpp>
#include <grid_map_ros/grid_map_ros.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include "CostMap.h"
#include "terrain_planner/GridMapConversion.h"
#include "terrain_planner/ParameterLoading.h"

namespace terrain_planner {

class CostMapNode : public rclcpp::Node {
public:
	explicit CostMapNode(const rclcpp::NodeOptions& options) : Node("cost_map_node", options) {
		parameters = declareCostMapParameters(*this);
		elevationLayer = declare_parameter("elevation_layer", std::string("elevation"));
		parameterCallback = add_on_set_parameters_callback([this](const std::vector<rclcpp::Parameter>& changed) {
			return applyCostMapParameters(changed, parameters);
		});

		costMapPublisher = create_publisher<grid_map_msgs::msg::GridMap>("cost_map", rclcpp::QoS(1).transient_local());
		gridPublisher = create_publisher<nav_msgs::msg::OccupancyGrid>("cost_map_grid", rclcpp::QoS(1).transient_local());
		elevationSubscription = create_subscription<grid_map_msgs::msg::GridMap>("elevation_map", rclcpp::QoS(1),
			[this](grid_map_msgs::msg::GridMap::ConstSharedPtr message) { onElevationMap(*message); });
		RCLCPP_INFO(get_logger(), "waiting for elevation maps on %s", elevationSubscription->get_topic_name());
	}

private:
	void onElevationMap(const grid_map_msgs::msg::GridMap& message) {
		grid_map::GridMap elevationMap;
		if (!grid_map::GridMapRosConverter::fromMessage(message, elevationMap)) {
			RCLCPP_WARN(get_logger(), "could not read the elevation map message");
			return;
		}
		GridMap map;
		MapPlacement placement;
		if (!fromGridMap(elevationMap, map, placement, { elevationLayer })) {
			RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "the elevation map has no \"%s\" layer", elevationLayer.c_str());
			return;
		}
		if (elevationLayer != "elevation") {
			map.addLayer("elevation"); //the core reads "elevation"
			map.getLayer("elevation") = map.getLayer(elevationLayer);
		}

		auto started = std::chrono::steady_clock::now();
		runCostMapPipeline(map, parameters);
		double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
		RCLCPP_DEBUG(get_logger(), "%d x %d cells in %.1f ms", map.getColumns(), map.getRows(), milliseconds);

		costMapPublisher->publish(*grid_map::GridMapRosConverter::toMessage(toGridMap(map, placement)));
		gridPublisher->publish(toOccupancyGrid(map, placement, parameters));
	}

	CostMapParameters parameters;
	std::string elevationLayer;
	OnSetParametersCallbackHandle::SharedPtr parameterCallback;
	rclcpp::Publisher<grid_map_msgs::msg::GridMap>::SharedPtr costMapPublisher;
	rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr gridPublisher;
	rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr elevationSubscription;
};

} // namespace terrain_planner

RCLCPP_COMPONENTS_REGISTER_NODE(terrain_planner::CostMapNode)
