#ifndef TERRAIN_PLANNER_OMPL_SETUP_H_
#define TERRAIN_PLANNER_OMPL_SETUP_H_

#include "terrain_planner/DubinsAirplane.hpp"
#include "terrain_planner/terrain_ompl.h"

#include <ompl/base/objectives/PathLengthOptimizationObjective.h>

#include <ompl/base/spaces/SE3StateSpace.h>
#include <ompl/geometric/SimpleSetup.h>
#include <ompl/geometric/planners/fmt/FMT.h>
#include <ompl/geometric/planners/informedtrees/BITstar.h>
#include <ompl/geometric/planners/rrt/RRTstar.h>
#include "ompl/base/SpaceInformation.h"

namespace ompl {

class OmplSetup : public geometric::SimpleSetup {
 public:
  OmplSetup() : geometric::SimpleSetup(base::StateSpacePtr(new fw_planning::spaces::DubinsAirplaneStateSpace())) {}

  void setDefaultObjective() {
    setOptimizationObjective(
        ompl::base::OptimizationObjectivePtr(new ompl::base::PathLengthOptimizationObjective(getSpaceInformation())));
  }

  void setDefaultPlanner() {
    // setRrtStar();
    setBitStar();
    // setFmtStar();
  }

  void setRrtStar() { setPlanner(ompl::base::PlannerPtr(new ompl::geometric::RRTstar(getSpaceInformation()))); }
  void setBitStar() { setPlanner(ompl::base::PlannerPtr(new ompl::geometric::BITstar(getSpaceInformation()))); }
  void setFmtStar() { setPlanner(ompl::base::PlannerPtr(new ompl::geometric::FMT(getSpaceInformation()))); }

  const base::StateSpacePtr& getGeometricComponentStateSpace() const { return getStateSpace(); }

  void setStateValidityCheckingResolution(double resolution) {
    // This is a protected attribute, so need to wrap this function.
    si_->setStateValidityCheckingResolution(resolution);
  }

  void setTerrainCollisionChecking(const grid_map::GridMap& map) {
    std::shared_ptr<TerrainValidityChecker> validity_checker(new TerrainValidityChecker(getSpaceInformation(), map));

    setStateValidityChecker(base::StateValidityCheckerPtr(validity_checker));
  }
};

}  // namespace ompl

#endif
