/***************************************************************************
 *   Copyright (C) 2006 - 2007 by                                          *
 *      Tarek Taha, CAS-UTS  <tataha@tarektaha.com>                        *
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
#include "astar.h"

namespace SSPP
{

Astar::Astar(ros::NodeHandle & n,Robot *rob,double dG, double cT,QString heuristicT):
    nh(n),
    distGoal(dG),
    covTolerance(cT),
    map(NULL),
    robot(rob),
    root(NULL),
    test(NULL),
    path(NULL),
    p(NULL),
    openList(NULL),
    closedList(NULL)
{
   // if (heuristicT == "Distance")
    //{
        try
        {
            heuristic = Heuristic::factory(heuristicT);
        }
        catch(SSPPException e)
        {
            cout<<e.what()<<endl;
        }
    //}
    orientation2Goal = DTOR(60);
    obj = new OcclusionCulling("etihad.pcd");
    covFilteredCloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud <pcl::PointXYZ>);

}

Astar::Astar():
    distGoal(0.01),
    covTolerance(0.05),
    heuristic(NULL),
    map(NULL),
    root(NULL),
    test(NULL),
    path(NULL),
    p(NULL),
    openList(NULL),
    closedList(NULL)
{
    try
    {
        heuristic = Heuristic::factory("Distance");
    }
    catch(SSPPException e)
    {
        cout<<e.what()<<endl;
    }
    distGoal = 1;
    orientation2Goal = DTOR(180);
    obj = new OcclusionCulling("etihad.pcd");
    covFilteredCloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud <pcl::PointXYZ>);

}

Astar::~Astar()
{
    if(heuristic)
        delete heuristic;
    if(openList)
    {
        openList->free();
        delete openList;
    }
    if(closedList)
    {
        closedList->free();
        delete closedList;
    }
}

void Astar::displayTree()
{
    geometry_msgs::Pose child;
    for(unsigned int k=0;k<tree.size();k++)
    {
        for(int j=0;j<tree[k].children.size();j++)
        {
            child = tree[k].children[j];

            linePoint.x = tree[k].location.position.x;
            linePoint.y = tree[k].location.position.y;
            linePoint.z = tree[k].location.position.z;
            lineSegments.push_back(linePoint);
            linePoint.x = child.position.x;
            linePoint.y = child.position.y;
            linePoint.z = child.position.z;
            lineSegments.push_back(linePoint);

        }
    }
    visualization_msgs::Marker linesList = drawLines(lineSegments);
    treePub.publish(linesList);
}

visualization_msgs::Marker Astar::drawLines(std::vector<geometry_msgs::Point> links)
{
    visualization_msgs::Marker linksMarkerMsg;
    linksMarkerMsg.header.frame_id="/map";
    linksMarkerMsg.header.stamp=ros::Time::now();
    linksMarkerMsg.ns="link_marker1";
    linksMarkerMsg.id = 0;
    linksMarkerMsg.type = visualization_msgs::Marker::LINE_LIST;
    linksMarkerMsg.scale.x = 0.05;
    linksMarkerMsg.action  = visualization_msgs::Marker::ADD;
    linksMarkerMsg.lifetime  = ros::Duration(100000.0);
    std_msgs::ColorRGBA color;
    color.r = 0.0f; color.g=1.0f; color.b=.0f, color.a=1.0f;
    std::vector<geometry_msgs::Point>::iterator linksIterator;
    for(linksIterator = links.begin();linksIterator != links.end();linksIterator++)
    {
        linksMarkerMsg.points.push_back(*linksIterator);
        linksMarkerMsg.colors.push_back(color);
    }
   return linksMarkerMsg;
}

void  Astar::setSocialReward(QHash<QString, int>* soRew)
{
    try
    {
        heuristic = Heuristic::factory("Social",soRew);
    }
    catch(SSPPException e)
    {
        std::cout<<"\nCritical:"<<e.what();
    }
}

// Tests for whether a node is in an obstacle or not
//int Astar::inObstacle(geometry_msgs::Pose P, double theta)
//{
//    int m,n;
//    geometry_msgs::Pose det_point;
//    // Rotates and Translates the check points according to the vehicle position and orientation
//    for (int i=0;i<robot->check_points.size();i++)
//    {
//        det_point.position.x = robot->check_points[i].x()*cos(theta) - robot->check_points[i].y()*sin(theta) + P.position.x;
//        det_point.position.y = robot->check_points[i].x()*sin(theta) + robot->check_points[i].y()*cos(theta) + P.position.y;

//        map->convert2Pix(&det_point);
//        m = (int)(round(det_point.position.x));
//        n = (int)(round(det_point.position.y));
//        if (m <= 0 || n <= 0 || m >=map->width || n >=this->map->height)
//            return 1;
//        if (this->map->grid[m][n])
//            return 1;
//    }
//    return 0;
//};
// find the nearest node to the start
void Astar::findRoot() throw (SSPPException)
{
    SearchSpaceNode * temp;
    if(!this->search_space)
    {
        throw(SSPPException((char*)"No SearchSpace Defined"));
        return;
    }
    double distance,shortest_distance = 100000;
    // allocate and setup the root node
    root = new Node;
    temp = this->search_space;
    while(temp!=NULL)
    {
        distance = Dist(temp->location,start.p);
        // Initialize the root node information and put it in the open list
        if (distance < shortest_distance)
        {
            shortest_distance = distance;
            root->pose.p.position.x = temp->location.position.x;
            root->pose.p.position.y = temp->location.position.y;
            root->pose.p.position.z = temp->location.position.z;//newly added
            root->pose.p.orientation.x = temp->location.orientation.x;
            root->pose.p.orientation.y = temp->location.orientation.y;
            root->pose.p.orientation.z = temp->location.orientation.z;
            root->pose.p.orientation.w = temp->location.orientation.w;
            root->senPose.p.position.x = temp->sensorLocation.position.x;
            root->senPose.p.position.y = temp->sensorLocation.position.y;
            root->senPose.p.position.z = temp->sensorLocation.position.z;
            root->senPose.p.orientation.x = temp->sensorLocation.orientation.x;
            root->senPose.p.orientation.y = temp->sensorLocation.orientation.y;
            root->senPose.p.orientation.z = temp->sensorLocation.orientation.z;
            root->senPose.p.orientation.w = temp->sensorLocation.orientation.w;
            root->id = temp->id;


        }
        temp = temp->next;
    }
    //************voxelgrid***********
    pcl::PointCloud<pcl::PointXYZ>::Ptr tempCloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ> temp_cloud;
    temp_cloud = obj->extractVisibleSurface(root->senPose.p);//extract visible surface at the sensor position
    tempCloud->points = temp_cloud.points;
    pcl::VoxelGrid<pcl::PointXYZ> voxelgrid;
    voxelgrid.setInputCloud (tempCloud);
    voxelgrid.setLeafSize (0.5f, 0.5f, 0.5f);
    voxelgrid.filter (*root->cloud_filtered);
    std::cout<<"root cloud before filtering size: "<<tempCloud->size()<<"\n";
    std::cout<<"root cloud after filtering size: "<<root->cloud_filtered->size()<<"\n";

    root->id = 0;
    root->parent = NULL;
    root->next = NULL;
    root->prev = NULL;
    root->g_value = 0;
    root->distance = 0;
    root->coverage =0;
    root->h_value = 0;//heuristic->gCost(root);
    root->f_value = root->g_value + root->h_value;
    root->depth = 0;
//    root->pose.phi = start.phi;
//    root->direction = FORWARD;
    //Translate(root->pose,start.phi);
    std::cout<<"\n"<<QString("	---->>>Root is Set to be X=%1 Y=%2 Z=%3").arg(root->pose.p.position.x).arg(root->pose.p.position.y).arg(root->pose.p.position.z).toStdString();
}

// find the nearest node to the end //not used when surface coverage heuristic is used
void Astar::findDest() throw (SSPPException)
{
    SearchSpaceNode * temp;
    if(!this->search_space)
    {
        throw(SSPPException((char*)"No SearchSpace Defined"));
        return;
    }
    double distance,shortest_distance = 100000;
    // allocate and setup the root node
    dest = new Node;
    temp = this->search_space;
    while(temp!=NULL)
    {
        distance = Dist(temp->location,end.p);
        // Initialize the root node information and put it in the open list
        if (distance < shortest_distance)
        {
            shortest_distance = distance;
            dest->pose.p.position.x = temp->location.position.x;
            dest->pose.p.position.y = temp->location.position.y;
            dest->pose.p.position.z = temp->location.position.z;//newly added
            dest->pose.p.orientation.x = temp->location.orientation.x;
            dest->pose.p.orientation.y = temp->location.orientation.y;
            dest->pose.p.orientation.z = temp->location.orientation.z;
            dest->pose.p.orientation.w = temp->location.orientation.w;

            dest->id = temp->id;
        }
        temp = temp->next;
    }
    dest->parent = NULL;
    dest->next = NULL;
    dest->prev = NULL;
    dest->g_value = 0;;
    dest->h_value = 0;
    dest->f_value = 0;
    dest->depth = 0;
//    dest->pose.phi = end.phi;
//    dest->direction = FORWARD;
    //Translate(root->pose,start.phi);
}

void Astar::setRobot(Robot*rob)
{
    this->robot = rob;
}


Node *  Astar::startSearch(Pose start,double targetCov, int coord)
{
    int      ID = 1;
    int      NodesExpanded = 0;
    if(this->tree.size() > 0)
        this->tree.clear();
    if(!openList)
    {
        openList   = new LList;
    }
    if(!closedList)
    {
        closedList = new LList;
    }
    // Be sure that open and closed lists are empty
    openList->free();
    closedList->free();
    if (!this->search_space) // Make sure that we have a map to search
    {
        //        LOG(Logger::Warning,"Read the map and generate SearchSpace before Searching !!!")
        return NULL;
    }
    if(coord == PIXEL)
    {
        map->convertPix(&start.p);
        map->convertPix(&end.p);
    }
    this->start.p.position.x = start.p.position.x;
    this->start.p.position.y = start.p.position.y;
    this->start.p.position.z = start.p.position.z;
    this->start.p.orientation.x = start.p.orientation.x;
    this->start.p.orientation.y = start.p.orientation.y;
    this->start.p.orientation.z = start.p.orientation.z;
    this->start.p.orientation.w = start.p.orientation.w;

    this->targetCov = targetCov;
//    this->start.phi = start.phi;
//    this->end.p.position.x = end.p.position.x;
//    this->end.p.position.y = end.p.position.y;
//    this->end.p.position.z = end.p.position.z;
//    this->end.p.orientation.x = end.p.orientation.x;
//    this->end.p.orientation.y = end.p.orientation.y;
//    this->end.p.orientation.z = end.p.orientation.z;
//    this->end.p.orientation.w = end.p.orientation.w;

//    this->end.phi = end.phi;
    std::cout<<"\n	--->>> Search Started <<<---";
    findRoot();
//    findDest();
//    std::cout<<"\n"<<QString("	---->>>Target is Set to be X=%1 Y=%2 Z=%3<<<---").arg(end.p.position.x).arg(end.p.position.y).arg(end.p.position.z).toStdString();
    openList->add(root);				// add the root to OpenList
    // while openList is not empty
    int count = 0;
    while (openList->Start != NULL)
    {
        if((count++%1) == 0)
        {
            displayTree();
        }
        current = openList->getHead(); 	// Get the node with the highest cost (first node) (it was the cheapest one before since we were taking the lower cost but now it is converted to a reward function)
        openList->next();				// Move to the next Node
        NodesExpanded++;
        // We reached the target pose, so build the path and return it.
        if (surfaceCoverageReached(current) && current!= root)//change goalReached to surfaceCoverageReached
        {
            // build the complete path to return
            //			qDebug("Last Node destination: %f %f",current->pose.p.x(),current->pose.p.y());
            current->next = NULL;//the last node in the path
            covFilteredCloud->points = current->cloud_filtered->points; //getting the accumelated cloud to check the coverage and display it in the test code
            std::cout<<"*************commulative distance : "<<current->distance<<"************ \n";
            std::cout<<"*************commulative coverage : "<<current->coverage<<"************ \n";

            std::cout<<"\n"<<QString("	--->>> Goal state reached with :%1 nodes created and :%2 nodes expanded <<<---").arg(ID).arg(NodesExpanded).toStdString();
            //			qDebug("	--->>> General Clean UP <<<---");
            fflush(stdout);
            //			int m=0;
            //	   		while (p != NULL)
            //				{
            //					cout<<"\n	--->>> Step["<<++m<<"] X="<<p->pose.p.x()<<" Y="<<p->pose.p.y();
            //					//cout<<"\n	--->>> Angle is="<<RTOD(p->angle);
            //					fflush(stdout);
            //					p = p->parent;
            //				}
            //			Going up to the Root
            p = current;
            path = NULL;
            while (p != NULL)
            {
                //				cout<<"\n Am Still HERE Step["<<++m<<"] X="<<p->pose.x<<" Y="<<p->pose.y;
                //				fflush(stdout);
                // remove the parent node from the closed list (where it has to be)
                if(p->prev != NULL)
                    (p->prev)->next = p->next;
                if(p->next != NULL)
                    (p->next)->prev = p->prev;
                // check if we're removing the top of the list
                if(p == closedList->Start)
                    closedList->next();
                // set it up in the path
                p->next = path;
                path = p;
                p = p->parent;
            }
            // now delete all nodes on OPEN and Closed Lists
            openList->free();
            closedList->free();
            return path;
        }
        // Create List of Children for the current NODE
        if(!(childList = makeChildrenNodes(current))) // No more Children => Search Ended Unsuccessfully at this path Branch
        {
            std::cout<<"\n	--->>> Search Ended On this Branch / We Reached a DEAD END <<<---";
        }
        // insert the children into the OPEN list according to their f values
        ros::Time iteration_begin = ros::Time::now();

        while (childList != NULL)
        {
            ros::Time test_begin = ros::Time::now();

            curChild  = childList;
            childList = childList->next;
            // set up the rest of the child node details
            curChild->parent = current;
            curChild->depth  = current->depth + 1;
            curChild->id = ID++;
            curChild->next = NULL;
            curChild->prev = NULL;

            //************voxelgrid***********
            pcl::PointCloud<pcl::PointXYZ>::Ptr tempCloud(new pcl::PointCloud<pcl::PointXYZ>);
            pcl::PointCloud<pcl::PointXYZ> temp_cloud, collective_cloud;
            temp_cloud = obj->extractVisibleSurface(curChild->senPose.p);
            collective_cloud.points = curChild->parent->cloud_filtered->points;
//            std::cout<<"Parent cloud size: "<<collective_cloud.size()<<"\n";
//            std::cout<<"child cloud size: "<<temp_cloud.size()<<"\n";
            collective_cloud +=temp_cloud;
//            std::cout<<"collective cloud size: "<<collective_cloud.size()<<"\n";
            tempCloud->points = collective_cloud.points;
            pcl::VoxelGrid<pcl::PointXYZ> voxelgrid;
            voxelgrid.setInputCloud (tempCloud);
            voxelgrid.setLeafSize (0.5f, 0.5f, 0.5f);
            voxelgrid.filter (*curChild->cloud_filtered);
//            std::cout<<"\nchild collective cloud after filtering size: "<<curChild->cloud_filtered->size()<<"\n";
            curChild->coverage = obj->calcCoveragePercent(curChild->cloud_filtered);
            curChild->distance = curChild->parent->distance + Dist(curChild->pose.p,curChild->parent->pose.p);


            curChild->g_value = heuristic->gCost(curChild);
            curChild->h_value = heuristic->hCost(curChild);
            curChild->f_value = curChild->h_value;//curChild->g_value + curChild->h_value;
//            std::cout<<"curChildren f value= "<<curChild->f_value<<"\n";
            Node * p;
            // check if the child is already in the open list
            if( (p = openList->find(curChild)))
            {
                if (p->f_value > curChild->f_value)// it was <= (to take least cost) but now it is changed to be reward function && (p->direction == curChild->direction))
                {
                    freeNode(curChild);
                    curChild = NULL;
                }
                // the child is a shorter path to this point, delete p from  the closed list
                else if (p->f_value <= curChild->f_value )//&& (p->direction == curChild->direction))//************IMPORTANT******************
                {
                    openList->remove(p);
//                    std::cout<<"*****SELECTED child h value: "<<curChild->f_value<<"********\n";
//                    std::cout<<"parent :"<<"position x:"<<curChild->pose.p.position.x<<" y:"<<curChild->pose.p.position.y<<" z:"<<curChild->pose.p.position.z<<"\n";
//                    std::cout<<"parent :"<<"orientation x:"<<curChild->pose.p.orientation.x<<" y:"<<curChild->pose.p.orientation.y<<" z:"<<curChild->pose.p.orientation.z<<" w:"<<curChild->pose.p.orientation.w<<"\n";

                    //cout<<"\n	--->>> Opened list -- Node is deleted, current child X="<<curChild->pose.x<<" Y="<<curChild->pose.y<<" has shorter path<<<---";
                    fflush(stdout);
                }
            }
            // test whether the child is in the closed list (already been there)
            if (curChild)
            {
                if((p = closedList->find(curChild)))
                {
                    if (p->f_value > curChild->f_value)// && p->direction == curChild->direction)//************IMPORTANT******************
                    {
                        freeNode(curChild);
                        curChild = NULL;
                    }
                    // the child is a shorter path to this point, delete p from  the closed list
                    else
                    {
                        /* This is the tricky part, it rarely happens, but in my case it happenes all the time :s
                                                 * Anyways, we are here cause we found a better path to a node that we already visited, we will have to
                                                 * Update the cost of that node and ALL ITS DESCENDENTS because their cost is parent dependent ;)
                                                 * Another Solution is simply to comment everything and do nothing, doing this, the child will be added to the
                                                 * Open List and it will be investigated further later on.
                                                 */
                        //TODO : this SHOULD be fixed, very very DODGY
                        //						Node *ptr = closedList->Start;
                        //						while(ptr)
                        //						{
                        //							if(ptr->parent == p)
                        //								ptr->parent = NULL;
                        //							ptr = ptr->next;
                        //						}
                        //						closedList->Remove(p);
                        //cout<<"\n	--->>> Closed list -- Node is deleted, current child X="<<curChild->pose.x<<" Y="<<curChild->pose.y<<" has shorter path<<<---";
                        fflush(stdout);

                    }
                }
            }
            // ADD the child to the OPEN List
            if (curChild)
            {
                openList->add(curChild);
            }
            ros::Time test_end = ros::Time::now();
            double elapsed1 =  test_end.toSec() - test_begin.toSec();
            std::cout<<"****Child Test duration (s)= "<<elapsed1<<"****\n";
        }
        ros::Time iteration_end = ros::Time::now();
        double elapsed =  iteration_end.toSec() - iteration_begin.toSec();
        std::cout<<"****Children Test duration (s) of node "<<current->id<<"= "<<elapsed<<"****\n";

        // put the current node onto the closed list, ==>> already visited List
        closedList->add(current);
        // Test to see if we have expanded too many nodes without a solution
        if (current->id > this->MAXNODES)
        {
            //            LOG(Logger::Info,QString("	--->>>	Expanded %d Nodes which is more than the maximum allowed MAXNODE=%1 , Search Terminated").arg(current->id,MAXNODES))
            //Delete Nodes in Open and Closed Lists
            closedList->free();
            openList->free();
            path = NULL;
            return path; // Expanded more than the maximium nodes state
        }
    }	//...  end of OPEN loop

    /* if we got here, then there is no path to the goal
     *  delete all nodes on CLOSED since OPEN is now empty
     */
    std::cout<<"counter for displaying the tree: "<<count<<" \n";
    closedList->free();
    std::cout<<"\n	--->>>No Path Found<<<---";
    return NULL;
}

bool Astar::goalReached (Node *n)
{
    double angle_diff, delta_d;
    delta_d = Dist(n->pose.p,end.p);
    if ( delta_d <= distGoal)
        return true;
    else
        return false;
//    if (n->direction == FORWARD)
//        angle_diff =	anglediff(end.phi,n->pose.phi);
//    else
//    {
//        angle_diff =	anglediff(end.phi,n->pose.phi + M_PI);
//    }
//    if ( delta_d <= distGoal && angle_diff <= orientation2Goal)
//    {
//        //        LOG(Logger::Info," \n Desired Final Orientation ="<<RTOD(end.phi)<<" Current="<<RTOD(n->pose.phi))
//        //        LOG(Logger::Info,"\n Reached Destination with Diff Orientation="<< RTOD(angle_diff))
//        return 1;
//    }
//    return 0;
};

bool Astar::surfaceCoverageReached (Node *n)// newly added
{
    double cov_delta;
    cov_delta = targetCov - n->coverage;//n->h_value;//Dist(n->pose.p,end.p);
    std::cout<<"cov_delta= "<<cov_delta<<"\n";
    if ( cov_delta <= covTolerance)
        return true;
    else
        return false;

};


Node *Astar::makeChildrenNodes(Node *parent)
{
    geometry_msgs::Pose P, sensorP;
    Node  *p, *q;
    SearchSpaceNode *temp;
    double start_angle,end_angle,angle,angle_difference,discrete_angle,robot_angle,child_angle,angle_resolution = DTOR(10);
    bool collides = FALSE;
    int direction;
    P.position.x  = parent->pose.p.position.x;
    P.position.y  = parent->pose.p.position.y;
    P.position.z  = parent->pose.p.position.z;
    P.orientation.x  = parent->pose.p.orientation.x;
    P.orientation.y  = parent->pose.p.orientation.y;
    P.orientation.z  = parent->pose.p.orientation.z;
    P.orientation.w  = parent->pose.p.orientation.w;
    //not sure if it is necessary (Not necessary)
    sensorP.position.x = parent->senPose.p.position.x;
    sensorP.position.y = parent->senPose.p.position.y;
    sensorP.position.z = parent->senPose.p.position.z;
    sensorP.orientation.x = parent->senPose.p.orientation.x;
    sensorP.orientation.y = parent->senPose.p.orientation.y;
    sensorP.orientation.z = parent->senPose.p.orientation.z;
    sensorP.orientation.w = parent->senPose.p.orientation.w;

//    std::cout<<"parent #"<<"position x:"<<parent->pose.p.position.x<<" y:"<<parent->pose.p.position.y<<" z:"<<parent->pose.p.position.z<<"\n";
//    std::cout<<"parent #"<<"orientation x:"<<parent->pose.p.orientation.x<<" y:"<<parent->pose.p.orientation.y<<" z:"<<parent->pose.p.orientation.z<<" w:"<<parent->pose.p.orientation.w<<"\n";
    Tree t;
    t.location = P;
    if(!search_space)
        return NULL;
    temp = search_space;
    // Locate the Cell in the Search Space, necessary to determine the neighbours
    while(temp!=NULL)
    {
        //added the orientation since we have different
        if (isPositionEqual(temp->location.position,P.position) && isOrientationEqual(temp->location.orientation,P.orientation))
            break;
        temp = temp->next;
    }
    if (!temp)
    {
        //        LOG(Logger::Info,"	--->>>	Node not found in the search Space ")
        return NULL;
    }
    //    qDebug("Node has %d children x=%f y=%f",temp->children.size(),temp->location.x(),temp->location.y());
    q = NULL;
    // Check Each neighbour
    for (int i=0;i<temp->children.size();i++)
    {
        /*
         * Check what what as the Robot's direction of motion and see
         * if we it's easier to go forward or backwards to the child
         */
//        if (parent->direction == FORWARD)
//            robot_angle = parent->pose.phi;
//        else
//            robot_angle = parent->pose.phi + M_PI;
        // What will be our orientation when we go to this child node ?
        //TODO:: Transfer this into a 3D Vector angle diff
        //angle = ATAN2(temp->children[i]->location,P);
//        angle = 0;
        // How much it differs from our current orientations ?
//        angle_difference = anglediff(angle,parent->pose.phi);
        // Are we gonna turn too much ? if yes then why not go backwards ?
        /*
                if (angle_difference > DTOR(120))
                {
                        //cout<<"\n Angle difference ="<<RTOD(angle_difference)<<" parent angle="<<RTOD(parent->angle)<<" destination angle="<<RTOD(angle);
                        direction = parent->direction * -1;
                }
                else
                */
//        {
//            direction = parent->direction;
//        }
//        collides = FALSE;
        /* Discreatize the turning space and check for collison
                 * 1- Angle stored in the node is the direction of the PATH (NOT THE ROBOT)
                 * 2- If we were moving Forward then the Robot direction is the same as the Path
                 * 3- If we were moving BackWard then the Robot direction is Path + M_PI
                 * 4- Determine what will the Robot orientation will be at this step
                 * 5- Check for collision detection with a certain resolution
                 */
//        if (direction == FORWARD)
//            child_angle = angle;
//        else
//            child_angle = angle + M_PI;
//        if(robot_angle  < 0 ) robot_angle  += 2*M_PI;
//        if(child_angle  < 0 ) child_angle  += 2*M_PI;
        // Start from the largest angle and go down
//        if (robot_angle > child_angle)
//        {
//            start_angle  = robot_angle;
//            end_angle    = child_angle;
//        }
//        else
//        {
//            start_angle  = child_angle;
//            end_angle    = robot_angle;
//        }
//        discrete_angle =  start_angle;
        //cout<<"\n Start is"<<RTOD(start_angle)<<" End angle="<<RTOD(end_angle);
//        angle_difference = anglediff(start_angle,end_angle);
//        for (int s=0 ; s <= ceil(angle_difference/angle_resolution); s++)
//        {
//            if (inObstacle(temp->children[i]->location,discrete_angle))
//            {
//                collides= true;
//                break;
//            }
//            if(Abs(start_angle - end_angle) >= DTOR(180))
//            {
//                discrete_angle += angle_resolution;
//                if (discrete_angle > 2*M_PI)
//                    discrete_angle-= 2*M_PI;
//                if(discrete_angle > end_angle)
//                    discrete_angle = end_angle;
//            }
//            else
//            {
//                discrete_angle -= angle_resolution;
//                if (discrete_angle < end_angle)
//                    discrete_angle = end_angle;
//            }
//        }
        //if (!collides) // if after discretization the child still doens't collide then add it
        //{
            p = new Node;
            p->pose.p.position.x = temp->children[i]->location.position.x;
            p->pose.p.position.y = temp->children[i]->location.position.y;
            p->pose.p.position.z = temp->children[i]->location.position.z;
            p->pose.p.orientation.x = temp->children[i]->location.orientation.x;
            p->pose.p.orientation.y = temp->children[i]->location.orientation.y;
            p->pose.p.orientation.z = temp->children[i]->location.orientation.z;
            p->pose.p.orientation.w = temp->children[i]->location.orientation.w;

//            std::cout<<"child #"<<i<<"position x:"<<p->pose.p.position.x<<" y:"<<p->pose.p.position.y<<" z:"<<p->pose.p.position.z<<"\n";
//            std::cout<<"child #"<<i<<"orientation x:"<<p->pose.p.orientation.x<<" y:"<<p->pose.p.orientation.y<<" z:"<<p->pose.p.orientation.z<<" w:"<<p->pose.p.orientation.w<<"\n";

            p->senPose.p.position.x = temp->children[i]->sensorLocation.position.x;
            p->senPose.p.position.y = temp->children[i]->sensorLocation.position.y;
            p->senPose.p.position.z = temp->children[i]->sensorLocation.position.z;
            p->senPose.p.orientation.x = temp->children[i]->sensorLocation.orientation.x;
            p->senPose.p.orientation.y = temp->children[i]->sensorLocation.orientation.y;
            p->senPose.p.orientation.z = temp->children[i]->sensorLocation.orientation.z;
            p->senPose.p.orientation.w = temp->children[i]->sensorLocation.orientation.w;

            p->id = temp->children[i]->id;
//            p->direction  =	direction ;
            t.children.push_back(p->pose.p);
//            p->nearest_obstacle = temp->children[i]->obstacle_cost;
            p->parent = parent;
//            p->pose.phi = angle;
            p->next = q;
            q = p;
       // }
    }
    // Save the search tree so that it can be displayed later
    if (t.children.size() > 0)
        tree.push_back(t);
//    std::cout<<" tree size after making children nodes: "<<tree.size()<<"\n";
    return q;
}
// Free node function
void Astar::freeNode(Node *n)
{
    delete n;
}

}
