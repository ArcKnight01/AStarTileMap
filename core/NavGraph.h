#pragma once
// Navigation graph, merged from the iD Tech C4-Template / newProjectiles Map.h.
// Every walkable tile becomes a vertex; each vertex lists the walkable tiles
// around it (8 directions). This is the graph A* searches.
//
// Change from the original: calcNeighbors() used to store the neighbour's
// OFFSET (e.g. {-1, 0}) in `adjacent`; it now stores the neighbour's actual
// grid coordinates, so `adjacent` can be used directly as A*'s neighbour list.
#include <map>
#include <utility>
#include <vector>

typedef std::pair<unsigned int, unsigned int> gridcoord; // (x, y)

typedef struct {
	gridcoord coords;                 // this tile
	std::vector<gridcoord> adjacent;  // walkable tiles next to it
} vertex;

class NavGraph
{
	std::map<gridcoord, vertex> vertices;

public:
	/**
	 * Add the vertex of a walkable tile.
	 *
	 * @param x  Column of the tile.
	 * @param y  Row of the tile.
	 * @param v  The vertex: its coordinates and walkable neighbours.
	 */
	void addVertex(unsigned int x, unsigned int y, vertex v) {
		vertices.insert(std::make_pair(std::make_pair(x, y), v));
	}

	// Returns false if (x, y) is not walkable (wall / off the map).
	/**
	 * Look up the vertex of a tile.
	 *
	 * @param x     Column of the tile.
	 * @param y     Row of the tile.
	 * @param vert  Filled with the vertex if it exists.
	 * @return      False if (x, y) is not walkable.
	 */
	bool getVertex(int x, int y, vertex& vert) const {
		auto found = vertices.find(std::make_pair(x, y));
		if (found != vertices.end()) {
			vert = found->second;
			return true;
		}
		return false;
	}

	/**
	 * Remove every vertex.
	 */
	void clear() { vertices.clear(); }
	/**
	 * Get the number of vertices.
	 *
	 * @return  Number of walkable tiles.
	 */
	size_t size() const { return vertices.size(); }
};
