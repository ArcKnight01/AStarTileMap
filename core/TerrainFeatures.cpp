#include "TerrainFeatures.h"
#include <algorithm>
#include <cmath>

/**
 * Collects the known elevation points (x, y, z in metres) within radius of a cell.
 *
 * @param map     Map with an "elevation" layer.
 * @param column  Column of the centre cell.
 * @param row     Row of the centre cell (0 = top row).
 * @param radius  Window radius in metres (at least one cell).
 * @param xs      Filled with the points' x (metres).
 * @param ys      Filled with the points' y (metres).
 * @param zs      Filled with the points' heights (metres).
 */
static void getWindowPoints(const GridMap& map, int column, int row, double radius,
	std::vector<double>& xs, std::vector<double>& ys, std::vector<double>& zs) {
	xs.clear(); ys.clear(); zs.clear();
	int reach = std::max(1, (int)std::round(radius / map.getResolution()));
	const std::vector<float>& elevation = map.getLayer("elevation");
	for (int dy = -reach; dy <= reach; dy++) {
		for (int dx = -reach; dx <= reach; dx++) {
			if (dx * dx + dy * dy > reach * reach) continue; //circular window
			int c = column + dx, r = row + dy;
			if (!map.isInside(c, r)) continue;
			float z = elevation[map.getIndex(c, r)];
			if (GridMap::isUnknown(z)) continue;
			double x, y;
			map.getPosition(c, r, x, y);
			xs.push_back(x); ys.push_back(y); zs.push_back(z);
		}
	}
}

bool fitPlane(const std::vector<double>& xs, const std::vector<double>& ys, const std::vector<double>& zs, double& a, double& b, double& c) {
	//normal equations of least squares for z = a*x + b*y + c, solved around the mean point (keeps the numbers small)
	std::size_t n = xs.size();
	if (n < 3) return false;
	double mx = 0, my = 0, mz = 0;
	for (std::size_t i = 0; i < n; i++) { mx += xs[i]; my += ys[i]; mz += zs[i]; }
	mx /= n; my /= n; mz /= n;
	double sxx = 0, sxy = 0, syy = 0, sxz = 0, syz = 0;
	for (std::size_t i = 0; i < n; i++) {
		double dx = xs[i] - mx, dy = ys[i] - my, dz = zs[i] - mz;
		sxx += dx * dx; sxy += dx * dy; syy += dy * dy; sxz += dx * dz; syz += dy * dz;
	}
	double determinant = sxx * syy - sxy * sxy;
	if (std::fabs(determinant) < 1e-12) return false; //points on one line
	a = (sxz * syy - syz * sxy) / determinant;
	b = (syz * sxx - sxz * sxy) / determinant;
	c = mz - a * mx - b * my;
	return true;
}

void computeSlope(GridMap& map, const CostMapParameters& parameters) {
	map.addLayer("slope", GridMap::getUnknownValue());
	std::vector<float>& slope = map.getLayer("slope");
	const std::vector<float>& elevation = map.getLayer("elevation");
	std::vector<double> xs, ys, zs;
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			int index = map.getIndex(column, row);
			if (GridMap::isUnknown(elevation[index])) continue;
			getWindowPoints(map, column, row, parameters.slopeRadius, xs, ys, zs);
			double a, b, c;
			if (!fitPlane(xs, ys, zs, a, b, c)) continue;
			//the plane's normal is (-a, -b, 1); its angle from vertical is atan(sqrt(a^2 + b^2))
			slope[index] = (float)std::atan(std::sqrt(a * a + b * b));
		}
	}
}

void computeRoughness(GridMap& map, const CostMapParameters& parameters) {
	map.addLayer("roughness", GridMap::getUnknownValue());
	std::vector<float>& roughness = map.getLayer("roughness");
	const std::vector<float>& elevation = map.getLayer("elevation");
	std::vector<double> xs, ys, zs;
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			int index = map.getIndex(column, row);
			if (GridMap::isUnknown(elevation[index])) continue;
			getWindowPoints(map, column, row, parameters.roughnessRadius, xs, ys, zs);
			double a, b, c;
			if (!fitPlane(xs, ys, zs, a, b, c)) continue;
			//standard deviation of the heights from the fitted plane
			double sumSquares = 0;
			for (std::size_t i = 0; i < xs.size(); i++) {
				double residual = zs[i] - (a * xs[i] + b * ys[i] + c);
				sumSquares += residual * residual;
			}
			roughness[index] = (float)std::sqrt(sumSquares / xs.size());
		}
	}
}

void computeStepHeight(GridMap& map, const CostMapParameters& parameters) {
	map.addLayer("step", GridMap::getUnknownValue());
	std::vector<float>& step = map.getLayer("step");
	const std::vector<float>& elevation = map.getLayer("elevation");
	std::vector<double> xs, ys, zs;
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			int index = map.getIndex(column, row);
			if (GridMap::isUnknown(elevation[index])) continue;
			getWindowPoints(map, column, row, parameters.stepRadius, xs, ys, zs);
			auto lowestHighest = std::minmax_element(zs.begin(), zs.end());
			step[index] = (float)(*lowestHighest.second - *lowestHighest.first);
		}
	}
}

void computeTerrainFeatures(GridMap& map, const CostMapParameters& parameters) {
	computeSlope(map, parameters);
	computeRoughness(map, parameters);
	computeStepHeight(map, parameters);
}
