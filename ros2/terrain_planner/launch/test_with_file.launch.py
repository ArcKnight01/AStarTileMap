"""No robot: a maps/*.gridmap file stands in for the elevation mapper (and a static TF for the robot's pose).

    ros2 launch terrain_planner test_with_file.launch.py map:=hybrid
    ros2 launch terrain_planner test_with_file.launch.py map:=dungeon rviz:=false

The file's start cell is the robot, its end cell the goal; set another goal in RViz with "2D Goal Pose".
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def make_nodes(context):
    share = get_package_share_directory('terrain_planner')
    parameters = [os.path.join(share, 'config', 'cost_map.yaml'), os.path.join(share, 'config', 'planner.yaml')]
    map_name = LaunchConfiguration('map').perform(context)
    map_file = map_name if map_name.endswith('.gridmap') else os.path.join(share, 'maps', map_name + '.gridmap')
    return [
        Node(package='terrain_planner', executable='gridmap_file_publisher', output='screen',
             parameters=[{'file': map_file, 'publish_as': LaunchConfiguration('publish_as')}]),
        Node(package='terrain_planner', executable='cost_map_node', output='screen', parameters=parameters),
        Node(package='terrain_planner', executable='planner_node', output='screen', parameters=parameters),
        Node(package='rviz2', executable='rviz2', output='screen',
             arguments=['-d', os.path.join(share, 'rviz', 'terrain_planner.rviz')],
             condition=IfCondition(LaunchConfiguration('rviz'))),
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('map', default_value='hybrid',
                              description='flat | rugged | dungeon | hybrid, or a path to a .gridmap file'),
        DeclareLaunchArgument('publish_as', default_value='auto', description='auto | elevation | cost'),
        DeclareLaunchArgument('rviz', default_value='true'),
        OpaqueFunction(function=make_nodes),
    ])
