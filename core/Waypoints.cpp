#include "Waypoints.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include "CostMap.h"
#include "PathPlanner.h"

std::vector<int> getCellsOnSegment(const GridMap& map, int fromCell, int toCell, std::vector<bool>* onlyCornerTouched) {


	// pseudo-code (Amanatides & Woo 1987, from cell centre to cell centre, in cells):
	// steps n_x = |dx| boundaries in x, n_y = |dy| in y;  the k-th x boundary is crossed at t = ( 2k + 1 ) / ( 2 |dx| )
	// repeat until the last cell: step in x if the next x boundary comes first, in y if the next y one does,
	//                             both (adding the two side cells first) if they come at the same t (a grid corner)
	// compare ( 2 k_x + 1 ) |dy|  with  ( 2 k_y + 1 ) |dx|  in integers, so a corner is found exactly


	int column  = map.getColumn(fromCell), row = map.getRow(fromCell);
	int dx      = map.getColumn(toCell) - column, dy = map.getRow(toCell) - row;
	int stepX   = (dx > 0) - (dx < 0), stepY = (dy > 0) - (dy < 0);
	int lengthX = std::abs(dx), lengthY = std::abs(dy);
	int crossedX = 0, crossedY = 0; //boundaries crossed so far in x and in y
	std::vector<int> cells = { fromCell };
	std::vector<bool> cornerFlags = { false };


	while (crossedX < lengthX || crossedY < lengthY) {
		//which boundary comes next: x if only x is left, y if only y is left, else compare their t in integers
		long long nextX = (long long)(2 * crossedX + 1) * lengthY;
		long long nextY = (long long)(2 * crossedY + 1) * lengthX;
		bool stepInX = crossedY >= lengthY || (crossedX < lengthX && nextX < nextY);
		bool stepInY = crossedX >= lengthX || (crossedY < lengthY && nextY < nextX);
		if (!stepInX && !stepInY) {
			//through a grid corner: the line touches both side cells, then the diagonal one
			if (map.isInside(column + stepX, row)) { cells.push_back(map.getIndex(column + stepX, row)); cornerFlags.push_back(true); }
			if (map.isInside(column, row + stepY)) { cells.push_back(map.getIndex(column, row + stepY)); cornerFlags.push_back(true); }
			stepInX = stepInY = true;
		}
		if (stepInX) { column += stepX; crossedX++; }
		if (stepInY) { row += stepY; crossedY++; }
		cells.push_back(map.getIndex(column, row));
		cornerFlags.push_back(false);
	}


	if (onlyCornerTouched) *onlyCornerTouched = cornerFlags;
	return cells;
}

double computeSegmentCost(const GridMap& map, const CostMapParameters& parameters, int fromCell, int toCell) {


	// pseudo-code (eq. 3.8 terms along a straight line):
	// cost = h( from, to ) + costWeight * sum over the cells passed through of cost( cell )  (infinity if one is lethal / unknown)
	// a cell only touched at a grid corner is skipped: the line spends no distance in it, and A* itself steps diagonally
	// past lethal corners (buildNavGraph allows it), so a line through such a corner is no riskier than the path.
	// (Blocking on lethal corner cells made pruning keep every cell of A*'s diagonal runs beside walls. If A* stops
	//  cutting corners, these cells never come up on a pruned line.)


	const std::vector<float>& cost = map.getLayer("cost");
	double total = getHeuristic(map, fromCell, toCell);
	std::vector<bool> onlyCornerTouched;
	std::vector<int> cells = getCellsOnSegment(map, fromCell, toCell, &onlyCornerTouched);


	for (std::size_t i = 0; i < cells.size(); i++) {
		if (onlyCornerTouched[i]) continue;
		if (isLethal(cost[cells[i]], parameters)) return std::numeric_limits<double>::infinity(); //NaN counts as lethal
		if (i > 0) total += parameters.costWeight * cost[cells[i]];
	}


	return total;
}

std::vector<int> prunePath(const GridMap& map, const CostMapParameters& parameters, const std::vector<int>& path) {


	// pseudo-code (thesis Algorithm 3, walking forward):
	// anchor = path[ 0 ];  keep anchor
	// while anchor is not the end:
	//     reach = the farthest path[ k ] after anchor such that the straight line anchor -> path[ k ] is allowed for
	//             every k up to it: computeSegmentCost <= the path's own cost from anchor to path[ k ]
	//     keep path[ reach ];  anchor = reach


	if (path.size() <= 2) return path;
	std::vector<int> pruned = { path[0] };
	std::size_t anchor = 0;


	while (anchor < path.size() - 1) {
		std::size_t reach = anchor + 1;
		double pathCost = getStepCost(map, parameters, path[anchor], path[anchor + 1]);
		for (std::size_t next = anchor + 2; next < path.size(); next++) {
			pathCost += getStepCost(map, parameters, path[next - 1], path[next]);
			if (computeSegmentCost(map, parameters, path[anchor], path[next]) > pathCost + 1e-9) break;
			reach = next;
		}
		pruned.push_back(path[reach]);
		anchor = reach;
	}


	return pruned;
}

std::vector<Waypoint> computeWaypointPositions(const GridMap& map, const std::vector<int>& path, double spacing) {


	// pseudo-code:
	// for each stretch from corner a to corner b (length L):  n = ceil( L / spacing ) pieces
	//     waypoints a + ( b - a ) * j / n  for j = 0 .. n - 1;   then the last corner


	std::vector<Waypoint> waypoints;
	for (std::size_t i = 0; i < path.size(); i++) {
		Waypoint corner;
		map.getPosition(map.getColumn(path[i]), map.getRow(path[i]), corner.x, corner.y);
		if (i == 0) {
			waypoints.push_back(corner);
			continue;
		}


		const Waypoint start = waypoints.back();
		double length = std::hypot(corner.x - start.x, corner.y - start.y);
		int pieces = std::max(1, (int)std::ceil(length / spacing - 1e-9));
		for (int j = 1; j < pieces; j++) {
			Waypoint between;
			between.x = start.x + (corner.x - start.x) * j / pieces;
			between.y = start.y + (corner.y - start.y) * j / pieces;
			waypoints.push_back(between);
		}
		waypoints.push_back(corner);
	}


	return waypoints;
}

void computeWaypointYaws(std::vector<Waypoint>& waypoints, double goalYaw) {


	// pseudo-code:
	// yaw_k = atan2( y_{k+1} - y_k, x_{k+1} - x_k );   yaw_last = goalYaw, or yaw_{last-1} if goalYaw is NaN


	for (std::size_t k = 0; k + 1 < waypoints.size(); k++) {
		waypoints[k].yaw = std::atan2(waypoints[k + 1].y - waypoints[k].y, waypoints[k + 1].x - waypoints[k].x);
	}
	if (waypoints.empty()) return;
	if (!std::isnan(goalYaw)) waypoints.back().yaw = goalYaw;
	else if (waypoints.size() > 1) waypoints.back().yaw = waypoints[waypoints.size() - 2].yaw;
}

void computeWaypointTimes(std::vector<Waypoint>& waypoints, const GridMap& map, const WaypointParameters& waypointParameters) {


	// pseudo-code:
	// t_0 = 0;  t_k = t_{k-1} + | p_k - p_{k-1} | / speed_k,  speed_k = nominalSpeed / ( 1 + costSlowdown * cost_k )
	// cost_k = cost where the piece arrives, read just before p_k (p_k - 1e-6 ( p_k - p_{k-1} )), clamped to 0 .. 1
	// (read just before: a waypoint can sit exactly on a grid corner, whose cell may be a lethal one the piece only
	//  touches; clamped: walkable costs are 0 .. 0.5, so a lethal value never makes a piece take 60 times longer)
	// (cost 0 where the map has no cost layer, or the cost is unknown)


	bool hasCost = map.hasLayer("cost");
	double time  = 0;
	for (std::size_t k = 0; k < waypoints.size(); k++) {
		if (k > 0) {
			double cost = 0;
			int column, row;
			double arriveX = waypoints[k].x - 1e-6 * (waypoints[k].x - waypoints[k - 1].x);
			double arriveY = waypoints[k].y - 1e-6 * (waypoints[k].y - waypoints[k - 1].y);
			if (hasCost && map.getCell(arriveX, arriveY, column, row)) {
				float value = map.at("cost", column, row);
				if (!GridMap::isUnknown(value)) cost = std::max(0.f, std::min(1.f, value));
			}
			double speed = waypointParameters.nominalSpeed / (1 + waypointParameters.costSlowdown * cost);
			time += std::hypot(waypoints[k].x - waypoints[k - 1].x, waypoints[k].y - waypoints[k - 1].y) / std::max(1e-6, speed);
		}
		waypoints[k].time = time;
	}
}

std::vector<Waypoint> makeWaypoints(const GridMap& map, const std::vector<int>& path,
	const WaypointParameters& waypointParameters, double goalYaw) {


	// pseudo-code:
	// waypoints = computeWaypointPositions( path, spacing );  computeWaypointYaws;  computeWaypointTimes


	double spacing = std::max(map.getResolution(), waypointParameters.spacing);
	std::vector<Waypoint> waypoints = computeWaypointPositions(map, path, spacing);
	computeWaypointYaws(waypoints, goalYaw);
	computeWaypointTimes(waypoints, map, waypointParameters);


	return waypoints;
}

std::string getWaypointListText(const std::vector<Waypoint>& waypoints, int decimals) {


	// pseudo-code:
	// "[x_0,y_0] [x_1,y_1] ... [x_n,y_n]"   (husky-stable_ws: unified_waypoint_control.py [x,y] ... [marker heights])


	std::ostringstream text;
	text << std::fixed << std::setprecision(decimals);
	for (std::size_t k = 0; k < waypoints.size(); k++) {
		text << (k == 0 ? "" : " ") << "[" << waypoints[k].x << "," << waypoints[k].y << "]";
	}


	return text.str();
}
