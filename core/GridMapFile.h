#pragma once
#include <ostream>
#include <string>
#include <vector>
#include "GridMap.h"

// Saving and loading a GridMap as a grid_map_msgs/msg/GridMap of ROS 2 Jazzy (the header sits in the message, not in
// GridMapInfo as in ROS 1), written as YAML in the shape `ros2 topic echo` prints it, so a map file is the message itself:
//   ros2 topic pub --once /elevation_map grid_map_msgs/msg/GridMap "$(cat maps/rugged.gridmap)"
// https://github.com/ANYbotics/grid_map/tree/jazzy/grid_map_msgs/msg (GridMap.msg, GridMapInfo.msg)
//
//   header:            stamp 0, frame_id "map" (REP 105)
//   info:              resolution, length_x = columns * resolution, length_y = rows * resolution,
//                      pose = map centre at (0, 0, 0), no rotation (our GridMap is always centred on the origin)
//   layers:            every layer of the map
//   basic_layers:      []
//   data:              one std_msgs/Float32MultiArray per layer, in grid_map's storage order (below)
//   outer_start_index, inner_start_index: 0 (no circular-buffer shift)
//
// grid_map's storage order. grid_map keeps a layer in an Eigen matrix (column-major) whose index (i, j) runs from the
// corner with the largest x and y: i grows toward -x, j toward -y. Our GridMap has the column growing toward +x and
// row 0 at +y, so
//   i = columns - 1 - column,     j = row,     data[ j * columns + i ]
// and the layout is  dim[0] = { "column_index", size = rows, stride = rows * columns },
//                    dim[1] = { "row_index",    size = columns, stride = columns }   (as grid_map_ros writes it).
// Unknown values are written as .nan (YAML's NaN).
//
// The start and the end of the path are not part of the message, so they are saved next to it in
// maps/<name>.endpoints.yaml, as world positions in metres in the same frame (like a start pose and a goal in ROS).
//
// (the first format was a plain text "gridmap 1" file with "size", "resolution", "start", "end" lines and one
// row-major "layer" block per layer; maps were converted once to this format)

// The index is the position of the cell's value in a layer's data array.
/**
 * Get the index of a cell in grid_map's storage order.
 *
 * @param map     The map (its column count).
 * @param column  Column of the cell (0 = smallest x).
 * @param row     Row of the cell (0 = largest y).
 * @return        Index into the layer's Float32MultiArray data, j * columns + i with i = columns - 1 - column, j = row.
 */
int getMessageIndex(const GridMap& map, int column, int row);

/**
 * Get the name of the file that holds a map's start and end: "maps/x.gridmap" -> "maps/x.endpoints.yaml".
 *
 * @param filename  Name of the map file.
 * @return          Name of its endpoints file.
 */
std::string getEndpointsFilename(const std::string& filename);

/**
 * Save a map as a grid_map_msgs/msg/GridMap in YAML.
 *
 * @param filename  File to write.
 * @param map       The map, every layer.
 * @param frameId   header.frame_id, the frame the map is in ("map").
 * @return          False if the file could not be opened.
 */
bool saveGridMapMessage(const std::string& filename, const GridMap& map, const std::string& frameId = "map");

/**
 * Write some layers of a map as a grid_map_msgs/msg/GridMap in YAML to a stream.
 *
 * @param file     Stream to write to (a file, or a string for the clipboard).
 * @param map      The map.
 * @param names    Layers to write, in order (each must exist in the map).
 * @param frameId  header.frame_id, the frame the map is in ("map").
 */
void writeGridMapMessage(std::ostream& file, const GridMap& map, const std::vector<std::string>& names, const std::string& frameId = "map");

/**
 * Format a number as YAML float text (float or double).
 *
 * @param value  The number.
 * @return       Shortest text that reads back as the same value, always with a "."; NaN as ".nan", infinity as ".inf".
 */
template <typename Number>
std::string getYamlNumberText(Number value);

// The file may be as saveGridMapMessage writes it, or as `ros2 topic echo` prints it. Mappings must be in block
// style; sequences may be block ("- a") or flow ("[a, b]"). Both storage orders are read (dim[0].label
// "column_index" or "row_index"), and the start indices undo a circular-buffer shift. The pose is not read: our
// GridMap is always centred on (0, 0).
/**
 * Load a map from a grid_map_msgs/msg/GridMap in YAML.
 *
 * @param filename  File to read.
 * @param map       Filled with the map.
 * @return          False if the file is missing or not in this format.
 */
bool loadGridMapMessage(const std::string& filename, GridMap& map);

// A missing cell (-1) is left out.
/**
 * Save the start and end cells as world positions (metres) in a small YAML file.
 *
 * @param filename    File to write (maps/<name>.endpoints.yaml).
 * @param map         The map the cells belong to (for their positions).
 * @param startIndex  Start cell (row * columns + column), or -1.
 * @param endIndex    End cell, or -1.
 * @return            False if the file could not be opened.
 */
bool saveEndpoints(const std::string& filename, const GridMap& map, int startIndex, int endIndex);

// A missing file, a missing entry, or a position outside the map gives -1 (none).
/**
 * Load the start and end cells from an endpoints file.
 *
 * @param filename    File to read.
 * @param map         The map the positions are looked up in.
 * @param startIndex  Filled with the start cell, or -1.
 * @param endIndex    Filled with the end cell, or -1.
 */
void loadEndpoints(const std::string& filename, const GridMap& map, int& startIndex, int& endIndex);

/**
 * Save a map and its start / end: the grid_map message to filename, the endpoints next to it.
 *
 * @param filename    Map file to write; the endpoints go to getEndpointsFilename(filename).
 * @param map         The map, every layer.
 * @param startIndex  Start cell (row * columns + column), or -1.
 * @param endIndex    End cell, or -1.
 * @return            False if the map file could not be written.
 */
bool saveGridMap(const std::string& filename, const GridMap& map, int startIndex, int endIndex);

/**
 * Load a map and its start / end (each -1 if there is none).
 *
 * @param filename    Map file to read; the endpoints come from getEndpointsFilename(filename).
 * @param map         Filled with the map.
 * @param startIndex  Filled with the start cell, or -1.
 * @param endIndex    Filled with the end cell, or -1.
 * @return            False if the map file is missing or not in this format.
 */
bool loadGridMap(const std::string& filename, GridMap& map, int& startIndex, int& endIndex);
