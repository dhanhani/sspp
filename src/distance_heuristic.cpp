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
#include "heuristic_interface.h"
#include "distance_heuristic.h"
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include "rviz_visual_tools/rviz_visual_tools.h"

namespace SSPP
{

DistanceHeuristic::DistanceHeuristic(ros::NodeHandle & nh, bool d)
{
    debug   = d;
    treePub = nh.advertise<visualization_msgs::Marker>("search_tree", 10);
}

double DistanceHeuristic::gCost(Node *node)
{
    double cost;
    if(node == NULL || node->parent==NULL)
        return 0.0;
    cost = node->parent->g_value + Dist(node->pose.p,node->parent->pose.p);
    return cost;
}

double DistanceHeuristic::hCost(Node *node)
{
    double h=0;
    if(node == NULL)
        return(0);
    // Using the Euclidean distance
    h = Dist(endPose,node->pose.p);
    return h;
}

bool DistanceHeuristic::terminateConditionReached(Node *node)
{
    double deltaDist;
    deltaDist = Dist(node->pose.p,endPose);
    if ( deltaDist <= tolerance2Goal)
        return true;
    else
        return false;
}

bool DistanceHeuristic::isConnectionConditionSatisfied(SearchSpaceNode *temp, SearchSpaceNode *S)
{
    //TODO::do a collision check with the map
    return true;
}
bool DistanceHeuristic::isFilteringConditionSatisfied(geometry_msgs::Pose pose, geometry_msgs::PoseArray& correspondingSensorPoses, double minDist, double maxDist)
{
    //TODO::preform filtering check
    return true;
}
void DistanceHeuristic::displayProgress(vector<Tree> tree)
{
    rviz_visual_tools::RvizVisualToolsPtr visualTools;
    visualTools.reset(new rviz_visual_tools::RvizVisualTools("map","/sspp_visualisation"));
    geometry_msgs::Pose child;
    std::vector<geometry_msgs::Point> lineSegments;
    geometry_msgs::Point linePoint1,linePoint2;
    for(unsigned int k=0;k<tree.size();k++)
    {
        for(int j=0;j<tree[k].children.size();j++)
        {
            child = tree[k].children[j];

            linePoint1.x = tree[k].location.position.x;
            linePoint1.y = tree[k].location.position.y;
            linePoint1.z = tree[k].location.position.z;
            lineSegments.push_back(linePoint1);

            linePoint2.x = child.position.x;
            linePoint2.y = child.position.y;
            linePoint2.z = child.position.z;
            lineSegments.push_back(linePoint2);

            visualTools->publishLine(linePoint1,linePoint2,rviz_visual_tools::GREEN, rviz_visual_tools::LARGE);
        }
    }
    //visualTools->publishPath(lineSegments, rviz_visual_tools::GREEN, rviz_visual_tools::LARGE,"search_tree");
    /*
    visualization_msgs::Marker linesList = drawLines(lineSegments,1000000,2,100000000,0.08);
    treePub.publish(linesList);
    */
}

bool DistanceHeuristic::isCost()
{
    return true;
}

void DistanceHeuristic::setDebug(bool debug)
{
    this->debug = debug;
}

void DistanceHeuristic::setEndPose(geometry_msgs::Pose p)
{
    this->endPose = p;
}

void DistanceHeuristic::setTolerance2Goal(double tolerance2Goal)
{
    this->tolerance2Goal = tolerance2Goal;
}

void DistanceHeuristic::calculateHeuristic(Node *node)
{
    if(node==NULL)
        return;
    node->g_value = gCost(node);
    node->h_value = hCost(node);
    node->f_value = node->g_value + node->h_value;
}

}
