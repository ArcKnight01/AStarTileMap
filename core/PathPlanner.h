#pragma once
#include <string>
#include <vector>
#include "CostMapParameters.h"
#include "GridMap.h"
#include "NavGraph.h"

// Path planning on the "cost" layer of a GridMap. Lethal cells (cost >= lethalCost) are left out of the navigation
// graph, like walls were; every other cell can be entered, at a price.
//
// Step cost (Rajput, thesis eq. 3.8, ground nodes):  g(n) = g(c) + h(c, n) + costWeight * cost(n)
//   h(a, b) = Euclidean distance in cells (eq. 3.1): 1 for a straight step, sqrt(2) for a diagonal one.
// Because every cost is >= 0, h never overestimates, so A* still finds the cheapest path.

// The algorithms the menu's dropdown (and the PLAYING side panel's dropdown) can choose from.
// To add one: add it here and to getPathAlgorithmNames() (same order), write a pathfinding...() function in
// PathPlanner.cpp, and call it from findPath().
enum class PathAlgorithm {
	AStar,
	Dijkstra,
	BFS,
	DFS
};
inline std::vector<std::string> getPathAlgorithmNames() {
	return { "A*", "Dijkstra's", "BFS", "DFS" };
}
inline std::string getPathAlgorithmName(PathAlgorithm algorithm) {
	return getPathAlgorithmNames()[(int)algorithm];
}

struct PathResult {
	std::vector<int> path;          //cell indices from start to end (both included); empty if there is no path
	std::vector<int> searchedCells; //every cell the algorithm evaluated, in order (drawn purple)
	double cost = 0;                //total step cost of the path (eq. 3.8), comparable between algorithms
};

//walkable (non-lethal) cells and their walkable 8 neighbours
NavGraph buildNavGraph(const GridMap& map, const CostMapParameters& parameters);
//Euclidean distance between two cells, in cells (eq. 3.1)
double getHeuristic(const GridMap& map, int cellA, int cellB);
//cost of stepping from one cell to a neighbour (eq. 3.8)
double getStepCost(const GridMap& map, const CostMapParameters& parameters, int fromCell, int toCell);
//total step cost along a path
double getPathCost(const GridMap& map, const CostMapParameters& parameters, const std::vector<int>& path);

//runs the chosen algorithm
PathResult findPath(const GridMap& map, const CostMapParameters& parameters, PathAlgorithm algorithm, int start, int end);
//A* (Sebastian Lague's simple version, with Rajput's step cost): cheapest path, searches toward the end
PathResult pathfindingAstar(const GridMap& map, const CostMapParameters& parameters, int start, int end);
//Dijkstra's: A* without the heuristic; cheapest path, spreads out evenly
PathResult pathfindingDijkstra(const GridMap& map, const CostMapParameters& parameters, int start, int end);
//breadth-first search: fewest steps, ignores cost
PathResult pathfindingBFS(const GridMap& map, const CostMapParameters& parameters, int start, int end);
//depth-first search: some path, usually long, ignores cost
PathResult pathfindingDFS(const GridMap& map, const CostMapParameters& parameters, int start, int end);
