/*
* Software License Agreement (Apache License)
*
* Copyright (c) 2014, Southwest Research Institute
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
/*
 * path_planning_node.cpp
 *
 *  Created on: May 29, 2014
 *      Author: Dan Solomon
 */


#include <ros/ros.h>
#include "godel_msgs/TrajectoryPlanning.h"

#include "godel_path_planning/trajectory_planning.h"

bool trajectory_planning_callback(godel_msgs::TrajectoryPlanning::Request& req,
                                  godel_msgs::TrajectoryPlanning::Response& res)
{
  return godel_path_planning::generateTrajectory(req, res.trajectory);
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "godel_trajectory_planner");
  ros::NodeHandle nh;

  ros::ServiceServer trajectory_server = nh.advertiseService("trajectory_planner", trajectory_planning_callback);

  ROS_INFO("%s ready to service requests.", trajectory_server.getService().c_str());

  ros::spin();

  return 0;
}