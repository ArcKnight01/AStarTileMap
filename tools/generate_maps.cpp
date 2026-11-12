// Builds the example maps in maps/ (run from the repo folder: ./build/generate_maps):
//   flat     - height 0 everywhere (cost 0), for comparison
//   rugged   - dunes, hills and small bumps, through the full pipeline (elevation -> features -> traversability -> cost)
//   dungeon  - the old 20x20 maps/dungeon.txt scaled x4; its cost layer is written directly (walls lethal, floor 0)
//   hybrid   - a dungeon zone on top, a ridge across the whole map with three rugged passes, flat ground below
// Each map is checked (a path exists; for hybrid: no route avoids rugged terrain, and there are three passes).
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "CostMap.h"
#include "CostMapParameters.h"
#include "GridMap.h"
#include "GridMapFile.h"
#include "PathPlanner.h"
#include "TerrainGenerator.h"

//72 cells: the app's 720 px map gives 10 px cells, so the 10 px wall sprite draws 1:1 and the 20 px floor sprites at half size
static const int SIZE = 72;           //cells per side
static const double RESOLUTION = 0.1; //m per cell -> a 7.2 m x 7.2 m map
//how rugged the rugged map is (metres)
static double RUGGED_DUNES = 0.06, RUGGED_RIPPLES = 0.02, RUGGED_NOISE = 0.008;

static void printSummary(const char* name, const GridMap& map, const CostMapParameters& parameters, int start, int end) {
	const std::vector<float>& cost = map.getLayer("cost");
	int lethal = 0, zero = 0;
	double sum = 0;
	for (float c : cost) {
		if (isLethal(c, parameters)) lethal++;
		else { sum += c; if (c == 0.f) zero++; }
	}
	int walkable = map.getCellCount() - lethal;
	PathResult astar = pathfindingAstar(map, parameters, start, end);
	PathResult bfs = pathfindingBFS(map, parameters, start, end);
	std::printf("%-8s lethal %5.1f%%  zero-cost %5.1f%%  mean walkable cost %.3f | A*: %zu cells, cost %.1f | BFS: %zu cells, cost %.1f\n",
		name, 100.0 * lethal / map.getCellCount(), 100.0 * zero / map.getCellCount(), walkable ? sum / walkable : 0.0,
		astar.path.size(), astar.cost, bfs.path.size(), bfs.cost);
}

static GridMap buildFlat(const CostMapParameters& parameters) {
	GridMap map = createFlatMap(SIZE, SIZE, RESOLUTION);
	runCostMapPipeline(map, parameters);
	return map;
}

static GridMap buildRugged(const CostMapParameters& parameters) {
	GridMap map = createFlatMap(SIZE, SIZE, RESOLUTION);
	addDunes(map, RUGGED_DUNES, 2.4, 3.0, 0.6);    //long rolling dunes
	addDunes(map, RUGGED_RIPPLES, 1.0, 1.2, 1.7);  //shorter ripples on top
	addHill(map, 20, 27, 0.45, 0.35);              //steep hills: lethal cores with costly slopes around them
	addHill(map, 47, 16, 0.35, 0.45);
	addHill(map, 36, 47, 0.50, 0.40);
	addHill(map, 58, 54, 0.30, 0.50);
	addHill(map, 13, 58, 0.30, 0.35);
	addNoise(map, RUGGED_NOISE, 7);                //surface roughness everywhere
	runCostMapPipeline(map, parameters);
	return map;
}

static bool buildDungeon(const CostMapParameters& parameters, GridMap& map, int& start, int& end) {
	//old format: 400 numbers (0 ground, 1 wall, 2 start, 3 end), row by row, for a 20 x 20 map
	std::ifstream file("maps/dungeon.txt");
	std::vector<int> tiles;
	int value;
	while (file >> value) tiles.push_back(value);
	if (tiles.size() != 400) {
		std::printf("maps/dungeon.txt not found or not 20x20\n");
		return false;
	}
	//each old tile covers SIZE / 20 cells per side (3.6 for 72): every cell takes the old tile it falls in
	map = createFlatMap(SIZE, SIZE, RESOLUTION);
	map.addLayer("cost", 0.f);
	std::vector<float>& elevation = map.getLayer("elevation");
	std::vector<float>& cost = map.getLayer("cost");
	for (int row = 0; row < SIZE; row++) {
		for (int column = 0; column < SIZE; column++) {
			int tile = (row * 20 / SIZE) * 20 + column * 20 / SIZE;
			if (tiles[tile] == 1) {
				int index = map.getIndex(column, row);
				elevation[index] = 0.5f;               //walls drawn as 0.5 m blocks
				cost[index] = parameters.lethalCost;   //and lethal
			}
		}
	}
	//the start and end go in the cell at the centre of their old tile
	start = end = -1;
	for (int tile = 0; tile < 400; tile++) {
		int centre = map.getIndex(((tile % 20) * 2 + 1) * SIZE / 40, ((tile / 20) * 2 + 1) * SIZE / 40);
		if (tiles[tile] == 2) start = centre;
		if (tiles[tile] == 3) end = centre;
	}
	//no features / traversability / inflation: this map starts at the cost layer
	return true;
}

//the three passes through the hybrid map's ridge: first and last column, and how rugged each one is
struct Pass { int column0, column1; double dunes, noise; };
static const int RIDGE_ROW0 = 36, RIDGE_ROW1 = 47;
static const Pass PASSES[3] = {
	{  4, 16, 0.030, 0.014 }, //left: roughest
	{ 30, 42, 0.020, 0.010 }, //middle
	{ 56, 68, 0.010, 0.006 }, //right: smoothest
};

static GridMap buildHybrid(const CostMapParameters& parameters) {
	GridMap map = createFlatMap(SIZE, SIZE, RESOLUTION);
	//dungeon zone (rows 0-33): two 0.5 m walls make a zig-zag corridor down from the top-left room
	addBlock(map, 0, 10, 51, 11, 0.5);   //wall with a gap on the right
	addBlock(map, 20, 22, 71, 23, 0.5);  //wall with a gap on the left
	//a ridge across the whole map (rows 36-47), 0.8 m high, except in the three passes
	for (int column = 0; column < SIZE; column++) {
		bool inPass = false;
		for (const Pass& pass : PASSES) {
			if (column >= pass.column0 && column <= pass.column1) inPass = true;
		}
		if (!inPass) {
			addBlock(map, column, RIDGE_ROW0, column, RIDGE_ROW1, 0.8);
			//a rocky top, so the ridge has no flat (walkable-looking) plateau in its middle
			addNoiseInRectangle(map, column, RIDGE_ROW0, column, RIDGE_ROW1, 0.2, 100 + column);
		}
	}
	//each pass is rugged: dunes plus bumps, rougher on the left than on the right
	unsigned int seed = 11;
	for (const Pass& pass : PASSES) {
		GridMap bumps = createFlatMap(SIZE, SIZE, RESOLUTION);
		addDunes(bumps, pass.dunes, 0.7, 0.9, 0.3);
		std::vector<float>& elevation = map.getLayer("elevation");
		const std::vector<float>& bump = bumps.getLayer("elevation");
		for (int row = RIDGE_ROW0 - 2; row <= RIDGE_ROW1 + 2; row++) {
			for (int column = pass.column0; column <= pass.column1; column++) {
				elevation[map.getIndex(column, row)] += bump[map.getIndex(column, row)];
			}
		}
		addNoiseInRectangle(map, pass.column0, RIDGE_ROW0 - 2, pass.column1, RIDGE_ROW1 + 2, pass.noise, seed++);
	}
	runCostMapPipeline(map, parameters);
	return map;
}

//checks for hybrid: (1) treating every costly cell as blocked leaves no path, (2) there are three separate passes
static void checkHybrid(const GridMap& map, const CostMapParameters& parameters, int start, int end) {
	GridMap zeroOnly = map;
	for (float& c : zeroOnly.getLayer("cost")) {
		if (c > 0.f) c = parameters.lethalCost;
	}
	bool zeroCostRouteExists = !pathfindingBFS(zeroOnly, parameters, start, end).path.empty();
	std::printf("         hybrid checks: route avoiding all rugged (cost > 0) cells: %s\n",
		zeroCostRouteExists ? "EXISTS (bad)" : "none (good)");
	//each pass on its own is a way through: block the other two passes and a path must still exist
	const std::vector<float>& cost = map.getLayer("cost");
	for (int p = 0; p < 3; p++) {
		GridMap onlyThisPass = map;
		std::vector<float>& blocked = onlyThisPass.getLayer("cost");
		for (int other = 0; other < 3; other++) {
			if (other == p) continue;
			for (int row = RIDGE_ROW0; row <= RIDGE_ROW1; row++) {
				for (int column = PASSES[other].column0; column <= PASSES[other].column1; column++) {
					blocked[onlyThisPass.getIndex(column, row)] = parameters.lethalCost;
				}
			}
		}
		PathResult through = pathfindingAstar(onlyThisPass, parameters, start, end);
		std::printf("         pass %d alone: %s (cost %.1f)\n", p + 1, through.path.empty() ? "NO PATH (bad)" : "path exists", through.cost);
	}
	for (int p = 0; p < 3; p++) {
		double sum = 0; int count = 0;
		for (int row = RIDGE_ROW0; row <= RIDGE_ROW1; row++) {
			for (int column = PASSES[p].column0; column <= PASSES[p].column1; column++) {
				float c = cost[map.getIndex(column, row)];
				if (!isLethal(c, parameters)) { sum += c; count++; }
			}
		}
		std::printf("         pass %d (columns %d-%d): %d walkable cells, mean cost %.3f\n",
			p + 1, PASSES[p].column0, PASSES[p].column1, count, count ? sum / count : 0.0);
	}
}

int main() {
	CostMapParameters parameters;

	GridMap flat = buildFlat(parameters);
	int flatStart = flat.getIndex(5, 66), flatEnd = flat.getIndex(66, 5);
	printSummary("flat", flat, parameters, flatStart, flatEnd);
	saveGridMap("maps/flat.gridmap", flat, flatStart, flatEnd);

	GridMap rugged = buildRugged(parameters);
	int ruggedStart = rugged.getIndex(5, 66), ruggedEnd = rugged.getIndex(66, 5);
	printSummary("rugged", rugged, parameters, ruggedStart, ruggedEnd);
	saveGridMap("maps/rugged.gridmap", rugged, ruggedStart, ruggedEnd);

	GridMap dungeon;
	int dungeonStart, dungeonEnd;
	if (buildDungeon(parameters, dungeon, dungeonStart, dungeonEnd)) {
		printSummary("dungeon", dungeon, parameters, dungeonStart, dungeonEnd);
		saveGridMap("maps/dungeon.gridmap", dungeon, dungeonStart, dungeonEnd);
	}

	GridMap hybrid = buildHybrid(parameters);
	int hybridStart = hybrid.getIndex(5, 5), hybridEnd = hybrid.getIndex(36, 65);
	printSummary("hybrid", hybrid, parameters, hybridStart, hybridEnd);
	checkHybrid(hybrid, parameters, hybridStart, hybridEnd);
	saveGridMap("maps/hybrid.gridmap", hybrid, hybridStart, hybridEnd);
	return 0;
}
