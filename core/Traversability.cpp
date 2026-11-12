#include "Traversability.h"

double getFeatureScore(double value, double critical) {
	if (value < critical) {
		return 1.0 - value / critical;
	}
	return 0.0;
}

void computeTraversability(GridMap& map, const CostMapParameters& parameters) {
	map.addLayer("traversability", GridMap::getUnknownValue());
	std::vector<float>& traversability = map.getLayer("traversability");
	const std::vector<float>& slope = map.getLayer("slope");
	const std::vector<float>& roughness = map.getLayer("roughness");
	const std::vector<float>& step = map.getLayer("step");

	for (int i = 0; i < map.getCellCount(); i++) {
		if (GridMap::isUnknown(slope[i]) || GridMap::isUnknown(roughness[i]) || GridMap::isUnknown(step[i])) {
			continue; //stays unknown
		}
		double slopeScore = getFeatureScore(slope[i], parameters.criticalSlope);
		double roughnessScore = getFeatureScore(roughness[i], parameters.criticalRoughness);
		double stepScore = getFeatureScore(step[i], parameters.criticalStep);
		if (parameters.zeroIfAnyCritical && (slopeScore == 0.0 || roughnessScore == 0.0 || stepScore == 0.0)) {
			traversability[i] = 0.f;
			continue;
		}
		traversability[i] = (float)(parameters.slopeWeight * slopeScore
			+ parameters.roughnessWeight * roughnessScore
			+ parameters.stepWeight * stepScore);
	}
}
