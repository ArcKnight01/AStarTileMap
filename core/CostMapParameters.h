#pragma once
// Every threshold, weight and radius of the cost-map pipeline in one place (later: a ROS parameter / YAML file).
// The values are starting points to tune for the Go2 on sand.

enum class UnknownPolicy {
	Lethal,    //unknown cells (holes in the elevation map) are never entered
	FixedCost, //unknown cells cost unknownCost
	Free       //unknown cells cost 0
};

struct CostMapParameters {
	// --- traversability features and score ---
	// From Wermelinger, Fankhauser, Diethelm, Krusi, Siegwart & Hutter, "Navigation Planning for Legged Robots in
	// Challenging Terrain" (IROS 2016) and ETH's traversability_estimation package (built on grid_map / elevation_mapping).
	// Its default config (robot_filter_parameter.yaml): critical slope 1.0 rad, step 0.12 m, roughness 0.05 m,
	// windows 0.04-0.05 m (for fine maps on ANYmal), and an equal-weight average of the three scores.
	// The radii here are at least one 0.1 m cell, and the critical slope is a lower starting value for sand.
	double slopeRadius = 0.15;       //m: window for the plane fit that gives the slope
	double roughnessRadius = 0.15;   //m: window for the roughness (spread of heights around that plane)
	double stepRadius = 0.10;        //m: window for the step height (max - min height); kept small (one cell around the
	                                 //centre) so a smooth slope isn't mistaken for a step (ETH uses 0.04 m windows)
	double criticalSlope = 0.40;     //rad (about 23 degrees; ETH default 1.0): steeper scores 0
	double criticalRoughness = 0.05; //m (ETH default)
	double criticalStep = 0.12;      //m (ETH default): higher steps score 0
	double slopeWeight = 1.0 / 3.0;  //the three weights sum to 1 (ETH: an equal-weight average)
	double roughnessWeight = 1.0 / 3.0;
	double stepWeight = 1.0 / 3.0;
	//false (ETH): a feature above its critical value only zeroes its own score.
	//true (stricter): any feature above its critical value makes the whole cell 0.
	bool zeroIfAnyCritical = false;

	// --- cost map (Rajput, Algorithm 1, ground locomotion only) ---
	double traversabilityThreshold = 0.5; //below this a cell is lethal (on the M4 it meant "fly"; the Go2 can't)
	float lethalCost = 60.f;              //Rajput's Le; any cost >= lethalCost is never entered
	double inflationRadius = 0.2;         //m: Go2 half width (~0.16 m) plus a margin
	UnknownPolicy unknownPolicy = UnknownPolicy::Lethal;
	float unknownCost = 0.5f;             //used by UnknownPolicy::FixedCost

	// --- planner (Rajput eq. 3.8: g(n) = g(c) + h(c, n) + costWeight * c_n) ---
	double costWeight = 1.0;
};
