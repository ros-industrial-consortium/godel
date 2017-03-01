#include <godel_process_planning/godel_process_planning.h>

#include <ros/console.h>

// descartes
#include "descartes_trajectory/axial_symmetric_pt.h"
#include "descartes_trajectory/joint_trajectory_pt.h"
#include "descartes_planner/dense_planner.h"

#include "common_utils.h"
#include "generate_motion_plan.h"
#include "eigen_conversions/eigen_msg.h"
#include "boost/make_shared.hpp"

namespace godel_process_planning
{

// Planning Constants
const double BLENDING_ANGLE_DISCRETIZATION =
    M_PI / 12.0; // The discretization of the tool's pose about
                 // the z axis
const static std::string JOINT_TOPIC_NAME =
    "joint_states"; // ROS topic to subscribe to for current robot
                    // state info
/**
 * @brief Translated an Eigen pose to a Descartes trajectory point appropriate for the BLEND
 * process!
 *        Note that this function is local only to this file, and there is a similar function in
 *        the keyence_process_planning.cpp document.
 * @param pose
 * @param dt The upper limit of time from the previous point to achieve this one
 * @return A descartes trajectory point encapsulating a move to this pose
 */
static inline descartes_core::TrajectoryPtPtr toDescartesPt(const Eigen::Affine3d& pose, double dt)
{
  using namespace descartes_trajectory;
  using namespace descartes_core;
  const TimingConstraint tm(dt);
  return boost::make_shared<AxialSymmetricPt>(pose, BLENDING_ANGLE_DISCRETIZATION,
                                              AxialSymmetricPt::Z_AXIS, tm);
}

struct ConnectingPath
{
  EigenSTL::vector_Affine3d depart;
  EigenSTL::vector_Affine3d approach;
};

static std::vector<ConnectingPath>
generateTransitions(const std::vector<geometry_msgs::PoseArray>& segments,
                    const double traverse_height)
{
  const static double APPROACH_STEP_SIZE = 0.02; // m
  const int steps = std::ceil(traverse_height / APPROACH_STEP_SIZE);

  std::vector<ConnectingPath> result;

  // Loop over every connecting edge
  for (std::size_t i = 0; i < segments.size(); ++i)
  {
    const auto& start_pose = segments[i].poses.front(); // First point in this segment
    const auto& end_pose = segments[i].poses.back();    // Last point in this segment

    Eigen::Affine3d e_start, e_end;
    tf::poseMsgToEigen(start_pose, e_start);
    tf::poseMsgToEigen(end_pose, e_end);

    // Each connecting segment has a retraction from 'from_pose'
    // And an approach to 'to_pose'
    auto approach = linearMoveZ(e_start, APPROACH_STEP_SIZE, steps);
    auto depart = linearMoveZ(e_end, APPROACH_STEP_SIZE, steps);
    std::reverse(approach.begin(), approach.end()); // we flip the 'to' path to keep the time ordering of the path

    ConnectingPath c;
    c.depart = std::move(depart);
    c.approach = std::move(approach);
    result.push_back(c);
  }
  return result;
}

/**
 * @brief transforms a sequence of pose-arrays, each representing a single 'segment' of a
 * process path into a Descartes specific format. This function also adds transitions between
 * segments.
 * @param segments Sequence of poses (relative to the world space of blending robot model)
 * @param params Surface blending parameters, including info such as traversal speed
 * @return The input trajectory encoded in Descartes points
 */
static godel_process_planning::DescartesTraj
toDescartesTraj(const std::vector<geometry_msgs::PoseArray>& segments,
                const godel_msgs::BlendingPlanParameters& params)
{
  auto transitions = generateTransitions(segments, params.safe_traverse_height);

  DescartesTraj traj;
  Eigen::Affine3d last_pose = createNominalTransform(segments.front().poses.front());

  // Convert pose arrays to Eigen types
  auto eigen_segments = toEigenArrays(segments);

  // Inline function for adding a sequence of motions
  auto add_segment = [&traj, &last_pose, params] (const EigenSTL::vector_Affine3d& poses, bool free_last)
  {
    // Create Descartes trajectory for the segment path
    for (std::size_t j = 0; j < poses.size(); ++j)
    {
      Eigen::Affine3d this_pose = createNominalTransform(poses[j]);
      // O(1) jerky - may need to revisit this time parameterization later. This at least allows
      // Descartes to perform some optimizations in its graph serach.
      double dt = (this_pose.translation() - last_pose.translation()).norm() / params.traverse_spd;
      if (j == poses.size() - 1 && free_last)
      {
        dt = 0.0;
      }
      traj.push_back(toDescartesPt(this_pose, dt));
      last_pose = this_pose;
    }
  };

  for (std::size_t i = 0; i < segments.size(); ++i)
  {
    add_segment(transitions[i].approach, true);

    add_segment(eigen_segments[i], false);

    add_segment(transitions[i].depart, false);
  } // end segments

  return traj;
}

/**
 * @brief Computes a joint motion plan based on input points and the blending process; this includes
 *        motion from current position to process path and back to the starting position.
 * @param req Process plan including reference pose, points, and process parameters
 * @param res Set of approach, process, and departure trajectories
 * @return True if a valid plan was generated; false otherwise
 */
bool ProcessPlanningManager::handleBlendPlanning(godel_msgs::BlendProcessPlanning::Request& req,
                                                 godel_msgs::BlendProcessPlanning::Response& res)
{
  // Enable Collision Checks
  blend_model_->setCheckCollisions(true);

  // Precondition: There must be at least one input segments
  if (req.path.segments.empty())
  {
    ROS_WARN("Planning request contained no trajectory segments. Nothing to be done.");
    return true;
  }

  // Precondition: All input segments must have at least one pose associated with them
  for (const auto& segment : req.path.segments)
  {
    if (segment.poses.empty())
    {
      ROS_ERROR("Input trajectory segment contained no poses. Invalid input.");
      return false;
    }
  }

  // Transform process path from geometry msgs to descartes points
  std::vector<double> current_joints = getCurrentJointState(JOINT_TOPIC_NAME);

  DescartesTraj process_points = toDescartesTraj(req.path.segments, req.params);

  if (generateMotionPlan(blend_model_, process_points, moveit_model_, blend_group_name_,
                         current_joints, res.plan))
  {
    res.plan.type = res.plan.BLEND_TYPE;
    return true;
  }
  else
  {
    return false;
  }
}

} // end namespace
