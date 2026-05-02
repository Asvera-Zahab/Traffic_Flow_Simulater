#pragma once
#include <iostream>
#include <string>
#include <cmath>   //for pow
using namespace std;

// BFT (Bureau of Public Roads) model constants
const double ALPHA = 0.15;   // Congestion sensitivity
const double BETA = 4.0;    // Nonlinearity factor

class Road {
public:
    int id;            // Unique road ID
    int source;        // Source intersection (node)
    int destination;   // Destination intersection (node)

    //Physical properties
    double length;     // lij: length of road in km
    double maxSpeed;   // vij_max: max speed in km/h
    int capacity;      // cij: max vehicles on road at once
    double dischargeRate; // muij: max vehicles that can leave per step

    //Dynamic state (updated each)
    int currentFlow;   // fij(t): vehicles currently on road
    int queueCount;    // Qij(t): vehicles waiting at destination intersection
    double congestion; // rho = fij / cij (0 to 1+)
    double travelTime; // wij(t): current travel time (in steps)
    double freeTravelTimeInSteps; // wij_free = length / maxSpeed 

    Road() {
        id = -1; source = -1; destination = -1; length = 1.0; maxSpeed = 60.0; capacity = 10;
        dischargeRate = 3.0; currentFlow = 0; queueCount = 0; congestion = 0.0; travelTime = 0.0; freeTravelTimeInSteps = 0.0;
    }

    Road(int roadId, int src, int dst, double len, double speed, int cap, double discharge = 3.0) {
        id = roadId; source = src; destination = dst; length = len; maxSpeed = speed;  capacity = cap;
        dischargeRate = discharge; currentFlow = 0; queueCount = 0; congestion = 0.0;

        // Section 4.4: Free flow travel time wij_free = lij / vij_max
        freeTravelTimeInSteps = length / maxSpeed * 60.0; // convert to minutes(steps)
        if (freeTravelTimeInSteps < 1.0) freeTravelTimeInSteps = 1.0; // minimum 1 step = 1 one unit of time
        travelTime = freeTravelTimeInSteps;
    }

    // Section 4.3: Congestion Model
    // rho = fij(t) / cij
    // rho = 0: free road, rho = 1: congested road
    void updateCongestion() {
        if (capacity > 0) congestion = (double)currentFlow / (double)capacity;
        else congestion = 0.0;
    }

    // Section 4.4: Congested Travel Time (BPR Formula)
    // wij(t) = wij_free * (1 + alpha * (fij/cij)^beta)
    void updateTravelTime() {
        updateCongestion();
        double ratio = congestion;  // reuse it
        travelTime = freeTravelTimeInSteps * (1.0 + ALPHA * pow(ratio, BETA));
        if (travelTime < 1.0) travelTime = 1.0;
    }

    // Section 4.2: Traffic Flow Update
    // fij(t+1) = fij(t) + aij(t) - xij(t)
    // arrivals: vehicles entering this road
    // departures: vehicles leaving this road into queue
    void updateFlow(int arrivals, int departures) {
        currentFlow = currentFlow + arrivals - departures;
        if (currentFlow < 0) currentFlow = 0;
    }

    // Section 4.2: Queue Release Formula
    // dij(t) = gij(t) * min(Qij(t), muij, nextCapacity)
    // signalGreen: 1 if green, 0 if red
    // nextAvailableCapacity: cjk - fjk(t)
    int computeRelease(int signalGreen, int nextAvailableCapacity) {
        if (signalGreen == 0) return 0;
        int release = min(queueCount, min((int)dischargeRate, nextAvailableCapacity));
        if (release < 0) release = 0;
        return release;
    }

    // Display road state
    void display() const {
        cout << "  Road " << source << "->" << destination << " | Flow: " << currentFlow << "/" << capacity
            << " | Queue: " << queueCount << " | Congestion: " << congestion << " | TravelTime: " << travelTime << endl;
    }
};
