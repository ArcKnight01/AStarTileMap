#include "PathPlanner.h"
#include <algorithm>
#include <cmath>
#include "CostMap.h"

NavGraph buildNavGraph(const GridMap& map, const CostMapParameters& parameters) {
	NavGraph nav;
	const std::vector<float>& cost = map.getLayer("cost");
	int offsets[8][2] = { {-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1} };
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			if (isLethal(cost[map.getIndex(column, row)], parameters)) continue;
			vertex v;
			v.coords = std::make_pair(column, row);
			for (auto& offset : offsets) {
				int c = column + offset[0], r = row + offset[1];
				if (map.isInside(c, r) && !isLethal(cost[map.getIndex(c, r)], parameters)) {
					v.adjacent.push_back(std::make_pair(c, r));
				}
			}
			nav.addVertex(column, row, v);
		}
	}
	return nav;
}

double getHeuristic(const GridMap& map, int cellA, int cellB) {
	double dx = map.getColumn(cellA) - map.getColumn(cellB);
	double dy = map.getRow(cellA) - map.getRow(cellB);
	return std::sqrt(dx * dx + dy * dy);
}

double getStepCost(const GridMap& map, const CostMapParameters& parameters, int fromCell, int toCell) {
	return getHeuristic(map, fromCell, toCell) + parameters.costWeight * map.getLayer("cost")[toCell];
}

double getPathCost(const GridMap& map, const CostMapParameters& parameters, const std::vector<int>& path) {
	double total = 0;
	for (std::size_t i = 1; i < path.size(); i++) {
		total += getStepCost(map, parameters, path[i - 1], path[i]);
	}
	return total;
}

//shared by every algorithm: start and end must be on the map and walkable
static bool isPlannable(const GridMap& map, const NavGraph& nav, int start, int end) {
	vertex v;
	return start >= 0 && start < map.getCellCount() && end >= 0 && end < map.getCellCount()
		&& nav.getVertex(map.getColumn(start), map.getRow(start), v) && nav.getVertex(map.getColumn(end), map.getRow(end), v);
}

//follows parent from the end back to the start, then reverses
static std::vector<int> retracePath(const std::vector<int>& parent, int end) {
	std::vector<int> path;
	for (int cell = end; cell != -1; cell = parent[cell]) {
		path.push_back(cell);
	}
	std::reverse(path.begin(), path.end());
	return path;
}

PathResult findPath(const GridMap& map, const CostMapParameters& parameters, PathAlgorithm algorithm, int start, int end) {
	switch (algorithm) {
	case PathAlgorithm::AStar:    return pathfindingAstar(map, parameters, start, end);
	case PathAlgorithm::Dijkstra: return pathfindingDijkstra(map, parameters, start, end);
	case PathAlgorithm::BFS:      return pathfindingBFS(map, parameters, start, end);
	case PathAlgorithm::DFS:      return pathfindingDFS(map, parameters, start, end);
	}
	return pathfindingAstar(map, parameters, start, end);
}

// A* path planning, following Sebastian Lague's
// "A* Pathfinding (E01: algorithm explanation)": https://www.youtube.com/watch?v=-L-WgKMFuhE
// with Rajput's ground step cost (eq. 3.8). OPEN is a plain list searched for the lowest F cost each step.
PathResult pathfindingAstar(const GridMap& map, const CostMapParameters& parameters, int start, int end) {
	PathResult result;
	NavGraph nav = buildNavGraph(map, parameters);
	if (!isPlannable(map, nav, start, end)) {
		return result;
	}

	//per-cell node data: G cost = cost from the start, H cost = estimated cost to the end, F = G + H
	int cellCount = map.getCellCount();
	std::vector<double> gCost(cellCount, 0);
	std::vector<double> hCost(cellCount, 0);
	std::vector<int> parent(cellCount, -1);
	std::vector<bool> inOpen(cellCount, false);
	std::vector<bool> inClosed(cellCount, false);

	//OPEN : the set of nodes to be evaluated
	std::vector<int> openNodes;
	openNodes.push_back(start);
	inOpen[start] = true;
	hCost[start] = getHeuristic(map, start, end);

	while (!openNodes.empty()) {
		//current = the node in OPEN with the lowest F cost (if tied, the lowest H cost)
		int best = 0;
		for (int k = 1; k < (int)openNodes.size(); k++) {
			int candidate = openNodes[k];
			int current = openNodes[best];
			double fCandidate = gCost[candidate] + hCost[candidate];
			double fCurrent = gCost[current] + hCost[current];
			if (fCandidate < fCurrent || (fCandidate == fCurrent && hCost[candidate] < hCost[current])) {
				best = k;
			}
		}
		int current = openNodes[best];

		//remove current from OPEN, add it to CLOSED
		openNodes.erase(openNodes.begin() + best);
		inOpen[current] = false;
		inClosed[current] = true;
		result.searchedCells.push_back(current); //shown in purple

		//path has been found: retrace it from the end back to the start
		if (current == end) {
			result.path = retracePath(parent, end);
			result.cost = getPathCost(map, parameters, result.path);
			return result;
		}

		//for each neighbour of the current node (lethal cells are never in the nav graph)
		vertex v;
		nav.getVertex(map.getColumn(current), map.getRow(current), v);
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = map.getIndex(coord.first, coord.second);
			if (inClosed[neighbour]) {
				continue;
			}
			//if the new path to the neighbour is cheaper, or the neighbour is not in OPEN yet
			double newCostToNeighbour = gCost[current] + getStepCost(map, parameters, current, neighbour);
			if (newCostToNeighbour < gCost[neighbour] || !inOpen[neighbour]) {
				gCost[neighbour] = newCostToNeighbour;
				hCost[neighbour] = getHeuristic(map, neighbour, end);
				parent[neighbour] = current;
				if (!inOpen[neighbour]) {
					openNodes.push_back(neighbour);
					inOpen[neighbour] = true;
				}
			}
		}
	}

	//OPEN ran out without reaching the end: no path exists
	return result;
}

// Dijkstra's algorithm: the A* above without the H cost, so it always evaluates the cheapest cell found so far.
PathResult pathfindingDijkstra(const GridMap& map, const CostMapParameters& parameters, int start, int end) {
	PathResult result;
	NavGraph nav = buildNavGraph(map, parameters);
	if (!isPlannable(map, nav, start, end)) {
		return result;
	}
	int cellCount = map.getCellCount();
	std::vector<double> gCost(cellCount, 0);
	std::vector<int> parent(cellCount, -1);
	std::vector<bool> inOpen(cellCount, false);
	std::vector<bool> inClosed(cellCount, false);
	std::vector<int> openNodes;
	openNodes.push_back(start);
	inOpen[start] = true;

	while (!openNodes.empty()) {
		//current = the node in OPEN with the lowest G cost
		int best = 0;
		for (int k = 1; k < (int)openNodes.size(); k++) {
			if (gCost[openNodes[k]] < gCost[openNodes[best]]) {
				best = k;
			}
		}
		int current = openNodes[best];
		openNodes.erase(openNodes.begin() + best);
		inOpen[current] = false;
		inClosed[current] = true;
		result.searchedCells.push_back(current);

		if (current == end) {
			result.path = retracePath(parent, end);
			result.cost = getPathCost(map, parameters, result.path);
			return result;
		}

		vertex v;
		nav.getVertex(map.getColumn(current), map.getRow(current), v);
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = map.getIndex(coord.first, coord.second);
			if (inClosed[neighbour]) {
				continue;
			}
			double newCostToNeighbour = gCost[current] + getStepCost(map, parameters, current, neighbour);
			if (newCostToNeighbour < gCost[neighbour] || !inOpen[neighbour]) {
				gCost[neighbour] = newCostToNeighbour;
				parent[neighbour] = current;
				if (!inOpen[neighbour]) {
					openNodes.push_back(neighbour);
					inOpen[neighbour] = true;
				}
			}
		}
	}
	return result;
}

// Breadth-first search: a queue (first in, first out). Fewest steps; it ignores the cost layer apart from lethal cells.
PathResult pathfindingBFS(const GridMap& map, const CostMapParameters& parameters, int start, int end) {
	PathResult result;
	NavGraph nav = buildNavGraph(map, parameters);
	if (!isPlannable(map, nav, start, end)) {
		return result;
	}
	std::vector<int> parent(map.getCellCount(), -1);
	std::vector<bool> discovered(map.getCellCount(), false);
	std::vector<int> queue;
	std::size_t front = 0;
	queue.push_back(start);
	discovered[start] = true;

	while (front < queue.size()) {
		int current = queue[front];
		front++;
		result.searchedCells.push_back(current);
		if (current == end) {
			result.path = retracePath(parent, end);
			result.cost = getPathCost(map, parameters, result.path);
			return result;
		}
		vertex v;
		nav.getVertex(map.getColumn(current), map.getRow(current), v);
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = map.getIndex(coord.first, coord.second);
			if (!discovered[neighbour]) {
				discovered[neighbour] = true;
				parent[neighbour] = current;
				queue.push_back(neighbour);
			}
		}
	}
	return result;
}

// Depth-first search: a stack (last in, first out) of (cell, the cell it was reached from). Finds a path if one
// exists, usually a long, winding one; ignores the cost layer apart from lethal cells.
PathResult pathfindingDFS(const GridMap& map, const CostMapParameters& parameters, int start, int end) {
	PathResult result;
	NavGraph nav = buildNavGraph(map, parameters);
	if (!isPlannable(map, nav, start, end)) {
		return result;
	}
	std::vector<int> parent(map.getCellCount(), -1);
	std::vector<bool> visited(map.getCellCount(), false);
	std::vector<std::pair<int, int>> stack;
	stack.push_back(std::make_pair(start, -1));

	while (!stack.empty()) {
		std::pair<int, int> top = stack.back();
		stack.pop_back();
		int current = top.first;
		if (visited[current]) {
			continue;
		}
		visited[current] = true;
		parent[current] = top.second;
		result.searchedCells.push_back(current);
		if (current == end) {
			result.path = retracePath(parent, end);
			result.cost = getPathCost(map, parameters, result.path);
			return result;
		}
		vertex v;
		nav.getVertex(map.getColumn(current), map.getRow(current), v);
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = map.getIndex(coord.first, coord.second);
			if (!visited[neighbour]) {
				stack.push_back(std::make_pair(neighbour, current));
			}
		}
	}
	return result;
}
