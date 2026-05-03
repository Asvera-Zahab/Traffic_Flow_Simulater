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
    map<int, int> signalState;
    int currentGreenRoad;
    int greenTimer;
    bool adaptiveMode;

    TrafficSignal()
        :nodeId(-1), currentGreenRoad(-1), greenTimer(0), adaptiveMode(true) {
    }

    TrafficSignal(int nid, const vector<int>& roads, bool adaptive = true):nodeId(nid), incomingRoadIds(roads),
        currentGreenRoad(-1), greenTimer(0), adaptiveMode(adaptive) {

        for (int r : incomingRoadIds) signalState[r] = 0;

        if (!incomingRoadIds.empty()) {
            currentGreenRoad = incomingRoadIds[0];
            signalState[currentGreenRoad] = 1;
        }
    }

    void update(const map<int, int>& roadQueues) {
        if (incomingRoadIds.empty()) return;

        greenTimer++;

        if (adaptiveMode) {
            int bestRoad = currentGreenRoad;
            int maxQueue = -1;

            for (int rid : incomingRoadIds) {
                auto it = roadQueues.find(rid);
                int q = 0;
                if (it != roadQueues.end()) q = it->second;

                if (q > maxQueue) {
                    maxQueue = q;
                    bestRoad = rid;
                }
            }

            if (bestRoad != currentGreenRoad) {
                setGreen(bestRoad);
            }
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
        if (incomingRoadIds.empty()) return;

        int n = incomingRoadIds.size();
        for (int i = 0; i < n; i++) {
            if (incomingRoadIds[i] == currentGreenRoad) {
                setGreen(incomingRoadIds[(i + 1) % n]);
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