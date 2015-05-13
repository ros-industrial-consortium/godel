/*
  Copyright May 7, 2014 Southwest Research Institute

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#ifndef SURFACE_BLENDING_SERVICE_H
#define SURFACE_BLENDING_SERVICE_H


#include <godel_surface_detection/scan/robot_scan.h>
#include <godel_surface_detection/detection/surface_detection.h>
#include <godel_surface_detection/interactive/interactive_surface_server.h>

#include <godel_msgs/SurfaceDetection.h>
#include <godel_msgs/SelectSurface.h>
#include <godel_msgs/SelectedSurfacesChanged.h>
#include <godel_msgs/ProcessPlanning.h>
#include <godel_msgs/SurfaceBlendingParameters.h>
#include "godel_msgs/TrajectoryPlanning.h"

#include <godel_process_path_generation/VisualizeBlendingPlan.h>
#include <godel_process_path_generation/mesh_importer.h>
#include <godel_process_path_generation/utils.h>
#include <godel_process_path_generation/polygon_utils.h>

#include <godel_surface_detection/scan/profilimeter_scan.h>

// Load helpers for reading/writing parameters
#include <godel_msgs/blending_params_helper.h>
#include <param_helpers/param_set.h>

#include <pcl/console/parse.h>
#include <rosbag/bag.h>

// topics and services
const static std::string TRAJECTORY_PLANNING_SERVICE = "trajectory_planner";
const static std::string SURFACE_DETECTION_SERVICE = "surface_detection";
const static std::string SURFACE_BLENDING_PARAMETERS_SERVICE = "surface_blending_parameters";
const static std::string SELECT_SURFACE_SERVICE = "select_surface";
const static std::string PROCESS_PATH_SERVICE="process_path";
const static std::string VISUALIZE_BLENDING_PATH_SERVICE = "visualize_path_generator";
const static std::string TOOL_PATH_PREVIEW_TOPIC = "tool_path_preview";
const static std::string SELECTED_SURFACES_CHANGED_TOPIC = "selected_surfaces_changed";
const static std::string ROBOT_SCAN_PATH_PREVIEW_TOPIC = "robot_scan_path_preview";
const static std::string PUBLISH_REGION_POINT_CLOUD = "publish_region_point_cloud";
const static std::string REGION_POINT_CLOUD_TOPIC="region_colored_cloud";

//  marker namespaces
const static std::string BOUNDARY_NAMESPACE = "process_boundary";
const static std::string PATH_NAMESPACE = "process_path";
const static std::string TOOL_NAMESPACE = "process_tool";

//  tool visual properties
const static float TOOL_DIA = .050;
const static float TOOL_THK = .005;
const static float TOOL_SHAFT_DIA = .006;
const static float TOOL_SHAFT_LEN = .045;
const static std::string TOOL_FRAME_ID = "process_tool";

struct ProcessPathDetails
{
  visualization_msgs::MarkerArray process_boundaries_;
  visualization_msgs::MarkerArray process_paths_;
  visualization_msgs::MarkerArray tool_parts_;
  visualization_msgs::MarkerArray scan_paths_; // profilimeter
};

/**
 * Associates a name with a visual msgs marker which contains a pose and sequence of points defining
 * a path
 */
struct ProcessPathResult {
  typedef std::pair<std::string, visualization_msgs::Marker> value_type;
  std::vector<value_type> paths;
};
/**
 * Associates a name with a joint trajectory
 */
struct ProcessPlanResult {
  typedef std::pair<std::string, trajectory_msgs::JointTrajectory> value_type;
  std::vector<value_type> plans;
};

class SurfaceBlendingService
{
public:
  SurfaceBlendingService();

  bool init();
  void run();

private:
  bool load_parameters(const std::string& filename, const std::string& ns);

  void save_parameters(const std::string& filename, const std::string& ns);

  void publish_selected_surfaces_changed();

  bool run_robot_scan(visualization_msgs::MarkerArray &surfaces);

  bool find_surfaces(visualization_msgs::MarkerArray &surfaces);

  void remove_previous_process_plan();
  
  /**
   * The following path generation and planning methods are defined in
   * src/blending_service_path_generation.cpp
   */
  bool generate_process_plan(godel_process_path_generation::VisualizeBlendingPlan &process_plan);

  void scan_planning_timer_callback(const ros::TimerEvent&);

  void trajectory_planning_timer_callback(const ros::TimerEvent&);

  bool animate_tool_path();

  void tool_animation_timer_callback(const ros::TimerEvent&);
  
  visualization_msgs::MarkerArray create_tool_markers(const geometry_msgs::Point &pos, 
                                                      const geometry_msgs::Pose &pose,
                                                      std::string frame_id);

  // Service callbacks, these components drive this class by signalling events
  // from the user
  bool surface_detection_server_callback(godel_msgs::SurfaceDetection::Request &req,
                                         godel_msgs::SurfaceDetection::Response &res);

  bool select_surface_server_callback(godel_msgs::SelectSurface::Request &req, 
                                      godel_msgs::SelectSurface::Response &res);

  bool process_path_server_callback(godel_msgs::ProcessPlanning::Request &req, 
                                    godel_msgs::ProcessPlanning::Response &res);
  
  bool surface_blend_parameters_server_callback(godel_msgs::SurfaceBlendingParameters::Request &req, 
                                                godel_msgs::SurfaceBlendingParameters::Response &res);

  bool requestBlendPath(const godel_process_path::PolygonBoundaryCollection& boundaries,
                        const geometry_msgs::Pose& boundary_pose,
                        const godel_msgs::BlendingPlanParameters& params,
                        visualization_msgs::Marker& path);

  bool requestScanPath(const godel_process_path::PolygonBoundaryCollection& boundaries,
                       const geometry_msgs::Pose& boundary_pose,
                       visualization_msgs::Marker& path);

  ProcessPathResult generateProcessPath(const std::string& name, 
                                        const pcl::PolygonMesh& mesh, 
                                        const godel_msgs::BlendingPlanParameters& params);

  // Services offered by this class
  ros::ServiceServer surface_detect_server_;
  ros::ServiceServer select_surface_server_;
  ros::ServiceServer process_path_server_;
  ros::ServiceServer surf_blend_parameters_server_;
  // Services subscribed to by this class
  ros::ServiceClient visualize_process_path_client_;
  ros::ServiceClient trajectory_planner_client_;
  // Current state publishers
  ros::Publisher selected_surf_changed_pub_;
  ros::Publisher point_cloud_pub_;
  ros::Publisher tool_path_markers_pub_;
  // Timers
  ros::Timer tool_animation_timer_;
  bool stop_tool_animation_;
  ros::Timer trajectory_planning_timer_;
  ros::Timer scan_planning_timer_;

  // robot scan instance
  godel_surface_detection::scan::RobotScan robot_scan_;
  // surface detection instance
  godel_surface_detection::detection::SurfaceDetection surface_detection_;
  // marker server instance
  godel_surface_detection::interactive::InteractiveSurfaceServer surface_server_;
  // mesh importer for generating surface boundaries
  godel_process_path::MeshImporter mesh_importer_;

  // parameters
  godel_msgs::RobotScanParameters default_robot_scan_params__;
  godel_msgs::SurfaceDetectionParameters default_surf_detection_params_;
  godel_msgs::BlendingPlanParameters default_blending_plan_params_;
  godel_msgs::BlendingPlanParameters blending_plan_params_;

  // results
  godel_msgs::SurfaceDetection::Response latest_surface_detection_results_;
  ProcessPathDetails process_path_results_;
  std::vector<std::vector<ros::Duration> > duration_results_; // returned by visualize plan service, needed by trajectory planner

  // parameters
  bool publish_region_point_cloud_;

  // msgs
  sensor_msgs::PointCloud2 region_cloud_msg_;
};

#endif // surface blending services
