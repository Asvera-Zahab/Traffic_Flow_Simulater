#pragma once
#include <iostream>
#include <vector>
#include <string>

using namespace std;
// Vehicle.h - Represents a single vehicle in the simulation
// Section 4.6: Vehicle Model

const int WAITING = 0; const int MOVING = 1; const int ARRIVED = 2;

class Vehicle {
public:
    int id;                      // Unique vehicle ID
    int source;                  // sv: origin intersection
    int destination;             // dv: target intersection
    int currentNode;             // Current intersection vehicle is at
    int currentRoad;             // Road ID the vehicle is currently on (-1 if at node)
    int lastRoadId;              // Road ID last traveled, kept even after arriving
    // at a node and going back to WAITING. Needed so
    // rendering/queueing logic can always find "which
    // road is this vehicle queued on" even after a
    // reroute resets path/pathIndex.
    double remainingTravelTime;  // rv(t): steps left on current road
    double entryTravelTime;      // travel time when vehicle FIRST entered road
    vector<int> path;            // Planned route (sequence of node IDs)
    int pathIndex;               // Current position in path
    int status;                  // WAITING, MOVING, or ARRIVED
    bool clearedStopLine;        // true if the car had already crossed the stop line on GREEN
    // when the light flipped red -- it is allowed to clear the junction

// Metrics tracking
    int stepEntered;             // Simulation step when vehicle was created
    int stepArrived;             // Simulation step when vehicle reached destination
    double totalDelay;           // Extra time beyond free-flow travel time

    // Constructor
    Vehicle() {
        id = -1; source = -1; destination = -1; currentNode = -1; currentRoad = -1; lastRoadId = -1;
        remainingTravelTime = 0.0; entryTravelTime = 0.0;
        pathIndex = 0; status = WAITING; clearedStopLine = false;
        stepEntered = 0; stepArrived = -1; totalDelay = 0.0;
    }

    Vehicle(int vid, int src, int dst, int step) {
        id = vid; source = src; destination = dst; currentNode = src; currentRoad = -1; lastRoadId = -1;
        remainingTravelTime = 0.0; entryTravelTime = 0.0;
        pathIndex = 0; status = WAITING; clearedStopLine = false; stepEntered = step; stepArrived = -1; totalDelay = 0.0;
    }

    // Section 4.6: Vehicle Model Update remaining travel time
    // rv(t+1) = rv(t) - 1
    // Returns true if vehicle has finished current road
    bool update() {
        if (status == MOVING) {
            remainingTravelTime -= 1.0;
            if (remainingTravelTime <= 0.0) {
                remainingTravelTime = 0.0;
                return true; // Reached end of current road
            }
        }
        return false;
    }

    // Set vehicle onto a new road
    void enterRoad(int roadId, double travelTime) {
        currentRoad = roadId;
        lastRoadId = roadId;
        remainingTravelTime = travelTime;
        entryTravelTime = travelTime;
        clearedStopLine = false;
        status = MOVING;
    }

    // Vehicle arrives at a node after finishing a road.
    // roadId is the road it just finished (kept in lastRoadId even though
    // currentRoad resets to -1), so anything downstream -- rendering, or
    // "which road is this vehicle queued on" logic -- can still find it
    // even after a later reroute resets path/pathIndex to 0.
    void arriveAtNode(int nodeId, int roadId = -1) {
        currentNode = nodeId;
        if (roadId >= 0) lastRoadId = roadId;
        currentRoad = -1;
        pathIndex++;
        status = WAITING;
    }

    // Mark vehicle as arrived at destination
    void markArrived(int step) {
        status = ARRIVED;
        stepArrived = step;
        currentNode = destination;
        currentRoad = -1;
    }

    // Get next node in planned path
    int getNextNode() const {
        // Check if there is a next node in the path
        if (pathIndex + 1 < (int)path.size())
            return path[pathIndex + 1];
        return -1;
    }

    // Check if vehicle has a valid path loaded
    bool hasPath() const {
        //if path not empty
        //(int)path.size() - 1 index of the last valid node
        return !path.empty() && pathIndex < (int)path.size() - 1;
    }

    // Get total travel time (for completed vehicles)
    int getTravelTime() const {
        if (stepArrived >= 0)
            return stepArrived - stepEntered;
        return -1;
    }

    // Get status string
    string getStatusString() const {
        if (status == WAITING)  return "Waiting";
        if (status == MOVING)   return "Moving";
        if (status == ARRIVED)  return "Arrived";
        return "Unknown";
    }

    // Display vehicle info
    void display() const {
        cout << "[BY VEHICLE]  Vehicle " << id << " | " << source << "->" << destination
            << " | At node: " << currentNode
            << " | Status: " << getStatusString()
            << " | RemainingTime: " << remainingTravelTime << endl;
    }
};