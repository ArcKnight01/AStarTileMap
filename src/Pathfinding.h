#pragma once
#include <string>
#include <vector>

// The path planning algorithms the menu's dropdown can choose from.
// To add one: add it here and to getPathAlgorithmNames() (same order), write a Board::pathfindingAstar...() function in
// Pathfinding.cpp, and call it from Board::findPath().
enum class PathAlgorithm {
	AStar,
	Dijkstra,
	BFS,
	DFS
};

// The names shown in the dropdown, in the same order as PathAlgorithm.
inline std::vector<std::string> getPathAlgorithmNames() {
	return { "A*", "Dijkstra's", "BFS", "DFS" };
}

inline std::string getPathAlgorithmName(PathAlgorithm algorithm) {
	return getPathAlgorithmNames()[(int)algorithm];
}
