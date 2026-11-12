#include "CostMap.h"
#include <cmath>
#include "TerrainFeatures.h"
#include "Traversability.h"

bool isLethal(float cost, const CostMapParameters& parameters) {
	return !(cost < parameters.lethalCost); //NaN counts as lethal too
}

void buildCostMap(GridMap& map, const CostMapParameters& parameters) {
	map.addLayer("cost", 0.f);
	std::vector<float>& cost = map.getLayer("cost");
	const std::vector<float>& traversability = map.getLayer("traversability");

	for (int i = 0; i < map.getCellCount(); i++) {
		float t = traversability[i];
		if (GridMap::isUnknown(t)) {
			switch (parameters.unknownPolicy) {
			case UnknownPolicy::Lethal:    cost[i] = parameters.lethalCost;  break;
			case UnknownPolicy::FixedCost: cost[i] = parameters.unknownCost; break;
			case UnknownPolicy::Free:      cost[i] = 0.f;                    break;
			}
		}
		else if (t < parameters.traversabilityThreshold) {
			cost[i] = parameters.lethalCost;
		}
		else {
			cost[i] = 1.f - t;
		}
	}
}

void inflateCostMap(GridMap& map, const CostMapParameters& parameters) {
	std::vector<float>& cost = map.getLayer("cost");
	std::vector<float> inflated = cost; //written separately so new lethal cells don't spread further
	int reach = (int)std::round(parameters.inflationRadius / map.getResolution());
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			if (!isLethal(cost[map.getIndex(column, row)], parameters)) continue;
			for (int dy = -reach; dy <= reach; dy++) {
				for (int dx = -reach; dx <= reach; dx++) {
					if (dx * dx + dy * dy > reach * reach) continue;
					int c = column + dx, r = row + dy;
					if (map.isInside(c, r)) {
						inflated[map.getIndex(c, r)] = parameters.lethalCost;
					}
				}
			}
		}
	}
	cost = inflated;
}

void runCostMapPipeline(GridMap& map, const CostMapParameters& parameters) {
	computeTerrainFeatures(map, parameters);
	computeTraversability(map, parameters);
	buildCostMap(map, parameters);
	inflateCostMap(map, parameters);
}
