/****************************************************************************
 *
 *   Copyright (c) 2021 Jaeyoung Lim, Autonomous Systems Lab,
 *  ETH Zürich. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @brief ROS Node to test ompl
 *
 *
 * @author Jaeyoung Lim <jalim@ethz.ch>
 */

#include <geometry_msgs/Point.h>
#include <ros/ros.h>
#include <terrain_navigation/terrain_map.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_eigen/tf2_eigen.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <visualization_msgs/MarkerArray.h>
#include <grid_map_ros/GridMapRosConverter.hpp>

#include "terrain_navigation_ros/visualization.h"
#include "terrain_planner/common.h"
#include "terrain_planner/terrain_ompl_rrt.h"
#include "terrain_planner_benchmark/visualization.h"

int main(int argc, char** argv) {
  ros::init(argc, argv, "ompl_rrt_planner");
  ros::NodeHandle nh("");
  ros::NodeHandle nh_private("~");

  // Initialize ROS related publishers for visualization
  auto start_pos_pub = nh.advertise<visualization_msgs::Marker>("start_position", 1, true);
  auto goal_pos_pub = nh.advertise<visualization_msgs::Marker>("goal_position", 1, true);
  auto path_pub = nh.advertise<nav_msgs::Path>("path", 1, true);
  auto interpolate_path_pub = nh.advertise<nav_msgs::Path>("interpolated_path", 1, true);
  auto path_segment_pub = nh.advertise<visualization_msgs::MarkerArray>("path_segments", 1, true);
  auto grid_map_pub = nh.advertise<grid_map_msgs::GridMap>("grid_map", 1, true);
  auto trajectory_pub = nh.advertise<visualization_msgs::MarkerArray>("tree", 1, true);
  std::string map_path, color_file_path;
  bool random{false};
  double turn_radius{80.0};
  nh_private.param<std::string>("map_path", map_path, "");
  nh_private.param<std::string>("color_file_path", color_file_path, "");
  nh_private.param<bool>("random", random, false);
  nh_private.param<double>("turn_radius", turn_radius, turn_radius);

  // Load terrain map from defined tif paths
  auto terrain_map = std::make_shared<TerrainMap>();
  terrain_map->initializeFromGeotiff(map_path, false);
  if (!color_file_path.empty()) {  // Load color layer if the color path is nonempty
    terrain_map->addColorFromGeotiff(color_file_path);
  }
  terrain_map->AddLayerDistanceTransform(50.0, "distance_surface");
  terrain_map->AddLayerDistanceTransform(120.0, "max_elevation");
  terrain_map->AddLayerHorizontalDistanceTransform(turn_radius, "ics_+", "distance_surface");
  terrain_map->AddLayerHorizontalDistanceTransform(-turn_radius, "ics_-", "max_elevation");
  terrain_map->addLayerSafety("safety", "ics_+", "ics_-");

  Path path;
  std::vector<Eigen::Vector3d> interpolated_path;
  double terrain_altitude{100.0};

  int num_experiments{20};
  for (int i = 0; i < num_experiments; i++) {
    // Initialize planner with loaded terrain map
    auto planner = std::make_shared<TerrainOmplRrt>();
    planner->setMap(terrain_map);
    planner->setAltitudeLimits(120.0, 50.0);
    /// TODO: Get bounds from gridmap
    planner->setBoundsFromMap(terrain_map->getGridMap());

    const Eigen::Vector2d map_pos = terrain_map->getGridMap().getPosition();
    const double map_width_x = terrain_map->getGridMap().getLength().x();
    const double map_width_y = terrain_map->getGridMap().getLength().y();

    Eigen::Vector3d start{Eigen::Vector3d(map_pos(0) - 0.4 * map_width_x, map_pos(1) - 0.4 * map_width_y, 0.0)};
    start(2) =
        terrain_map->getGridMap().atPosition("elevation", Eigen::Vector2d(start(0), start(1))) + terrain_altitude;
    double start_yaw = getRandom(-M_PI, M_PI);
    Eigen::Vector3d start_vel = 10.0 * Eigen::Vector3d(std::cos(start_yaw), std::sin(start_yaw), 0.0);
    Eigen::Vector3d goal{Eigen::Vector3d(map_pos(0) + 0.4 * map_width_x, map_pos(1) + 0.4 * map_width_y, 0.0)};
    goal(2) = terrain_map->getGridMap().atPosition("elevation", Eigen::Vector2d(goal(0), goal(1))) + terrain_altitude;
    double goal_yaw = getRandom(-M_PI, M_PI);
    Eigen::Vector3d goal_vel = 10.0 * Eigen::Vector3d(std::cos(goal_yaw), std::sin(goal_yaw), 0.0);

    planner->setupProblem(start, start_vel, goal, goal_vel);
    bool found_solution{false};
    while (!found_solution) {
      found_solution = planner->Solve(1.0, path);
    }
    planner->getSolutionPath(interpolated_path);

    // Repeatedly publish results
    terrain_map->getGridMap().setTimestamp(ros::Time::now().toNSec());
    grid_map_msgs::GridMap message;
    grid_map::GridMapRosConverter::toMessage(terrain_map->getGridMap(), message);
    grid_map_pub.publish(message);
    publishTrajectory(path_pub, path.position());
    publishTrajectory(interpolate_path_pub, interpolated_path);
    publishPathSegments(path_segment_pub, path);
    publishPositionSetpoints(start_pos_pub, start, start_vel);
    publishPositionSetpoints(goal_pos_pub, goal, goal_vel);
    publishTree(trajectory_pub, planner->getPlannerData(), planner->getProblemSetup());
    if (!random) {
      break;
    }
    ros::Duration(1.0).sleep();
  }
  ros::spin();
  return 0;
}
