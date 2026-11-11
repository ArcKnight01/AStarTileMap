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
  src/NavGraph.h
)
target_link_libraries(AStarTileMap PRIVATE sfml-graphics sfml-window sfml-system)

# The program loads "Textures/*.png" and "maps/<name>.txt" relative to the
# directory it is run from (the build folder). Link those folders back to the
# repo so maps you save with E land in the repo's maps/ folder, not a copy.
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/assets/Textures ${CMAKE_BINARY_DIR}/Textures SYMBOLIC COPY_ON_ERROR)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/maps            ${CMAKE_BINARY_DIR}/maps     SYMBOLIC COPY_ON_ERROR)
