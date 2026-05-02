#pragma once
#include <iostream>
#include <vector>
#include <map>
using namespace std;

// TrafficSignal.h - Controls signal states at an intersection
// Section 4.8: Traffic Signal Model
// Only one incoming road gets green at a time

const int GREEN_DURATION = 5; // Fixed green duration (steps) 

class TrafficSignal {
public:
    int nodeId;                      // Which intersection this signal controls
    vector<int> incomingRoadIds;     // Road IDs of incoming roads at this node
    map<int, int> signalState;       // roadId -> 1 (green) or 0 (red)
    int currentGreenRoad;            // Road currently having green
    int greenTimer;                  // Steps the current green has been active
    bool adaptiveMode;               // true: queue-based, false: fixed timer

    TrafficSignal() {
        nodeId = -1; currentGreenRoad = -1;
        greenTimer = 0;
        adaptiveMode = true;
    }

    TrafficSignal(int nid, vector<int> roads, bool adaptive = true) {
        nodeId = nid;
        incomingRoadIds = roads;
        currentGreenRoad = -1;
        greenTimer = 0;
        adaptiveMode = adaptive;

        // Initialize all signals to red
        for (int r : incomingRoadIds)
            signalState[r] = 0;

        // Set first road to green if any roads exist
        if (!incomingRoadIds.empty()) {
            currentGreenRoad = incomingRoadIds[0];
            signalState[currentGreenRoad] = 1;
        }
    }

    // Section 4.8: Update signal based on queue lengths
    // gij(t) = 1 for edge with maximum Qij(t)
    // Signal constraint: only ONE incoming road is green
    void update(map<int, int>& roadQueues) {
        if (incomingRoadIds.empty()) return;

        greenTimer++;

        if (adaptiveMode) {
            // Always pick road with longest queue
            int bestRoad = -1; int maxQueue = -1;
            for (int rid : incomingRoadIds) {
                int q = 0;
                if (roadQueues.count(rid)) q = roadQueues[rid];
                if (q > maxQueue) {
                    maxQueue = q;
                    bestRoad = rid;
                }
            }
            // If a road with queue found, switch to it
            if (bestRoad != -1 && bestRoad != currentGreenRoad) {
                setGreen(bestRoad);
            }
            else if (greenTimer >= GREEN_DURATION) {
                // Rotate to next road if no queue difference after green duration
                rotateSignal();
            }
        }
        else {
            // Fixed timer mode: rotate after GREEN_DURATION steps
            if (greenTimer >= GREEN_DURATION) {
                rotateSignal();
            }
        }
    }

    // Set a specific road to green, all others to red
    void setGreen(int roadId) {
        for (int r : incomingRoadIds)
            signalState[r] = 0;
        signalState[roadId] = 1;
        currentGreenRoad = roadId;
        greenTimer = 0;
    }

    // Rotate green to next road in round-robin fashion
    void rotateSignal() {
        if (incomingRoadIds.empty()) return;
        int idx = 0;
        for (int i = 0; i < (int)incomingRoadIds.size(); i++) {
            if (incomingRoadIds[i] == currentGreenRoad) {
                idx = (i + 1) % incomingRoadIds.size();
                break;
            }
        }
        setGreen(incomingRoadIds[idx]);
    }

    // Get signal state for a specific road (1 = green, 0 = red)
    int getSignal(int roadId) const {
        auto it = signalState.find(roadId);
        if (it != signalState.end()) return it->second;
        return 0; // default red if not found
    }

    void display() const {
        cout << "  Signal at Node " << nodeId << ": ";
        for (int rid : incomingRoadIds) {
            cout << "Road" << rid << "=";
            auto it = signalState.find(rid);
            if (it != signalState.end() && it->second == 1) {
                cout << "GREEN";
            }
            else {
                cout << "RED";
            }
            cout << " ";
        }
        cout << endl;
    }

};
