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
    double remainingTravelTime;  // rv(t): steps left on current road
    double entryTravelTime;      // travel time when vehicle FIRST entered road (fixed reference)
    vector<int> path;            // Planned route (sequence of node IDs)
    int pathIndex;               // Current position in path
    int status;                  // WAITING, MOVING, or ARRIVED

    // Metrics tracking
    int stepEntered;             // Simulation step when vehicle was created
    int stepArrived;             // Simulation step when vehicle reached destination
    double totalDelay;           // Extra time beyond free-flow travel time

    // Constructor
    Vehicle() {
        id = -1; source = -1; destination = -1; currentNode = -1; currentRoad = -1;
        remainingTravelTime = 0.0; entryTravelTime = 0.0;
        pathIndex = 0; status = WAITING;
        stepEntered = 0; stepArrived = -1; totalDelay = 0.0;
    }

    Vehicle(int vid, int src, int dst, int step) {
        id = vid; source = src; destination = dst; currentNode = src; currentRoad = -1;
        remainingTravelTime = 0.0; entryTravelTime = 0.0;
        pathIndex = 0; status = WAITING; stepEntered = step; stepArrived = -1; totalDelay = 0.0;
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
        remainingTravelTime = travelTime;
        entryTravelTime = travelTime; 
        status = MOVING;
    }

    // Vehicle arrives at a node after finishing a road
    void arriveAtNode(int nodeId) {
        currentNode = nodeId;
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
        if (pathIndex + 1 < (int)path.size())
            return path[pathIndex + 1];
        return -1;
    }

    // Check if vehicle has a valid path loaded
    bool hasPath() const {
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
        cout << "  Vehicle " << id << " | " << source << "->" << destination
            << " | At node: " << currentNode
            << " | Status: " << getStatusString()
            << " | RemainingTime: " << remainingTravelTime << endl;
    }
};