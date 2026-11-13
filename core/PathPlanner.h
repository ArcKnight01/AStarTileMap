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
/**
 * Get the display names of the algorithms.
 *
 * @return  The names, in the order of PathAlgorithm.
 */
inline std::vector<std::string> getPathAlgorithmNames() {
	return { "A*", "Dijkstra's", "BFS", "DFS" };
}
/**
 * Get the display name of an algorithm.
 *
 * @param algorithm  The algorithm.
 * @return           Its name, e.g. "A*".
 */
inline std::string getPathAlgorithmName(PathAlgorithm algorithm) {
	return getPathAlgorithmNames()[(int)algorithm];
}

struct PathResult {
	std::vector<int> path;          //cell indices from start to end (both included); empty if there is no path
	std::vector<int> searchedCells; //every cell the algorithm evaluated, in order (drawn purple)
	double cost = 0;                //total step cost of the path (eq. 3.8), comparable between algorithms
};

/**
 * Walkable (non-lethal) cells and their walkable 8 neighbours.
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  lethalCost.
 * @return            The navigation graph, one vertex per walkable cell.
 */
NavGraph buildNavGraph(const GridMap& map, const CostMapParameters& parameters);
/**
 * Euclidean distance between two cells, in cells (eq. 3.1).
 *
 * @param map    The map (for the cells' columns and rows).
 * @param cellA  First cell (row * columns + column).
 * @param cellB  Second cell (row * columns + column).
 * @return       Distance in cells.
 */
double getHeuristic(const GridMap& map, int cellA, int cellB);
/**
 * Cost of stepping from one cell to a neighbour (eq. 3.8).
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  costWeight.
 * @param fromCell    Cell stepped from (row * columns + column).
 * @param toCell      Neighbouring cell stepped to.
 * @return            h(fromCell, toCell) + costWeight * cost(toCell).
 */
double getStepCost(const GridMap& map, const CostMapParameters& parameters, int fromCell, int toCell);
/**
 * Total step cost along a path.
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  costWeight.
 * @param path        Cell indices from start to end.
 * @return            Sum of the step costs; 0 for a path of fewer than two cells.
 */
double getPathCost(const GridMap& map, const CostMapParameters& parameters, const std::vector<int>& path);

/**
 * Runs the chosen algorithm.
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  lethalCost and costWeight.
 * @param algorithm   The algorithm to run.
 * @param start       Start cell (row * columns + column).
 * @param end         End cell (row * columns + column).
 * @return            Path, searched cells and path cost; the path is empty if there is none.
 */
PathResult findPath(const GridMap& map, const CostMapParameters& parameters, PathAlgorithm algorithm, int start, int end);
/**
 * A* (Sebastian Lague's simple version, with Rajput's step cost): cheapest path, searches toward the end.
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  lethalCost and costWeight.
 * @param start       Start cell (row * columns + column).
 * @param end         End cell (row * columns + column).
 * @return            Path, searched cells and path cost; the path is empty if there is none.
 */
PathResult pathfindingAstar(const GridMap& map, const CostMapParameters& parameters, int start, int end);
/**
 * Dijkstra's: A* without the heuristic; cheapest path, spreads out evenly.
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  lethalCost and costWeight.
 * @param start       Start cell (row * columns + column).
 * @param end         End cell (row * columns + column).
 * @return            Path, searched cells and path cost; the path is empty if there is none.
 */
PathResult pathfindingDijkstra(const GridMap& map, const CostMapParameters& parameters, int start, int end);
/**
 * Breadth-first search: fewest steps, ignores cost.
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  lethalCost.
 * @param start       Start cell (row * columns + column).
 * @param end         End cell (row * columns + column).
 * @return            Path, searched cells and path cost; the path is empty if there is none.
 */
PathResult pathfindingBFS(const GridMap& map, const CostMapParameters& parameters, int start, int end);
/**
 * Depth-first search: some path, usually long, ignores cost.
 *
 * @param map         Map with a "cost" layer.
 * @param parameters  lethalCost.
 * @param start       Start cell (row * columns + column).
 * @param end         End cell (row * columns + column).
 * @return            Path, searched cells and path cost; the path is empty if there is none.
 */
PathResult pathfindingDFS(const GridMap& map, const CostMapParameters& parameters, int start, int end);
