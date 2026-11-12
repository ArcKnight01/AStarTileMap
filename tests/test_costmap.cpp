// Tests of core/ (no SFML): the grid map, each stage of the cost-map pipeline, the planners, and the example maps.
// Run: ctest --test-dir build --output-on-failure   (runs from the repo folder, so maps/ is found)
#include <cmath>
#include <cstdio>
#include <queue>
#include <random>
#include "CostMap.h"
#include "CostMapParameters.h"
#include "GridMap.h"
#include "GridMapFile.h"
#include "PathPlanner.h"
#include "TerrainFeatures.h"
#include "TerrainGenerator.h"
#include "Traversability.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

// Cheapest path cost with the planner's own step cost (eq. 3.8), by brute force (Dijkstra with a priority queue).
// With unitCost every step costs 1, which gives the fewest steps (what BFS minimises). -1 = unreachable.
static double bruteForceCost(const GridMap& map, const CostMapParameters& p, int start, int end, bool unitCost) {
	const std::vector<float>& cost = map.getLayer("cost");
	if (isLethal(cost[start], p) || isLethal(cost[end], p)) return -1;
	std::vector<double> best(map.getCellCount(), 1e300);
	std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> queue;
	best[start] = 0; queue.push({ 0, start });
	while (!queue.empty()) {
		auto top = queue.top(); queue.pop();
		double d = top.first; int cell = top.second;
		if (d > best[cell]) continue;
		if (cell == end) return d;
		for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
			if (!dx && !dy) continue;
			int c = map.getColumn(cell) + dx, r = map.getRow(cell) + dy;
			if (!map.isInside(c, r)) continue;
			int n = map.getIndex(c, r);
			if (isLethal(cost[n], p)) continue;
			double nd = d + (unitCost ? 1.0 : getStepCost(map, p, cell, n));
			if (nd < best[n] - 1e-12) { best[n] = nd; queue.push({ nd, n }); }
		}
	}
	return -1;
}

// A path's shape: starts and ends right, only neighbouring steps, never on a lethal cell.
static void checkPathShape(const GridMap& map, const CostMapParameters& p, const std::vector<int>& path, int start, int end, const char* what) {
	CHECK(path.front() == start && path.back() == end, "%s: path does not run from start to end", what);
	for (std::size_t k = 0; k < path.size(); k++) {
		CHECK(!isLethal(map.getLayer("cost")[path[k]], p), "%s: path enters a lethal cell", what);
		if (k == 0) continue;
		int dc = std::abs(map.getColumn(path[k]) - map.getColumn(path[k - 1]));
		int dr = std::abs(map.getRow(path[k]) - map.getRow(path[k - 1]));
		CHECK(std::max(dc, dr) == 1, "%s: path jumps between cells", what);
	}
}

static bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

int main() {
	CostMapParameters p;

	// --- GridMap: indices, positions, layers, file round trip ---
	{
		GridMap map(10, 6, 0.1);
		CHECK(map.getIndex(3, 2) == 23 && map.getColumn(23) == 3 && map.getRow(23) == 2, "grid: index <-> column/row");
		double x, y; int c, r;
		map.getPosition(3, 2, x, y);
		CHECK(map.getCell(x, y, c, r) && c == 3 && r == 2, "grid: cell -> world -> cell");
		map.getPosition(0, 0, x, y);
		CHECK(x < 0 && y > 0, "grid: the map is centred on (0, 0) with row 0 at the top (+y)");
		map.addLayer("elevation", 1.5f);
		map.at("elevation", 4, 4) = GridMap::getUnknownValue();
		CHECK(GridMap::isUnknown(map.at("elevation", 4, 4)) && map.at("elevation", 0, 0) == 1.5f, "grid: layers and unknown values");
		saveGridMap("/tmp/test_costmap_roundtrip.gridmap", map, 5, 17);
		GridMap loaded; int start, end;
		CHECK(loadGridMap("/tmp/test_costmap_roundtrip.gridmap", loaded, start, end), "file: loads what it saved");
		CHECK(loaded.getColumns() == 10 && loaded.getRows() == 6 && near(loaded.getResolution(), 0.1, 1e-9) && start == 5 && end == 17,
			"file: size, resolution, start and end survive");
		CHECK(GridMap::isUnknown(loaded.at("elevation", 4, 4)) && loaded.at("elevation", 0, 0) == 1.5f, "file: values and NaN survive");

		//grid_map's storage order: index (0, 0) is the corner with the largest x and y, i runs toward -x, j toward -y,
		//column-major (checked against grid_map_ros::GridMapRosConverter::fromMessage when the format was written)
		CHECK(getMessageIndex(map, 9, 0) == 0, "file: data[0] is the cell with the largest x and y (column 9, row 0)");
		CHECK(getMessageIndex(map, 8, 0) == 1 && getMessageIndex(map, 9, 1) == 10, "file: data runs toward -x first, then -y");
		CHECK(getEndpointsFilename("maps/rugged.gridmap") == "maps/rugged.endpoints.yaml", "file: start / end go next to the map");
		GridMap noEndpoints; int noStart, noEnd;
		saveGridMapMessage("/tmp/test_costmap_no_endpoints.gridmap", map);
		std::remove("/tmp/test_costmap_no_endpoints.endpoints.yaml");
		CHECK(loadGridMap("/tmp/test_costmap_no_endpoints.gridmap", noEndpoints, noStart, noEnd) && noStart == -1 && noEnd == -1,
			"file: a map without an endpoints file loads with no start or end");
	}

	// --- features on terrain with known answers ---
	{
		GridMap flat = createFlatMap(20, 20, 0.1);
		computeTerrainFeatures(flat, p);
		CHECK(near(flat.at("slope", 10, 10), 0, 1e-6) && near(flat.at("roughness", 10, 10), 0, 1e-6) && near(flat.at("step", 10, 10), 0, 1e-6),
			"features: flat ground has no slope, roughness or step");

		const double angle = 0.30; //rad: a ramp rising along +x
		GridMap ramp = createFlatMap(20, 20, 0.1);
		for (int row = 0; row < 20; row++) for (int column = 0; column < 20; column++) {
			double x, y; ramp.getPosition(column, row, x, y);
			ramp.at("elevation", column, row) = (float)(std::tan(angle) * x);
		}
		computeTerrainFeatures(ramp, p);
		CHECK(near(ramp.at("slope", 10, 10), angle, 1e-3), "features: a %.2f rad ramp has slope %.3f", angle, ramp.at("slope", 10, 10));
		CHECK(near(ramp.at("roughness", 10, 10), 0, 1e-4), "features: a plane has no roughness");

		GridMap block = createFlatMap(20, 20, 0.1);
		addBlock(block, 10, 0, 19, 19, 0.3); //a 0.3 m step at column 10
		computeTerrainFeatures(block, p);
		CHECK(near(block.at("step", 9, 10), 0.3, 1e-5) && near(block.at("step", 3, 10), 0, 1e-6), "features: the step height is found at the edge only");

		GridMap holes = createFlatMap(20, 20, 0.1);
		holes.at("elevation", 5, 5) = GridMap::getUnknownValue();
		computeTerrainFeatures(holes, p);
		CHECK(GridMap::isUnknown(holes.at("slope", 5, 5)), "features: an unknown cell has unknown features");
	}

	// --- traversability (Wermelinger et al. / ETH scoring) ---
	{
		CHECK(getFeatureScore(0, 1) == 1.0 && near(getFeatureScore(0.25, 1), 0.75, 1e-12) && getFeatureScore(1, 1) == 0.0 && getFeatureScore(2, 1) == 0.0,
			"traversability: score = 1 - value / critical, 0 at or above critical");
		GridMap map(1, 1, 0.1);
		map.addLayer("slope", (float)(2 * p.criticalSlope)); //too steep
		map.addLayer("roughness", 0.f);
		map.addLayer("step", 0.f);
		computeTraversability(map, p);
		CHECK(near(map.getLayer("traversability")[0], 2.0 / 3.0, 1e-6), "traversability: (ETH) a critical slope only zeroes its own third");
		CostMapParameters strict = p;
		strict.zeroIfAnyCritical = true;
		computeTraversability(map, strict);
		CHECK(map.getLayer("traversability")[0] == 0.f, "traversability: (strict) any critical feature makes the cell 0");
	}

	// --- cost map (Rajput Algorithm 1) and inflation ---
	{
		GridMap map(3, 1, 0.1);
		map.addLayer("traversability");
		std::vector<float>& t = map.getLayer("traversability");
		t[0] = 0.9f; t[1] = 0.3f; t[2] = GridMap::getUnknownValue();
		buildCostMap(map, p);
		const std::vector<float>& cost = map.getLayer("cost");
		CHECK(near(cost[0], 0.1, 1e-6) && isLethal(cost[1], p) && isLethal(cost[2], p),
			"cost map: 1 - t when t >= 0.5, lethal below, unknown lethal by default");
		CostMapParameters fixed = p;
		fixed.unknownPolicy = UnknownPolicy::FixedCost;
		buildCostMap(map, fixed);
		CHECK(map.getLayer("cost")[2] == fixed.unknownCost, "cost map: UnknownPolicy::FixedCost");

		GridMap inflate(21, 21, 0.1);
		inflate.addLayer("cost", 0.f);
		inflate.at("cost", 10, 10) = p.lethalCost;
		inflateCostMap(inflate, p);
		int reach = (int)std::round(p.inflationRadius / 0.1);
		CHECK(isLethal(inflate.at("cost", 10 + reach, 10), p) && !isLethal(inflate.at("cost", 10 + reach + 1, 10), p),
			"inflation: lethal grows by exactly the inflation radius");
	}

	// --- planners on random cost maps, against brute force ---
	{
		std::mt19937 rng(2026);
		int withPath = 0, withoutPath = 0;
		for (int trial = 0; trial < 400; trial++) {
			GridMap map(30, 30, 0.1);
			map.addLayer("cost", 0.f);
			std::uniform_real_distribution<float> costValue(0.f, 0.5f);
			std::bernoulli_distribution lethal((trial % 5) * 0.08);
			for (float& c : map.getLayer("cost")) c = lethal(rng) ? p.lethalCost : costValue(rng);
			std::uniform_int_distribution<int> cell(0, map.getCellCount() - 1);
			int start = cell(rng), end = cell(rng);
			double cheapest = bruteForceCost(map, p, start, end, false);
			double fewest = bruteForceCost(map, p, start, end, true);
			(cheapest < 0 ? withoutPath : withPath)++;
			for (PathAlgorithm algorithm : { PathAlgorithm::AStar, PathAlgorithm::Dijkstra, PathAlgorithm::BFS, PathAlgorithm::DFS }) {
				const std::string name = getPathAlgorithmName(algorithm);
				PathResult result = findPath(map, p, algorithm, start, end);
				if (cheapest < 0) {
					CHECK(result.path.empty(), "trial %d: %s found a path where none exists", trial, name.c_str());
					continue;
				}
				CHECK(!result.path.empty(), "trial %d: %s found no path but one exists", trial, name.c_str());
				if (result.path.empty()) continue;
				checkPathShape(map, p, result.path, start, end, name.c_str());
				CHECK(near(result.cost, getPathCost(map, p, result.path), 1e-9), "trial %d: %s reports the wrong cost", trial, name.c_str());
				if (algorithm == PathAlgorithm::AStar || algorithm == PathAlgorithm::Dijkstra)
					CHECK(near(result.cost, cheapest, 1e-6), "trial %d: %s cost %.4f, cheapest %.4f", trial, name.c_str(), result.cost, cheapest);
				if (algorithm == PathAlgorithm::BFS)
					CHECK((double)result.path.size() - 1 == fewest, "trial %d: BFS is not the fewest steps", trial);
				std::vector<bool> searched(map.getCellCount(), false);
				for (int s : result.searchedCells) searched[s] = true;
				for (int s : result.path) CHECK(searched[s], "trial %d: %s path cell not marked searched", trial, name.c_str());
			}
		}
		std::printf("random cost maps: %d with a path, %d without (each with A*, Dijkstra's, BFS, DFS)\n", withPath, withoutPath);
	}

	// --- costWeight: a costly strip is crossed when cost doesn't matter, and avoided when it does ---
	{
		GridMap map(30, 30, 0.1);
		map.addLayer("cost", 0.f);
		for (int row = 0; row < 25; row++) map.at("cost", 15, row) = 0.5f; //a costly wall-like strip with a gap at the bottom
		int start = map.getIndex(5, 5), end = map.getIndex(25, 5);
		CostMapParameters ignoreCost = p, avoidCost = p;
		ignoreCost.costWeight = 0.0;
		avoidCost.costWeight = 200.0; //crossing costs 200 * 0.5 = 100 extra; the detour around the gap is about 40 cells
		auto crossesStrip = [&](const PathResult& result) {
			for (int c : result.path) if (map.getColumn(c) == 15 && map.getRow(c) < 25) return true;
			return false;
		};
		CHECK(crossesStrip(pathfindingAstar(map, ignoreCost, start, end)), "costWeight 0: A* walks straight across the costly strip");
		CHECK(!crossesStrip(pathfindingAstar(map, avoidCost, start, end)), "costWeight 200: A* detours around the costly strip");
	}

	// --- the example maps in maps/ (made by ./build/generate_maps) ---
	{
		GridMap map; int start, end;
		CHECK(loadGridMap("maps/flat.gridmap", map, start, end), "maps: flat.gridmap loads");
		bool allZero = true;
		for (float c : map.getLayer("cost")) allZero = allZero && c == 0.f;
		CHECK(allZero && start >= 0 && end >= 0 && !pathfindingAstar(map, p, start, end).path.empty(), "maps: flat costs 0 everywhere and has a path");

		for (const char* name : { "rugged", "dungeon", "hybrid" }) {
			CHECK(loadGridMap(std::string("maps/") + name + ".gridmap", map, start, end), "maps: %s.gridmap loads", name);
			CHECK(!pathfindingAstar(map, p, start, end).path.empty(), "maps: %s has a path from its start to its end", name);
			CHECK(map.getColumns() == 72 && map.getRows() == 72, "maps: %s is 72 x 72 (10 px cells, so the sprites draw cleanly)", name);
		}
		//hybrid: every route crosses rugged terrain (blocking all costly cells leaves no path)
		GridMap zeroOnly = map;
		for (float& c : zeroOnly.getLayer("cost")) if (c > 0.f) c = p.lethalCost;
		CHECK(pathfindingBFS(zeroOnly, p, start, end).path.empty(), "maps: hybrid has no route that avoids rugged terrain");
	}

	std::printf(failures ? "%d FAILURE(S)\n" : "all core checks passed\n", failures);
	return failures ? 1 : 0;
}
