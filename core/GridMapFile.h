#pragma once
#include <string>
#include "GridMap.h"

// Saving and loading a GridMap with all its layers, plus a start and an end cell (-1 = none), as a text file:
//
//   gridmap 1
//   size <columns> <rows>
//   resolution <metres per cell>
//   start <column> <row>          (-1 -1 = none)
//   end <column> <row>
//   layer <name>
//   <rows lines of columns numbers; "nan" = unknown>
//   layer <name>
//   ...
//
// Plain text, so maps can be read, diffed and edited by hand.

bool saveGridMap(const std::string& filename, const GridMap& map, int startIndex, int endIndex);
bool loadGridMap(const std::string& filename, GridMap& map, int& startIndex, int& endIndex);
