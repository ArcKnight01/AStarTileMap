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
static int dijkstra(Board& b, int start, int end) {
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
			int n = ny * W + nx, nd = d + ((dx && dy) ? 14 : 10);
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

		std::vector<int> path = b.pathfinding(start, end);
		int best = dijkstra(b, start, end);
		if (best < 0) {
			CHECK(path.empty(), "trial %d: A* found a path where none exists", trial);
			blocked++;
		} else {
			CHECK(!path.empty(), "trial %d: A* found no path but one exists (cost %d)", trial, best);
			if (!path.empty()) {
				int cost = checkPath(b, path, start, end);
				CHECK(cost == best, "trial %d: A* path cost %d, shortest is %d", trial, cost, best);
				// searched (purple) tiles: every path tile was evaluated, and no wall ever is
				std::vector<bool> searched(b.tileCount, false);
				for (int t : b.searchedTiles) {
					CHECK(b.tileTypes[t] != wall, "trial %d: wall tile %d marked as searched", trial, t);
					searched[t] = true;
				}
				for (int t : path) CHECK(searched[t], "trial %d: path tile %d not marked as searched", trial, t);
			}
			found++;
		}
	}
	std::printf("random maps: %d with a path, %d without\n", found, blocked);

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
	std::string name;
	std::istringstream in("dungeon");
	std::streambuf* old = std::cin.rdbuf(in.rdbuf());
	b.importBoard(name);
	std::cin.rdbuf(old);
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
