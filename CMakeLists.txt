cmake_minimum_required(VERSION 3.16)
project(AStarTileMap CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)   # breakpoints work out of the box
endif()

# SFML 2.5+ (Ubuntu: sudo apt install libsfml-dev)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(AStarTileMap
  src/main.cpp
  src/Tile.cpp
  src/character.cpp
  src/Pathfinding.cpp
  src/TilePlacement.cpp
  src/Button.cpp
  src/TextBox.cpp
  src/Dropdown.cpp
  src/NavGraph.h
)
target_link_libraries(AStarTileMap PRIVATE sfml-graphics sfml-window sfml-system)

# A* test: compares pathfinding() with a brute-force search.  Run: ctest --test-dir build
enable_testing()
add_executable(test_astar tests/test_astar.cpp src/Tile.cpp src/Pathfinding.cpp src/TilePlacement.cpp)
target_include_directories(test_astar PRIVATE src)
target_link_libraries(test_astar PRIVATE sfml-graphics sfml-window sfml-system)
add_test(NAME astar COMMAND test_astar WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# The program loads "Textures/*.png", "Font/Candal.ttf" and "maps/<name>.txt" relative to the
# directory it is run from (the build folder). Link those folders back to the
# repo so maps you save with E land in the repo's maps/ folder, not a copy.
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/assets/Textures ${CMAKE_BINARY_DIR}/Textures SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/assets/Font     ${CMAKE_BINARY_DIR}/Font     SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/maps            ${CMAKE_BINARY_DIR}/maps     SYMBOLIC COPY_ON_ERROR)
