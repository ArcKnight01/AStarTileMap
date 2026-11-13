#pragma once
#include "GridMap.h"

// Building blocks for making "elevation" layers (synthetic test terrain). Each adds to the existing elevation, so
// terrain is built up from simple pieces. Heights and sizes are in metres.

/**
 * A map whose "elevation" is 0 everywhere.
 *
 * @param columns     Number of columns.
 * @param rows        Number of rows.
 * @param resolution  Cell side length in metres.
 * @return            The map, with an "elevation" layer of zeros.
 */
GridMap createFlatMap(int columns, int rows, double resolution);
//rolling sand dunes: amplitude * sin(2 pi x / wavelengthX + phase) * cos(2 pi y / wavelengthY)
/**
 * Add rolling sand dunes to the "elevation" layer.
 *
 * @param map          Map with an "elevation" layer.
 * @param amplitude    Peak height in metres.
 * @param wavelengthX  Wavelength along x in metres.
 * @param wavelengthY  Wavelength along y in metres.
 * @param phase        Phase shift along x in radians.
 */
void addDunes(GridMap& map, double amplitude, double wavelengthX, double wavelengthY, double phase);
//a smooth round hill (Gaussian) centred on a cell; radius is where it has dropped to about 60% of its height
/**
 * Add a smooth round hill (Gaussian) to the "elevation" layer.
 *
 * @param map           Map with an "elevation" layer.
 * @param centreColumn  Column of the hill's centre.
 * @param centreRow     Row of the hill's centre.
 * @param height        Height at the centre in metres.
 * @param radius        Standard deviation of the Gaussian in metres.
 */
void addHill(GridMap& map, int centreColumn, int centreRow, double height, double radius);
/**
 * Small random bumps of up to +/- amplitude, the same every time for the same seed.
 *
 * @param map        Map with an "elevation" layer.
 * @param amplitude  Largest bump height in metres.
 * @param seed       Seed of the random number generator.
 */
void addNoise(GridMap& map, double amplitude, unsigned int seed);
/**
 * The same bumps, only inside a rectangle of cells (columns column0..column1, rows row0..row1, inclusive).
 *
 * @param map        Map with an "elevation" layer.
 * @param column0    First column.
 * @param row0       First row.
 * @param column1    Last column (inclusive).
 * @param row1       Last row (inclusive).
 * @param amplitude  Largest bump height in metres.
 * @param seed       Seed of the random number generator.
 */
void addNoiseInRectangle(GridMap& map, int column0, int row0, int column1, int row1, double amplitude, unsigned int seed);
/**
 * Raises a rectangle of cells to at least height (a wall, a block or a ridge).
 *
 * @param map      Map with an "elevation" layer.
 * @param column0  First column.
 * @param row0     First row.
 * @param column1  Last column (inclusive).
 * @param row1     Last row (inclusive).
 * @param height   Lowest elevation of the rectangle in metres.
 */
void addBlock(GridMap& map, int column0, int row0, int column1, int row1, double height);
