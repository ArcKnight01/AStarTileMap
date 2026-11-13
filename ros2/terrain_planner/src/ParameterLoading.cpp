#include "terrain_planner/ParameterLoading.h"

namespace terrain_planner {

bool parsePathAlgorithm(const std::string& name, PathAlgorithm& algorithm) {
	if (name == "astar") algorithm = PathAlgorithm::AStar;
	else if (name == "dijkstra") algorithm = PathAlgorithm::Dijkstra;
	else if (name == "bfs") algorithm = PathAlgorithm::BFS;
	else if (name == "dfs") algorithm = PathAlgorithm::DFS;
	else return false;
	return true;
}

bool parseUnknownPolicy(const std::string& name, UnknownPolicy& policy) {
	if (name == "lethal") policy = UnknownPolicy::Lethal;
	else if (name == "fixed_cost") policy = UnknownPolicy::FixedCost;
	else if (name == "free") policy = UnknownPolicy::Free;
	else return false;
	return true;
}

static std::string getUnknownPolicyName(UnknownPolicy policy) {
	switch (policy) {
	case UnknownPolicy::FixedCost: return "fixed_cost";
	case UnknownPolicy::Free: return "free";
	default: return "lethal";
	}
}

CostMapParameters declareCostMapParameters(rclcpp::Node& node) {
	const CostMapParameters defaults;
	CostMapParameters p;
	p.slopeRadius = node.declare_parameter("slope_radius", defaults.slopeRadius);
	p.roughnessRadius = node.declare_parameter("roughness_radius", defaults.roughnessRadius);
	p.stepRadius = node.declare_parameter("step_radius", defaults.stepRadius);
	p.criticalSlope = node.declare_parameter("critical_slope", defaults.criticalSlope);
	p.criticalRoughness = node.declare_parameter("critical_roughness", defaults.criticalRoughness);
	p.criticalStep = node.declare_parameter("critical_step", defaults.criticalStep);
	p.slopeWeight = node.declare_parameter("weight_slope", defaults.slopeWeight);
	p.roughnessWeight = node.declare_parameter("weight_roughness", defaults.roughnessWeight);
	p.stepWeight = node.declare_parameter("weight_step", defaults.stepWeight);
	p.zeroIfAnyCritical = node.declare_parameter("zero_if_any_critical", defaults.zeroIfAnyCritical);
	p.traversabilityThreshold = node.declare_parameter("traversability_threshold", defaults.traversabilityThreshold);
	p.lethalCost = (float)node.declare_parameter("lethal_cost", (double)defaults.lethalCost);
	p.inflationRadius = node.declare_parameter("inflation_radius", defaults.inflationRadius);
	std::string policy = node.declare_parameter("unknown_policy", getUnknownPolicyName(defaults.unknownPolicy));
	if (!parseUnknownPolicy(policy, p.unknownPolicy)) {
		RCLCPP_WARN(node.get_logger(), "unknown_policy \"%s\" is not lethal / fixed_cost / free; using lethal", policy.c_str());
		p.unknownPolicy = UnknownPolicy::Lethal;
	}
	p.unknownCost = (float)node.declare_parameter("unknown_cost", (double)defaults.unknownCost);
	p.costWeight = node.declare_parameter("cost_weight", defaults.costWeight);
	return p;
}

bool applyCostMapParameter(const rclcpp::Parameter& parameter, CostMapParameters& p, std::string& reason) {
	const std::string& name = parameter.get_name();
	try {
		if (name == "unknown_policy") {
			if (!parseUnknownPolicy(parameter.as_string(), p.unknownPolicy)) {
				reason = "unknown_policy must be lethal, fixed_cost or free";
				return false;
			}
			return true;
		}
		if (name == "zero_if_any_critical") { p.zeroIfAnyCritical = parameter.as_bool(); return true; }

		//every other one is a number
		double* field = nullptr;
		if (name == "slope_radius") field = &p.slopeRadius;
		else if (name == "roughness_radius") field = &p.roughnessRadius;
		else if (name == "step_radius") field = &p.stepRadius;
		else if (name == "critical_slope") field = &p.criticalSlope;
		else if (name == "critical_roughness") field = &p.criticalRoughness;
		else if (name == "critical_step") field = &p.criticalStep;
		else if (name == "weight_slope") field = &p.slopeWeight;
		else if (name == "weight_roughness") field = &p.roughnessWeight;
		else if (name == "weight_step") field = &p.stepWeight;
		else if (name == "traversability_threshold") field = &p.traversabilityThreshold;
		else if (name == "inflation_radius") field = &p.inflationRadius;
		else if (name == "cost_weight") field = &p.costWeight;

		if (field) {
			double value = parameter.as_double();
			if (value < 0) { reason = name + " must be >= 0"; return false; }
			*field = value;
			return true;
		}
		if (name == "lethal_cost") {
			if (parameter.as_double() <= 0) { reason = "lethal_cost must be > 0"; return false; }
			p.lethalCost = (float)parameter.as_double();
			return true;
		}
		if (name == "unknown_cost") { p.unknownCost = (float)parameter.as_double(); return true; }
	}
	catch (const rclcpp::ParameterTypeException& error) {
		reason = name + ": " + error.what();
		return false;
	}
	return true; //not a cost map parameter: someone else's
}

rcl_interfaces::msg::SetParametersResult applyCostMapParameters(const std::vector<rclcpp::Parameter>& changed,
	CostMapParameters& parameters) {
	rcl_interfaces::msg::SetParametersResult result;
	result.successful = true;
	CostMapParameters updated = parameters;
	for (const rclcpp::Parameter& parameter : changed) {
		if (!applyCostMapParameter(parameter, updated, result.reason)) {
			result.successful = false;
			return result;
		}
	}
	parameters = updated;
	return result;
}

} // namespace terrain_planner
