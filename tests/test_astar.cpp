// Checks Board::pathfinding (A*) against a brute-force Dijkstra search on random maps.
// A* must find a path exactly when one exists, and its path must be as short as Dijkstra's.
// Run with:  ctest --test-dir build --output-on-failure
#include "Tile.h"
#include <cstdio>
#include <queue>
#include <random>
#include <sstream>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

// Shortest-path cost with the same moves A* uses (8 directions, 10/14 costs, no walls).
// With unitCost, every step costs 1, so the result is the fewest steps (what BFS minimises).
static int dijkstra(Board& b, int start, int end, bool unitCost = false) {
	const int W = b.tilesPerRow, H = b.tilesPerCol, INF = 1 << 30;
	std::vector<int> dist(b.tileCount, INF);
	std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>> pq;
	dist[start] = 0; pq.push({0, start});
	while (!pq.empty()) {
		auto [d, t] = pq.top(); pq.pop();
		if (d != dist[t]) continue;
		if (t == end) return d;
		int x = t % W, y = t / W;
		for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
			if (!dx && !dy) continue;
			int nx = x + dx, ny = y + dy;
			if (nx < 0 || ny < 0 || nx >= W || ny >= H || b.tileTypes[ny * W + nx] == wall) continue;
			int n = ny * W + nx, nd = d + (unitCost ? 1 : ((dx && dy) ? 14 : 10));
			if (nd < dist[n]) { dist[n] = nd; pq.push({nd, n}); }
		}
	}
	return -1; // unreachable
}

// Validates the path's shape and returns its cost.
static int checkPath(Board& b, const std::vector<int>& path, int start, int end) {
	const int W = b.tilesPerRow;
	CHECK(path.front() == start, "path does not begin at the start");
	CHECK(path.back() == end, "path does not finish at the end");
	int cost = 0;
	for (size_t k = 0; k < path.size(); k++) {
		CHECK(b.tileTypes[path[k]] != wall, "path goes through a wall at tile %d", path[k]);
		if (k == 0) continue;
		int ax = path[k - 1] % W, ay = path[k - 1] / W, bx = path[k] % W, by = path[k] / W;
		CHECK(std::max(std::abs(ax - bx), std::abs(ay - by)) == 1, "path jumps between tiles %d and %d", path[k - 1], path[k]);
		cost += b.getDistance(path[k - 1], path[k]);
	}
	return cost;
}

int main() {
	sf::RenderWindow window; // never opened; Board only needs the reference
	Board b(window, 1000, 1000, 20, 20);

	// 1) random maps
	std::mt19937 rng(12345);
	int found = 0, blocked = 0;
	for (int trial = 0; trial < 2000; trial++) {
		double wallChance = (trial % 5) * 0.1; // 0% .. 40% walls
		std::bernoulli_distribution isWall(wallChance);
		for (int i = 0; i < b.tileCount; i++) b.tileTypes[i] = isWall(rng) ? wall : ground;
		std::uniform_int_distribution<int> pick(0, b.tileCount - 1);
		int start = pick(rng), end = pick(rng);
		b.tileTypes[start] = startnode; b.tileTypes[end] = endnode;
		b.navDirty = true;

		int best = dijkstra(b, start, end);
		int fewest = dijkstra(b, start, end, true);
		if (best < 0) blocked++; else found++;

		// every algorithm: a path exactly when one exists, legal steps, path tiles searched, no wall searched
		for (PathAlgorithm algorithm : { PathAlgorithm::AStar, PathAlgorithm::Dijkstra, PathAlgorithm::BFS, PathAlgorithm::DFS }) {
			const char* algorithmName = getPathAlgorithmName(algorithm).c_str();
			std::vector<int> path = b.findPath(algorithm, start, end);
			if (best < 0) {
				CHECK(path.empty(), "trial %d: %s found a path where none exists", trial, algorithmName);
				continue;
			}
			CHECK(!path.empty(), "trial %d: %s found no path but one exists (cost %d)", trial, algorithmName, best);
			if (path.empty()) continue;
			int cost = checkPath(b, path, start, end);
			// A* and Dijkstra's must find the cheapest path; BFS the fewest steps; DFS just a valid one
			if (algorithm == PathAlgorithm::AStar || algorithm == PathAlgorithm::Dijkstra)
				CHECK(cost == best, "trial %d: %s path cost %d, shortest is %d", trial, algorithmName, cost, best);
			if (algorithm == PathAlgorithm::BFS)
				CHECK((int)path.size() - 1 == fewest, "trial %d: BFS took %d steps, fewest is %d", trial, (int)path.size() - 1, fewest);
			// searched (purple) tiles: every path tile was evaluated, and no wall ever is
			std::vector<bool> searched(b.tileCount, false);
			for (int t : b.searchedTiles) {
				CHECK(b.tileTypes[t] != wall, "trial %d: %s marked wall tile %d as searched", trial, algorithmName, t);
				searched[t] = true;
			}
			for (int t : path) CHECK(searched[t], "trial %d: %s path tile %d not marked as searched", trial, algorithmName, t);
		}
	}
	std::printf("random maps: %d with a path, %d without (each run with A*, Dijkstra's, BFS and DFS)\n", found, blocked);

	// board placed with a margin: tile <-> pixel conversion includes the origin, and clicks outside give -1
	{
		Board placed(window, 720, 720, 20, 20, sf::Vector2f(40, 40));
		CHECK(placed.getTilePos(0) == sf::Vector2f(40, 40), "margin: tile 0 starts at the board origin");
		CHECK(placed.getTileN(41, 41) == 0 && placed.getTileN(40 + 720 - 1, 40 + 720 - 1) == 399, "margin: corners map to tiles 0 and 399");
		CHECK(placed.getTileN(10, 10) == -1 && placed.getTileN(900, 300) == -1, "margin: points outside the board give -1");
	}
	// tile placement: start / end are offered only while missing (or on that very tile), Up / Down wrap, placing updates them
	{
		Board p(window, 720, 720, 20, 20);
		auto offers = [&](tiletype type) {
			return std::find(p.placementOptions.begin(), p.placementOptions.end(), type) != p.placementOptions.end();
		};
		auto choose = [&](tiletype type) { // move the preview to `type` with Down presses
			for (int i = 0; i < 10 && p.getPlacementPreview() != type; i++) p.movePlacementChoice(+1);
		};
		p.beginPlacement(45);
		CHECK(p.isPlacing && p.placementOptions.size() == 4, "placement: an empty board offers ground, wall, start and end");
		CHECK(p.getPlacementPreview() == ground, "placement: the preview starts on the tile's current type");
		p.movePlacementChoice(-1);
		CHECK(p.getPlacementPreview() == endnode, "placement: Up from the first option wraps to the last");
		p.cancelPlacement();
		CHECK(!p.isPlacing && p.tileTypes[45] == ground, "placement: unselecting changes nothing");

		p.beginPlacement(45); choose(startnode); p.confirmPlacement();
		CHECK(p.tileTypes[45] == startnode && p.startExists && p.startPos == 45 && !p.isPlacing && p.navDirty,
			"placement: placing a start sets startPos, re-plans and unselects");
		p.beginPlacement(60);
		CHECK(!offers(startnode) && offers(endnode) && p.placementOptions.size() == 3, "placement: Start is not offered once it exists");
		choose(endnode); p.confirmPlacement();
		CHECK(p.endExists && p.endPos == 60, "placement: placing an end");
		p.beginPlacement(61);
		CHECK(!offers(startnode) && !offers(endnode), "placement: neither is offered once both exist");
		p.cancelPlacement();

		p.beginPlacement(45);
		CHECK(offers(startnode) && p.getPlacementPreview() == startnode, "placement: the start tile keeps Start in its list");
		choose(ground); p.confirmPlacement();
		CHECK(p.tileTypes[45] == ground && !p.startExists, "placement: choosing Ground on the start removes it");
		p.beginPlacement(61);
		CHECK(offers(startnode), "placement: Start is offered again after it was removed");
		choose(wall); p.confirmPlacement();
		CHECK(p.tileTypes[61] == wall, "placement: placing a wall");
		p.beginPlacement(-1);
		CHECK(!p.isPlacing, "placement: selecting outside the board unselects");
	}

	// map names typed in the menu
	CHECK(Board::cleanMapName("") == "default" && Board::cleanMapName(" maps/dungeon.txt ") == "dungeon", "map names are cleaned");

	// 2) a solid wall across the middle row: start above, end below -> no path
	for (int i = 0; i < b.tileCount; i++) b.tileTypes[i] = (i / b.tilesPerRow == 10) ? wall : ground;
	b.navDirty = true;
	CHECK(b.pathfinding(2 * 20 + 5, 17 * 20 + 5).empty(), "walled-off map: A* found a path through a wall");
	// ...and one gap in that wall -> the path must go through the gap
	b.tileTypes[10 * 20 + 15] = ground;
	b.navDirty = true;
	std::vector<int> gapPath = b.pathfinding(2 * 20 + 5, 17 * 20 + 5);
	CHECK(std::find(gapPath.begin(), gapPath.end(), 10 * 20 + 15) != gapPath.end(), "walled map with a gap: path misses the gap");

	// 3) the dungeon map shipped in maps/
	std::string name = "dungeon"; // the menu's text box now supplies the name
	b.importBoard(name);
	std::vector<int> path = b.pathfinding(b.startPos, b.endPos);
	CHECK(!path.empty(), "dungeon: no path found");
	if (!path.empty()) {
		int cost = checkPath(b, path, b.startPos, b.endPos);
		CHECK(cost == dijkstra(b, b.startPos, b.endPos), "dungeon: path is not the shortest");
		std::printf("\ndungeon: %zu tiles, cost %d\n", path.size(), cost);
	}

	std::printf(failures ? "%d FAILURES\n" : "all A* checks passed\n", failures);
	return failures ? 1 : 0;
}
