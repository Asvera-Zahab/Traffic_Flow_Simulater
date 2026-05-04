#pragma once
#include <iostream>
#include <vector>
#include <map>
using namespace std;

const int GREEN_DURATION = 5;

class TrafficSignal {
public:
    int nodeId;
    vector<int> incomingRoadIds;
    map<int, int> signalState;  //road 10  GREEN
    int currentGreenRoad;
    int greenTimer;
    bool adaptiveMode;  //signal reacts to traffic

    TrafficSignal()
        :nodeId(-1), currentGreenRoad(-1), greenTimer(0), adaptiveMode(true) {
    }

    TrafficSignal(int nid, const vector<int>& roads, bool adaptive = true):nodeId(nid), incomingRoadIds(roads),
        currentGreenRoad(-1), greenTimer(0), adaptiveMode(adaptive) {

        for (int r : incomingRoadIds) signalState[r] = 0;
        //initialize all red,  id 10  RED(0)

        if (!incomingRoadIds.empty()) {
            //make first road green
            currentGreenRoad = incomingRoadIds[0];
            signalState[currentGreenRoad] = 1;
        }
    }

    void update(const map<int, int>& roadQueues) {
        //roadQueues = number of vehicles waiting on each road
        //Skip if no roads
        if (incomingRoadIds.empty()) return;

        //timer count increase
        greenTimer++;

        if (adaptiveMode) {
            //goal: Give GREEN to the road with highest traffic.
            int bestRoad = currentGreenRoad;
            int maxQueue = -1;

            for (int rid : incomingRoadIds) {
                //check each roaf
                auto it = roadQueues.find(rid);
                //key  rid
                //get queue size
                int q = 0;
                if (it != roadQueues.end()) q = it->second;
                //it->second is size

                if (q > maxQueue) {
                    maxQueue = q;
                    bestRoad = rid;
                }
            }

            if (bestRoad != currentGreenRoad) {
                setGreen(bestRoad);
            }
            //If the current green light has stayed on long enough, switch to the next road.
            else if (greenTimer >= GREEN_DURATION) {
                rotateSignal();
            }
        }
        else {
            if (greenTimer >= GREEN_DURATION) {
                rotateSignal();
            }
        }
    }

    void setGreen(int roadId) {
        if (roadId == currentGreenRoad) return;

        for (int r : incomingRoadIds)
            signalState[r] = 0;

        signalState[roadId] = 1;
        currentGreenRoad = roadId;
        greenTimer = 0;
    }

    void rotateSignal() {
        //If no roads exist nothing to rotate
        if (incomingRoadIds.empty()) return;

        int n = incomingRoadIds.size();
        for (int i = 0; i < n; i++) {
            if (incomingRoadIds[i] == currentGreenRoad) {
                setGreen(incomingRoadIds[(i + 1) % n]);
                //circular increment
                return;
            }
        }
    }

    int getSignal(int roadId) const {
        auto it = signalState.find(roadId);
        if (it != signalState.end()) {
            return it->second;
        }
        else {
            return 0;
        }
    }

    void display() const {
        cout << "[BY TRAFFICSIGNAL]  Signal at Node " << nodeId << ": ";
        for (int rid : incomingRoadIds) {
            cout << "Road" << rid << "=";
            if (getSignal(rid)) {
                cout << "GREEN ";
            }
            else {
                cout << "RED ";
            }
        }
        cout << endl;
    }
};