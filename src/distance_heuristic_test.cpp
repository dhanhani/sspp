/***************************************************************************
 *   Copyright (C) 2006 - 2016 by                                          *
 *      Tarek Taha, KURI  <tataha@tarektaha.com>                           *
 *      Randa Almadhoun   <randa.almadhoun@kustar.ac.ae>                   *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02111-1307, USA.          *
 ***************************************************************************/

#include "pathplanner.h"
#include "ros/ros.h"
#include <ros/package.h>
#include <tf/tf.h>
#include <tf_conversions/tf_eigen.h>
#include <geometry_msgs/Pose.h>
#include <eigen_conversions/eigen_msg.h>

#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseArray.h>

//#include <component_test/occlusion_culling_gpu.h>
//#include <component_test/occlusion_culling.h>
//#include "coverage_path_planning_heuristic.h"
#include "distance_heuristic.h"
#include "rviz_drawing_tools.h"
#include "rviz_visual_tools/rviz_visual_tools.h"

using namespace SSPP;

int main(int argc, char ** argv) {
    ros::init(argc, argv, "path_planning");
    ros::NodeHandle nh;
    ros::Publisher robotPosePub = nh.advertise<geometry_msgs::PoseArray>("robot_pose", 10);
    ros::Publisher sensorPosePub = nh.advertise<geometry_msgs::PoseArray>("sensor_pose", 10);
    rviz_visual_tools::RvizVisualToolsPtr visualTools;
    visualTools.reset(new rviz_visual_tools::RvizVisualTools("map", "/sspp_visualisation"));
    visualTools->deleteAllMarkers();
    visualTools->setLifetime(0.2);

    QTime timer;
    geometry_msgs::Pose gridStartPose;
    geometry_msgs::Vector3 gridSize;
    gridStartPose.position.x = 0;
    gridStartPose.position.y = 0;
    gridStartPose.position.z = 0;
    gridSize.x = 20;
    gridSize.y = 10;
    gridSize.z = 2;

    PathPlanner * pathPlanner;
    Pose start(0.0, 0.0, 0, DTOR(0.0));
    Pose end(5.0, 7.0, 2, DTOR(0.0));

    double robotH = 0.9, robotW = 0.5, narrowestPath = 0.987; //is not changed
    double distanceToGoal = 1.0, regGridConRad = 1.5;

    QPointF robotCenter(-0.3f, 0.0f);
    Robot *robot = new Robot("Robot", robotH, robotW, narrowestPath, robotCenter);

    // Every how many iterations to display the tree
    int progressDisplayFrequency = 1;
    pathPlanner = new PathPlanner(nh, robot, regGridConRad, progressDisplayFrequency);
    // This causes the planner to pause for the desired amount of time and display the search tree, useful for debugging
    pathPlanner->setDebugDelay(0.1);
    /*
    double coverageTolerance=0.5, targetCov=10;
    std::string modelPath = ros::package::getPath("component_test") + "/src/mesh/etihad_nowheels.obj";
    CoveragePathPlanningHeuristic coveragePathPlanningHeuristic(nh,modelPath,false);
    coveragePathPlanningHeuristic.setCoverageTarget(targetCov);
    coveragePathPlanningHeuristic.setCoverageTolerance(coverageTolerance);
    pathPlanner->setHeuristicFucntion(&coveragePathPlanningHeuristic);
     */

    DistanceHeuristic distanceHeuristic(nh, false);
    distanceHeuristic.setEndPose(end.p);
    distanceHeuristic.setTolerance2Goal(distanceToGoal);
    pathPlanner->setHeuristicFucntion(&distanceHeuristic);

    // Generate Grid Samples and visualise it
    pathPlanner->generateRegularGrid(gridStartPose, gridSize, 1.0, false, 180, false);
    std::vector<geometry_msgs::Point> searchSpaceNodes = pathPlanner->getSearchSpace();
    geometry_msgs::PoseArray robotPoseSS, sensorPoseSS;
    pathPlanner->getRobotSensorPoses(robotPoseSS, sensorPoseSS);
    std::cout << "\n" << QString("\n---->>> Total Nodes in search Space =%1").arg(searchSpaceNodes.size()).toStdString();
    visualTools->publishSpheres(searchSpaceNodes, rviz_visual_tools::PURPLE, 0.1, "search_space_nodes");

    // Connect nodes and visualise it
    pathPlanner->connectNodes();
    std::cout << "\nSpace Generation took:" << timer.elapsed() / double(1000.00) << " secs";
    std::vector<geometry_msgs::Point> searchSpaceConnections = pathPlanner->getConnections();
    visualTools->publishPath(searchSpaceConnections, rviz_visual_tools::BLUE, rviz_visual_tools::LARGE, "search_space");
    std::cout << "\n searchSpaceConnections1="<<searchSpaceConnections.size();

    // Find path and visualise it
    timer.restart();
    Node * retval = pathPlanner->startSearch(start);
    std::cout << "\n1. Path Finding took:" << (timer.elapsed() / double(1000.00)) << " secs";

    //path print and visualization
    if (retval) {
        pathPlanner->printNodeList();
    } else {
        std::cout << "\nNo Path Found";
    }

    Node * path = pathPlanner->path;
  
    geometry_msgs::Point linePoint;
    std::vector<geometry_msgs::Point> pathSegments;
        std::vector<geometry_msgs::Point> pathSegments2;
    geometry_msgs::PoseArray robotPose, sensorPose;
    double dist = 0;
    double yaw;
    while (path != NULL) {
        tf::Quaternion qt(path->pose.p.orientation.x, path->pose.p.orientation.y, path->pose.p.orientation.z, path->pose.p.orientation.w);
        yaw = tf::getYaw(qt);
        if (path->next != NULL) {
            linePoint.x = path->pose.p.position.x;
            linePoint.y = path->pose.p.position.y;
            linePoint.z = path->pose.p.position.z;
            robotPose.poses.push_back(path->pose.p);
            for (int i = 0; i < path->senPoses.size(); i++)
                sensorPose.poses.push_back(path->senPoses[i].p);
            pathSegments.push_back(linePoint);

            linePoint.x = path->next->pose.p.position.x;
            linePoint.y = path->next->pose.p.position.y;
            linePoint.z = path->next->pose.p.position.z;
            robotPose.poses.push_back(path->next->pose.p);
            for (int i = 0; i < path->next->senPoses.size(); i++)
                sensorPose.poses.push_back(path->next->senPoses[i].p);
            pathSegments.push_back(linePoint);

            dist = dist + Dist(path->next->pose.p, path->pose.p);
        }
        path = path->next;
    }
    visualTools->publishPath(pathSegments, rviz_visual_tools::RED, rviz_visual_tools::LARGE, "generated_path");

    ros::Rate loopRate(10);
    std::cout << "\nDistance calculated from the path: " << dist << "m\n";
    //Disconnect path and regenrate
    pathPlanner->freeResources();
        pathPlanner->generateRegularGrid(gridStartPose, gridSize, 1.0, false, 180, false);
    searchSpaceNodes = pathPlanner->getSearchSpace();
     //disconnect path
    //pathPlanner->disconnectNodes(path);
    pathPlanner->regenerateConnections(path);
    timer.restart();
    Pose start2(10.0, 10.0, 0, DTOR(0.0));
    Pose end2(0.0, 0.0, 0, DTOR(0.0));
    distanceHeuristic.setEndPose(end2.p);
    retval = pathPlanner->startSearch(start2);
    std::cout << "\n2. Path Finding took:" << (timer.elapsed() / double(1000.00)) << " secs";
    //path print and visualization
    if (retval) {
        pathPlanner->printNodeList();
    } else {
        std::cout << "\nNo Path Found";
    }
        Node * path2 = pathPlanner->path;
    std::vector<geometry_msgs::Point> searchSpaceConnections2 = pathPlanner->getConnections();
    std::cout << "\n searchSpaceConnections2="<<searchSpaceConnections2.size()<<std::endl;
    visualTools->publishPath(searchSpaceConnections2, rviz_visual_tools::YELLOW, rviz_visual_tools::LARGE, "new_search_space");
    
      while (path2 != NULL) {
        tf::Quaternion qt(path2->pose.p.orientation.x, path2->pose.p.orientation.y, path2->pose.p.orientation.z, path2->pose.p.orientation.w);
        yaw = tf::getYaw(qt);
        if (path2->next != NULL) {
            linePoint.x = path2->pose.p.position.x;
            linePoint.y = path2->pose.p.position.y;
            linePoint.z = path2->pose.p.position.z;
            robotPose.poses.push_back(path2->pose.p);
            for (int i = 0; i < path2->senPoses.size(); i++)
                sensorPose.poses.push_back(path2->senPoses[i].p);
            pathSegments2.push_back(linePoint);

            linePoint.x = path2->next->pose.p.position.x;
            linePoint.y = path2->next->pose.p.position.y;
            linePoint.z = path2->next->pose.p.position.z;
            robotPose.poses.push_back(path2->next->pose.p);
            for (int i = 0; i < path2->next->senPoses.size(); i++)
                sensorPose.poses.push_back(path2->next->senPoses[i].p);
            pathSegments2.push_back(linePoint);

            dist = dist + Dist(path2->next->pose.p, path2->pose.p);
        }
        path2 = path2->next;
    }
    visualTools->publishPath(pathSegments2, rviz_visual_tools::YELLOW, rviz_visual_tools::LARGE, "generated_path2");
    
    
    //End disconnect test
    
    
    while (ros::ok()) {
        visualTools->resetMarkerCounts();
        for (int i = 0; i < robotPose.poses.size(); i++) {
            visualTools->publishArrow(robotPose.poses[i], rviz_visual_tools::YELLOW, rviz_visual_tools::LARGE, 0.3);
        }
        visualTools->publishSpheres(searchSpaceNodes, rviz_visual_tools::PURPLE, 0.1, "search_space_nodes");
        visualTools->publishPath(searchSpaceConnections, rviz_visual_tools::BLUE, rviz_visual_tools::LARGE, "search_space");
        visualTools->publishPath(searchSpaceConnections2, rviz_visual_tools::YELLOW, rviz_visual_tools::LARGE, "new_search_space");
        visualTools->publishPath(pathSegments, rviz_visual_tools::RED, rviz_visual_tools::LARGE, "generated_path");
        visualTools->publishPath(pathSegments2, rviz_visual_tools::YELLOW, rviz_visual_tools::LARGE, "generated_path2");
        robotPoseSS.header.frame_id = "map";
        robotPoseSS.header.stamp = ros::Time::now();
        robotPosePub.publish(robotPoseSS);

        sensorPoseSS.header.frame_id = "map";
        sensorPoseSS.header.stamp = ros::Time::now();
        sensorPosePub.publish(sensorPoseSS);
        ros::spinOnce();
        loopRate.sleep();
    }
    delete robot;
    delete pathPlanner;
    return 0;
}


