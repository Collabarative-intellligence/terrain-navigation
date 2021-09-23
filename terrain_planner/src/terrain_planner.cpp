/****************************************************************************
 *
 *   Copyright (c) 2021 Jaeyoung Lim. All rights reserved.
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
 * 3. Neither the name terrain-navigation nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * @brief Terrain Planner
 *
 * Terrain Planner
 *
 * @author Jaeyoung Lim <jalim@ethz.ch>
 */

#include "terrain_planner/terrain_planner.h"

#include <grid_map_msgs/GridMap.h>
#include <mavros_msgs/PositionTarget.h>
#include <visualization_msgs/MarkerArray.h>
#include <grid_map_ros/GridMapRosConverter.hpp>

TerrainPlanner::TerrainPlanner(const ros::NodeHandle &nh, const ros::NodeHandle &nh_private)
    : nh_(nh), nh_private_(nh_private) {
  vehicle_path_pub_ = nh_.advertise<nav_msgs::Path>("vehicle_path", 1);
  cmdloop_timer_ = nh_.createTimer(ros::Duration(0.1), &TerrainPlanner::cmdloopCallback,
                                   this);  // Define timer for constant loop rate
  statusloop_timer_ = nh_.createTimer(ros::Duration(5.0), &TerrainPlanner::statusloopCallback,
                                      this);  // Define timer for constant loop rate

  grid_map_pub_ = nh_.advertise<grid_map_msgs::GridMap>("grid_map", 1, true);

  posehistory_pub_ = nh_.advertise<nav_msgs::Path>("geometric_controller/path", 10);
  candidate_manuever_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("visualization_marker", 1, true);
  position_target_pub_ = nh_.advertise<visualization_msgs::Marker>("position_target", 1, true);
  position_setpoint_pub_ = nh_.advertise<mavros_msgs::PositionTarget>("mavros/setpoint_raw/local", 1);
  vehicle_pose_pub_ = nh_.advertise<visualization_msgs::Marker>("vehicle_pose_marker", 1, true);

  mavpose_sub_ = nh_.subscribe("mavros/local_position/pose", 1, &TerrainPlanner::mavposeCallback, this,
                               ros::TransportHints().tcpNoDelay());
  mavtwist_sub_ = nh_.subscribe("mavros/local_position/velocity_local", 1, &TerrainPlanner::mavtwistCallback, this,
                                ros::TransportHints().tcpNoDelay());
  std::string map_path;
  nh_private.param<std::string>("terrain_path", map_path, "resources/cadastre.tif");
  maneuver_library_ = std::make_shared<ManeuverLibrary>();
  maneuver_library_->setPlanningHorizon(5.0);
  maneuver_library_->setTerrainMap(map_path);
  planner_profiler_ = std::make_shared<Profiler>("planner");
}
TerrainPlanner::~TerrainPlanner() {
  // Destructor
}

void TerrainPlanner::cmdloopCallback(const ros::TimerEvent &event) {
  // TODO: Get position setpoint based on time
  double time_since_start = (ros::Time::now() - plan_time_).toSec();
  std::vector<Eigen::Vector3d> trajectory = reference_primitive_.position();
  int i = 1;
  for (auto position : trajectory) {
    if (time_since_start < 0.1 * i) {
      publishPositionSetpoints(position);
      break;
    } else {
      i++;
    }
  }
  publishVehiclePose(vehicle_position_, vehicle_attitude_);
  publishPoseHistory();
}

void TerrainPlanner::statusloopCallback(const ros::TimerEvent &event) {
  // planner_profiler_->tic();
  ///TODO: Plan from next segment
  Eigen::Vector3d start_position = vehicle_position_ + vehicle_velocity_ * 0.4;
  maneuver_library_->generateMotionPrimitives(start_position, vehicle_velocity_);
  /// TODO: Switch to chrono
  plan_time_ = ros::Time::now();
  bool result = maneuver_library_->Solve();

  reference_primitive_ = maneuver_library_->getRandomPrimitive();
  // planner_profiler_->toc();
  publishCandidateManeuvers(maneuver_library_->getMotionPrimitives());
  publishTrajectory(reference_primitive_.position());
  MapPublishOnce();
}

void TerrainPlanner::publishTrajectory(std::vector<Eigen::Vector3d> trajectory) {
  nav_msgs::Path msg;
  std::vector<geometry_msgs::PoseStamped> posestampedhistory_vector;
  Eigen::Vector4d orientation(1.0, 0.0, 0.0, 0.0);
  for (auto pos : trajectory) {
    posestampedhistory_vector.insert(posestampedhistory_vector.begin(), vector3d2PoseStampedMsg(pos, orientation));
  }
  msg.header.stamp = ros::Time::now();
  msg.header.frame_id = "map";
  msg.poses = posestampedhistory_vector;
  vehicle_path_pub_.publish(msg);
}

void TerrainPlanner::mavposeCallback(const geometry_msgs::PoseStamped &msg) {
  vehicle_position_ = toEigen(msg.pose.position);
  vehicle_attitude_(0) = msg.pose.orientation.w;
  vehicle_attitude_(1) = msg.pose.orientation.x;
  vehicle_attitude_(2) = msg.pose.orientation.y;
  vehicle_attitude_(3) = msg.pose.orientation.z;
}

void TerrainPlanner::mavtwistCallback(const geometry_msgs::TwistStamped &msg) {
  vehicle_velocity_ = toEigen(msg.twist.linear);
  // mavRate_ = toEigen(msg.twist.angular);
}

void TerrainPlanner::MapPublishOnce() {
  maneuver_library_->getGridMap().setTimestamp(ros::Time::now().toNSec());
  grid_map_msgs::GridMap message;
  grid_map::GridMapRosConverter::toMessage(maneuver_library_->getGridMap(), message);
  grid_map_pub_.publish(message);
}

void TerrainPlanner::publishPoseHistory() {
  int posehistory_window_ = 20000;
  Eigen::Vector4d vehicle_attitude(1.0, 0.0, 0.0, 0.0);
  posehistory_vector_.insert(posehistory_vector_.begin(), vector3d2PoseStampedMsg(vehicle_position_, vehicle_attitude));
  if (posehistory_vector_.size() > posehistory_window_) {
    posehistory_vector_.pop_back();
  }

  nav_msgs::Path msg;
  msg.header.stamp = ros::Time::now();
  msg.header.frame_id = "map";
  msg.poses = posehistory_vector_;

  posehistory_pub_.publish(msg);
}

void TerrainPlanner::publishCandidateManeuvers(const std::vector<Trajectory> &candidate_maneuvers) {
  visualization_msgs::MarkerArray msg;

  std::vector<visualization_msgs::Marker> marker;
  visualization_msgs::Marker mark;
  mark.action = visualization_msgs::Marker::DELETEALL;
  marker.push_back(mark);
  msg.markers = marker;
  candidate_manuever_pub_.publish(msg);

  std::vector<visualization_msgs::Marker> maneuver_library_vector;
  int i = 0;
  for (auto maneuver : candidate_maneuvers) {
    maneuver_library_vector.insert(maneuver_library_vector.begin(), trajectory2MarkerMsg(maneuver, i));
    i++;
  }
  msg.markers = maneuver_library_vector;
  candidate_manuever_pub_.publish(msg);
}

void TerrainPlanner::publishPositionSetpoints(const Eigen::Vector3d &position) {
  using namespace mavros_msgs;
  // Publishes position setpoints sequentially as trajectory setpoints
  mavros_msgs::PositionTarget msg;
  msg.header.stamp = ros::Time::now();
  msg.coordinate_frame = PositionTarget::FRAME_LOCAL_NED;
  msg.type_mask = PositionTarget::IGNORE_AFX | PositionTarget::IGNORE_AFY | PositionTarget::IGNORE_AFZ;
  msg.position.x = position(0);
  msg.position.y = position(1);
  msg.position.z = position(2);
  position_setpoint_pub_.publish(msg);

  visualization_msgs::Marker marker;
  marker.header.stamp = ros::Time::now();
  marker.type = visualization_msgs::Marker::SPHERE;
  marker.header.frame_id = "map";
  marker.id = 0;
  marker.action = visualization_msgs::Marker::DELETEALL;
  position_target_pub_.publish(marker);

  marker.header.stamp = ros::Time::now();
  marker.action = visualization_msgs::Marker::ADD;
  marker.scale.x = 10.0;
  marker.scale.y = 10.0;
  marker.scale.z = 10.0;
  marker.color.a = 0.5;  // Don't forget to set the alpha!
  marker.color.r = 0.0;
  marker.color.g = 1.0;
  marker.color.b = 0.0;
  marker.pose.position.x = position(0);
  marker.pose.position.y = position(1);
  marker.pose.position.z = position(2);
  marker.pose.orientation.w = 1.0;
  marker.pose.orientation.x = 0.0;
  marker.pose.orientation.y = 0.0;
  marker.pose.orientation.z = 0.0;

  position_target_pub_.publish(marker);
}

void TerrainPlanner::publishVehiclePose(const Eigen::Vector3d &position, const Eigen::Vector4d &attitude) {
  Eigen::Vector4d mesh_attitude = quatMultiplication(attitude, Eigen::Vector4d(std::cos(M_PI/2), 0.0, 0.0, std::sin(M_PI/2)));
  geometry_msgs::Pose vehicle_pose = vector3d2PoseMsg(position, mesh_attitude);
  visualization_msgs::Marker marker;
  marker.header.stamp = ros::Time::now();
  marker.header.frame_id = "map";
  marker.type = visualization_msgs::Marker::MESH_RESOURCE;
  marker.ns = "my_namespace";
  marker.mesh_resource = "file:///home/jaeyoung/src/PX4-Autopilot/Tools/sitl_gazebo/models/believer/meshes/believer_body.dae";
  marker.scale.x = 10.0;
  marker.scale.y = 10.0;
  marker.scale.z = 10.0;
  marker.color.a = 0.5;  // Don't forget to set the alpha!
  marker.color.r = 0.5;
  marker.color.g = 0.5;
  marker.color.b = 0.5;
  marker.pose = vehicle_pose;
  vehicle_pose_pub_.publish(marker);
}