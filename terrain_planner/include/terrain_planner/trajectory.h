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

#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include <Eigen/Dense>
#include <vector>

struct State {
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  Eigen::Vector3d position;
  Eigen::Vector3d velocity;
  Eigen::Vector4d attitude;
};

class Trajectory {
 public:
  Trajectory(){};
  virtual ~Trajectory(){};
  std::vector<Eigen::Vector3d> position() {
    std::vector<Eigen::Vector3d> pos_vector;
    for (auto state : states) {
      pos_vector.push_back(state.position);
    }
    return pos_vector;
  }
  std::vector<Eigen::Vector3d> velocity() {
    std::vector<Eigen::Vector3d> vel_vector;
    for (auto state : states) {
      vel_vector.push_back(state.velocity);
    }
    return vel_vector;
  }
  bool valid() { return validity; }
  std::vector<State> states;
  double utility{0.0};
  bool validity{false};

 private:
};

class TrajectorySegments {
 public:
  TrajectorySegments(){};
  virtual ~TrajectorySegments(){};
  std::vector<Eigen::Vector3d> position() {
    std::vector<Eigen::Vector3d> pos_vector;
    for (auto segment : segments) {
      std::vector<Eigen::Vector3d> segment_pos = segment.position();
      pos_vector.insert(pos_vector.end(), segment_pos.begin(), segment_pos.end());
    }
    return pos_vector;
  }
  std::vector<Eigen::Vector3d> velocity() {
    std::vector<Eigen::Vector3d> vel_vector;
    for (auto segment : segments) {
      std::vector<Eigen::Vector3d> segment_vel = segment.velocity();
      vel_vector.insert(vel_vector.end(), segment_vel.begin(), segment_vel.end());
    }
    return vel_vector;
  }
  void resetSegments() { segments.clear(); };
  void appendSegment(const Trajectory &trajectory) { segments.push_back(trajectory); };
  Trajectory lastSegment() { return segments.back(); }
  bool valid() { return validity; }
  std::vector<State> states;
  double utility{0.0};
  bool validity{false};
  std::vector<Trajectory> segments;

 private:
};

#endif
