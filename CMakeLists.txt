cmake_minimum_required(VERSION 3.16)
project(AStarTileMap CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)   # breakpoints work out of the box
endif()

# --- core: the cost map pipeline and the path planners. Plain C++, no SFML and no ROS, so this folder can later be
# --- dropped into a ROS 2 package as-is.
add_library(costmap_core STATIC
  core/GridMap.cpp
  core/GridMapFile.cpp
  core/TerrainGenerator.cpp
  core/TerrainFeatures.cpp
  core/Traversability.cpp
  core/CostMap.cpp
  core/PathPlanner.cpp
)
target_include_directories(costmap_core PUBLIC core)

# --- the visualizer (SFML 2.5+; Ubuntu: sudo apt install libsfml-dev)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
add_executable(AStarTileMap
  src/main.cpp
  src/Tile.cpp
  src/TilePlacement.cpp
  src/TerrainBrush.cpp
  src/Button.cpp
  src/TextBox.cpp
  src/Dropdown.cpp
  # src/character.cpp   (CHARACTER: a relic of TileMap, not used on the costmap branch)
)
target_link_libraries(AStarTileMap PRIVATE costmap_core sfml-graphics sfml-window sfml-system)

# --- builds the example maps in maps/ (flat, rugged, dungeon, hybrid). Run from the repo folder: ./build/generate_maps
add_executable(generate_maps tools/generate_maps.cpp)
target_link_libraries(generate_maps PRIVATE costmap_core)

# --- tests of the core (no SFML).  Run: ctest --test-dir build --output-on-failure
enable_testing()
add_executable(test_costmap tests/test_costmap.cpp)
target_link_libraries(test_costmap PRIVATE costmap_core)
add_test(NAME costmap COMMAND test_costmap WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# The program loads "Textures/*.png", "Font/Candal.ttf" and "maps/<name>.gridmap" relative to the
# directory it is run from (the build folder). Link those folders back to the
# repo so maps you save land in the repo's maps/ folder, not a copy.
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/assets/Textures ${CMAKE_BINARY_DIR}/Textures SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/assets/Font     ${CMAKE_BINARY_DIR}/Font     SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/maps            ${CMAKE_BINARY_DIR}/maps     SYMBOLIC COPY_ON_ERROR)
