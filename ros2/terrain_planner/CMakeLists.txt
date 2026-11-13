cmake_minimum_required(VERSION 3.16)
project(terrain_planner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- the core: the repo's core/ folder, used as-is (plain C++, no ROS). By default it is found two folders up from this
# --- package (AStarTileMap/ros2/terrain_planner -> AStarTileMap/core). REALPATH, so it also works when the package is
# --- symlinked into a workspace:  ln -s ~/Repos/CppRepos/AStarTileMap/ros2/terrain_planner ~/ros2_ws/src/
get_filename_component(PACKAGE_DIR "${CMAKE_CURRENT_LIST_DIR}" REALPATH)
set(TERRAIN_PLANNER_REPO_DIR "${PACKAGE_DIR}/../.." CACHE PATH "The AStarTileMap repo (holds core/, maps/, tests/)")
get_filename_component(REPO_DIR "${TERRAIN_PLANNER_REPO_DIR}" REALPATH)
set(CORE_DIR "${REPO_DIR}/core")
if(NOT EXISTS "${CORE_DIR}/GridMap.h")
  message(FATAL_ERROR "terrain_planner: no core/ at ${CORE_DIR}. Set -DTERRAIN_PLANNER_REPO_DIR=<AStarTileMap repo>.")
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(grid_map_ros REQUIRED)
find_package(grid_map_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

add_library(costmap_core STATIC
  ${CORE_DIR}/GridMap.cpp
  ${CORE_DIR}/GridMapFile.cpp
  ${CORE_DIR}/TerrainGenerator.cpp
  ${CORE_DIR}/TerrainFeatures.cpp
  ${CORE_DIR}/Traversability.cpp
  ${CORE_DIR}/CostMap.cpp
  ${CORE_DIR}/PathPlanner.cpp
  ${CORE_DIR}/Waypoints.cpp
  ${CORE_DIR}/RosExport.cpp
)
target_include_directories(costmap_core PUBLIC ${CORE_DIR})
set_target_properties(costmap_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- path -> waypoints: now core/Waypoints.cpp (in costmap_core above), shared with the app
# (was: add_library(terrain_planner_waypoints STATIC src/Waypoints.cpp), this package's own copy)

# --- the ROS layer, as components (one process or several)
add_library(terrain_planner_nodes SHARED
  src/GridMapConversion.cpp
  src/ParameterLoading.cpp
  src/CostMapNode.cpp
  src/PlannerNode.cpp
  src/GridMapFilePublisher.cpp
)
target_include_directories(terrain_planner_nodes PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(terrain_planner_nodes costmap_core)
ament_target_dependencies(terrain_planner_nodes
  rclcpp rclcpp_components grid_map_ros grid_map_msgs nav_msgs geometry_msgs visualization_msgs
  tf2 tf2_ros tf2_geometry_msgs)

rclcpp_components_register_node(terrain_planner_nodes
  PLUGIN "terrain_planner::CostMapNode" EXECUTABLE cost_map_node)
rclcpp_components_register_node(terrain_planner_nodes
  PLUGIN "terrain_planner::PlannerNode" EXECUTABLE planner_node)
rclcpp_components_register_node(terrain_planner_nodes
  PLUGIN "terrain_planner::GridMapFilePublisher" EXECUTABLE gridmap_file_publisher)

install(TARGETS terrain_planner_nodes costmap_core
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch rviz DESTINATION share/${PROJECT_NAME})
# the example maps (flat, rugged, dungeon, hybrid) for gridmap_file_publisher: each map is a grid_map_msgs/GridMap in
# <name>.gridmap, and its start / end are in <name>.endpoints.yaml next to it (core/GridMapFile.h)
install(DIRECTORY ${REPO_DIR}/maps/ DESTINATION share/${PROJECT_NAME}/maps FILES_MATCHING PATTERN "*.gridmap" PATTERN "*.endpoints.yaml")

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_conversion test/test_conversion.cpp)
  target_link_libraries(test_conversion terrain_planner_nodes)

  ament_add_gtest(test_waypoints test/test_waypoints.cpp)
  target_link_libraries(test_waypoints costmap_core) #core/Waypoints (was terrain_planner_waypoints)

  # the repo's own core tests (a plain executable, non-zero on failure); run from the repo so maps/ is found
  add_executable(test_costmap ${REPO_DIR}/tests/test_costmap.cpp)
  target_link_libraries(test_costmap costmap_core)
  add_test(NAME test_costmap COMMAND test_costmap WORKING_DIRECTORY ${REPO_DIR})
endif()

ament_package()
