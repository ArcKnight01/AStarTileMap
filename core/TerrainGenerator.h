#pragma once
#include "GridMap.h"

// Building blocks for making "elevation" layers (synthetic test terrain). Each adds to the existing elevation, so
// terrain is built up from simple pieces. Heights and sizes are in metres.

//a map whose "elevation" is 0 everywhere
GridMap createFlatMap(int columns, int rows, double resolution);
//rolling sand dunes: amplitude * sin(2 pi x / wavelengthX + phase) * cos(2 pi y / wavelengthY)
void addDunes(GridMap& map, double amplitude, double wavelengthX, double wavelengthY, double phase);
//a smooth round hill (Gaussian) centred on a cell; radius is where it has dropped to about 60% of its height
void addHill(GridMap& map, int centreColumn, int centreRow, double height, double radius);
//small random bumps of up to +/- amplitude, the same every time for the same seed
void addNoise(GridMap& map, double amplitude, unsigned int seed);
//the same bumps, only inside a rectangle of cells (columns column0..column1, rows row0..row1, inclusive)
void addNoiseInRectangle(GridMap& map, int column0, int row0, int column1, int row1, double amplitude, unsigned int seed);
//raises a rectangle of cells to at least height (a wall, a block or a ridge)
void addBlock(GridMap& map, int column0, int row0, int column1, int row1, double height);
