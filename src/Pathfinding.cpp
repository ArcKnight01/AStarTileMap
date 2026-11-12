#include "Tile.h"

// Path planning on the board's navigation graph (moved out of Tile.cpp).
// Each algorithm returns the tile indices from start (nodeA) to end (nodeB), both included, or an empty list if there
// is no path, and fills searchedTiles with every tile it evaluated, in order (drawn purple; the path is drawn blue).

// Distance between two tiles in A* cost units: 10 per straight step, 14 per diagonal step
// (14 ~= 10 * sqrt(2)). Same as Sebastian Lague's GetDistance().
int Board::getDistance(int tileA, int tileB)
{
	int dstX = abs(tileA % tilesPerRow - tileB % tilesPerRow);
	int dstY = abs(tileA / tilesPerRow - tileB / tilesPerRow);
	if (dstX > dstY) {
		return 14 * dstY + 10 * (dstX - dstY);
	}
	return 14 * dstX + 10 * (dstY - dstX);
}

// A* path planning, following Sebastian Lague's
// "A* Pathfinding (E01: algorithm explanation)": https://www.youtube.com/watch?v=-L-WgKMFuhE
// Simplest version: OPEN is a plain list searched for the lowest F cost each step.
// Returns the tile indices from nodeA (start) to nodeB (end), both included,
// or an empty list if there is no path.
std::vector<int> Board::pathfindingAstar(int nodeA, int nodeB)
{
	std::vector<int> path;
	searchedTiles.clear();
	if (nodeA < 0 || nodeA >= tileCount || nodeB < 0 || nodeB >= tileCount) {
		return path;
	}
	if (navDirty) {
		recalculateNav();
	}

	//per-tile node data
	//G cost = distance from the start, H cost = estimated distance to the end, F cost = G + H
	std::vector<int> gCost(tileCount, 0);
	std::vector<int> hCost(tileCount, 0);
	std::vector<int> parent(tileCount, -1);   //tile we came from (-1 = none)
	std::vector<bool> inOpen(tileCount, false);
	std::vector<bool> inClosed(tileCount, false);

	//OPEN : the set of nodes to be evaluated
	std::vector<int> openNodes;
	//CLOSED : the set of nodes already evaluated (tracked with inClosed)

	//add the start node to OPEN:
	openNodes.push_back(nodeA);
	inOpen[nodeA] = true;
	hCost[nodeA] = getDistance(nodeA, nodeB);

	while (!openNodes.empty()) {
		//current = the node in OPEN with the lowest F cost (if tied, the lowest H cost)
		int best = 0;
		for (int k = 1; k < (int)openNodes.size(); k++) {
			int candidate = openNodes[k];
			int current = openNodes[best];
			int fCandidate = gCost[candidate] + hCost[candidate];
			int fCurrent = gCost[current] + hCost[current];
			if (fCandidate < fCurrent || (fCandidate == fCurrent && hCost[candidate] < hCost[current])) {
				best = k;
			}
		}
		int current = openNodes[best];

		//remove current from OPEN, add it to CLOSED
		openNodes.erase(openNodes.begin() + best);
		inOpen[current] = false;
		inClosed[current] = true;
		searchedTiles.push_back(current); //shown in purple

		//path has been found: retrace it from the end back to the start
		if (current == nodeB) {
			for (int tile = nodeB; tile != -1; tile = parent[tile]) {
				path.push_back(tile);
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		//for each neighbour of the current node (walls are never in the nav graph)
		vertex v;
		if (!nav.getVertex(current % tilesPerRow, current / tilesPerRow, v)) {
			continue;
		}
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = coord.second * tilesPerRow + coord.first;

			//skip it if it has already been evaluated
			if (inClosed[neighbour]) {
				continue;
			}

			//if the new path to the neighbour is shorter, or the neighbour is not in OPEN yet
			int newCostToNeighbour = gCost[current] + getDistance(current, neighbour);
			if (newCostToNeighbour < gCost[neighbour] || !inOpen[neighbour]) {
				gCost[neighbour] = newCostToNeighbour;
				hCost[neighbour] = getDistance(neighbour, nodeB);
				parent[neighbour] = current;

				if (!inOpen[neighbour]) {
					openNodes.push_back(neighbour);
					inOpen[neighbour] = true;
				}
			}
		}
	}

	//OPEN ran out without reaching the end: no path exists
	return path;
}

// Runs whichever algorithm was chosen in the menu.
std::vector<int> Board::findPath(PathAlgorithm algorithm, int nodeA, int nodeB)
{
	switch (algorithm) {
	case PathAlgorithm::AStar:    return pathfindingAstar(nodeA, nodeB);
	case PathAlgorithm::Dijkstra: return pathfindingDijkstra(nodeA, nodeB);
	case PathAlgorithm::BFS:      return pathfindingBFS(nodeA, nodeB);
	case PathAlgorithm::DFS:      return pathfindingDFS(nodeA, nodeB);
	}
	return pathfindingAstar(nodeA, nodeB);
}

// Dijkstra's algorithm: the same as the A* above, but with no H cost (no estimate of the distance to the end).
// It always evaluates the cheapest tile found so far, so it spreads out evenly in every direction, searches more
// tiles than A*, and finds an equally cheap path.
std::vector<int> Board::pathfindingDijkstra(int nodeA, int nodeB)
{
	std::vector<int> path;
	searchedTiles.clear();
	if (nodeA < 0 || nodeA >= tileCount || nodeB < 0 || nodeB >= tileCount) {
		return path;
	}
	if (navDirty) {
		recalculateNav();
	}

	//G cost = distance from the start (Dijkstra's only has G)
	std::vector<int> gCost(tileCount, 0);
	std::vector<int> parent(tileCount, -1);
	std::vector<bool> inOpen(tileCount, false);
	std::vector<bool> inClosed(tileCount, false);

	//OPEN : the set of nodes to be evaluated
	std::vector<int> openNodes;
	openNodes.push_back(nodeA);
	inOpen[nodeA] = true;

	while (!openNodes.empty()) {
		//current = the node in OPEN with the lowest G cost
		int best = 0;
		for (int k = 1; k < (int)openNodes.size(); k++) {
			if (gCost[openNodes[k]] < gCost[openNodes[best]]) {
				best = k;
			}
		}
		int current = openNodes[best];

		//remove current from OPEN, add it to CLOSED
		openNodes.erase(openNodes.begin() + best);
		inOpen[current] = false;
		inClosed[current] = true;
		searchedTiles.push_back(current); //shown in purple

		//path has been found: retrace it from the end back to the start
		if (current == nodeB) {
			for (int tile = nodeB; tile != -1; tile = parent[tile]) {
				path.push_back(tile);
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		//for each neighbour of the current node
		vertex v;
		if (!nav.getVertex(current % tilesPerRow, current / tilesPerRow, v)) {
			continue;
		}
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = coord.second * tilesPerRow + coord.first;
			if (inClosed[neighbour]) {
				continue;
			}
			int newCostToNeighbour = gCost[current] + getDistance(current, neighbour);
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

	//OPEN ran out without reaching the end: no path exists
	return path;
}

// Breadth-first search: explores the map in rings around the start, using a queue (first in, first out).
// It finds the path with the fewest steps, but treats a diagonal step the same as a straight one, so its path can
// cost more than A*'s.
std::vector<int> Board::pathfindingBFS(int nodeA, int nodeB)
{
	std::vector<int> path;
	searchedTiles.clear();
	if (nodeA < 0 || nodeA >= tileCount || nodeB < 0 || nodeB >= tileCount) {
		return path;
	}
	if (navDirty) {
		recalculateNav();
	}

	std::vector<int> parent(tileCount, -1);
	std::vector<bool> discovered(tileCount, false); //already put in the queue
	std::vector<int> queue;                          //tiles waiting to be evaluated, oldest first
	std::size_t front = 0;                           //index of the oldest tile still waiting

	queue.push_back(nodeA);
	discovered[nodeA] = true;

	while (front < queue.size()) {
		//current = the tile that has waited longest
		int current = queue[front];
		front++;
		searchedTiles.push_back(current); //shown in purple

		if (current == nodeB) {
			for (int tile = nodeB; tile != -1; tile = parent[tile]) {
				path.push_back(tile);
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		vertex v;
		if (!nav.getVertex(current % tilesPerRow, current / tilesPerRow, v)) {
			continue;
		}
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = coord.second * tilesPerRow + coord.first;
			if (!discovered[neighbour]) {
				discovered[neighbour] = true;
				parent[neighbour] = current;
				queue.push_back(neighbour);
			}
		}
	}

	//the queue ran out without reaching the end: no path exists
	return path;
}

// Depth-first search: always continues from the newest tile it found, using a stack (last in, first out), so it
// follows one direction as far as it can before backing up. It finds a path if one exists, but usually a long,
// winding one.
std::vector<int> Board::pathfindingDFS(int nodeA, int nodeB)
{
	std::vector<int> path;
	searchedTiles.clear();
	if (nodeA < 0 || nodeA >= tileCount || nodeB < 0 || nodeB >= tileCount) {
		return path;
	}
	if (navDirty) {
		recalculateNav();
	}

	std::vector<int> parent(tileCount, -1);
	std::vector<bool> visited(tileCount, false);
	//the stack holds (tile, the tile it was reached from)
	std::vector<std::pair<int, int>> stack;
	stack.push_back(std::make_pair(nodeA, -1));

	while (!stack.empty()) {
		//current = the newest tile on the stack
		std::pair<int, int> top = stack.back();
		stack.pop_back();
		int current = top.first;
		if (visited[current]) {
			continue;
		}
		visited[current] = true;
		parent[current] = top.second;
		searchedTiles.push_back(current); //shown in purple

		if (current == nodeB) {
			for (int tile = nodeB; tile != -1; tile = parent[tile]) {
				path.push_back(tile);
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		vertex v;
		if (!nav.getVertex(current % tilesPerRow, current / tilesPerRow, v)) {
			continue;
		}
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = coord.second * tilesPerRow + coord.first;
			if (!visited[neighbour]) {
				stack.push_back(std::make_pair(neighbour, current));
			}
		}
	}

	//the stack ran out without reaching the end: no path exists
	return path;
}
