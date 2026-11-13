"""On the robot: cost_map_node + planner_node in one process (no copy of the map between them).

    ros2 launch terrain_planner planner.launch.py
    ros2 launch terrain_planner planner.launch.py elevation_map:=/elevation_mapping/elevation_map_raw
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    share = get_package_share_directory('terrain_planner')
    parameters = [os.path.join(share, 'config', 'cost_map.yaml'), os.path.join(share, 'config', 'planner.yaml')]
    elevation_map = LaunchConfiguration('elevation_map')
    use_sim_time = {'use_sim_time': LaunchConfiguration('use_sim_time')}

    container = ComposableNodeContainer(
        name='terrain_planner_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        output='screen',
        composable_node_descriptions=[
            ComposableNode(package='terrain_planner', plugin='terrain_planner::CostMapNode', name='cost_map_node',
                           parameters=parameters + [use_sim_time],
                           remappings=[('elevation_map', elevation_map)],
                           extra_arguments=[{'use_intra_process_comms': True}]),
            ComposableNode(package='terrain_planner', plugin='terrain_planner::PlannerNode', name='planner_node',
                           parameters=parameters + [use_sim_time],
                           extra_arguments=[{'use_intra_process_comms': True}]),
        ])

    return LaunchDescription([
        DeclareLaunchArgument('elevation_map', default_value='elevation_map',
                              description='topic of the elevation mapper (grid_map_msgs/GridMap)'),
        DeclareLaunchArgument('use_sim_time', default_value='false', description='true when replaying a rosbag'),
        container,
    ])
