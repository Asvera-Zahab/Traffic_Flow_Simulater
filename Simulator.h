#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include "Graph.h"
#include "Vehicle.h"
#include "TrafficSignal.h"
#include "TrafficFormula.h"
#include "FileManager.h"
#include "Utility.h"

using namespace std;


const int NO_EVENT = 0;
const int ROAD_BLOCK = 1;
const int ROAD_CLEAR = 2;
const int PEAK_TRAFFIC = 3;

struct SimEvent {
    int step = 0;
    int type = 0;
    int roadId = -1;
    string description = "";
};

class Simulator {
public:
    Graph graph;  //intersections
    vector<Vehicle> vehicles;  //src,destination,path,state
    map<int, TrafficSignal> signals; //signal incoming road
    vector<SimEvent> events; //predefined

    int currentStep;
    int totalSteps;
    int nextVehicleId;
    bool peakMode;

    vector<int> completedTravelTimes;
    vector<double> completedFreeTimes;
    vector<double> stepAvgCongestion;

    int totalCompleted;
    int totalGenerated;

    int mostCongestedRoadTracked;
    double maxCongTracked;
    int mostBusyNodeTracked;
    int maxFlowTracked;

    Simulator() {
        currentStep = 0;
        totalSteps = 50;
        nextVehicleId = 1;
        peakMode = false;
        totalCompleted = 0;
        totalGenerated = 0;
        mostCongestedRoadTracked = 0;
        maxCongTracked = -1.0;
        mostBusyNodeTracked = 0;
        maxFlowTracked = -1;
    }

    void buildCityGraph() {
        Utility::printHeader("BUILDING CITY ROAD NETWORK");

        graph.addVertex(0, "Karachi");
        graph.addVertex(1, "Islamabad");
        graph.addVertex(2, "Lahore");
        graph.addVertex(3, "Murree");
        graph.addVertex(4, "Kashmir");

        // addEdge(src, dst, length_km, maxSpeed_kmh, capacity, dischargeRate)
        graph.addEdge(0, 1, 2.0, 80.0, 12, 4.0);  // Main highway
        graph.addEdge(0, 2, 1.5, 50.0, 8, 3.0);   // Urban road
        graph.addEdge(1, 2, 1.0, 60.0, 6, 2.0);   // Connector road
        graph.addEdge(1, 3, 3.0, 90.0, 14, 5.0);  // Expressway
        graph.addEdge(2, 3, 1.2, 40.0, 5, 2.0);   // Narrow road
        graph.addEdge(3, 4, 2.5, 70.0, 10, 3.0);  // Final stretch
        graph.addEdge(2, 4, 2.0, 55.0, 7, 3.0);   // Bypass road

        cout << "City graph created with 5 nodes and 7 roads." << endl;
        graph.displayGraph();
    }

    void setupSignals() {
        cout << "\n[Simulator] Setting up traffic signals..." << endl;
        for (auto& kv : graph.nodes) {
            int nid = kv.first;
            vector<int>& inRoads = kv.second.incomingRoads;
            //no incoming road no signal
            if (!inRoads.empty())
                signals[nid] = TrafficSignal(nid, inRoads, true);
        }
        cout << "[Simulator] Signals initialized at " << signals.size() << " intersections." << endl;
    }

    void scheduleEvents() {
        SimEvent block;
        block.step = 15;
        block.type = ROAD_BLOCK;
        block.roadId = 1;
        block.description = "Road 0->2 BLOCKED (accident)";
        events.push_back(block);

        SimEvent clear;
        clear.step = 25;
        clear.type = ROAD_CLEAR;
        clear.roadId = 1;
        clear.description = "Road 0->2 CLEARED";
        events.push_back(clear);

        SimEvent peak;
        peak.step = 20;
        peak.type = PEAK_TRAFFIC;
        peak.roadId = -1;
        peak.description = "PEAK TRAFFIC MODE activated";
        events.push_back(peak);
    }

    void processEvents() {
        for (SimEvent& e : events) {
            if (e.step != currentStep) continue;
            cout << "  [EVENT] " << e.description << endl;
            //if road is ROAD_Block and roadid valid, set road capacity 0
            if (e.type == ROAD_BLOCK && e.roadId >= 0 && e.roadId < (int)graph.roads.size())
                graph.roads[e.roadId].capacity = 0;
            //Restore capacity
            if (e.type == ROAD_CLEAR && e.roadId >= 0 && e.roadId < (int)graph.roads.size())
                graph.roads[e.roadId].capacity = 8;
            if (e.type == PEAK_TRAFFIC)
                peakMode = true;
        }
    }

    void generateVehicles() {
        //possible starting and destination
        vector<int> sources = { 0, 1, 2 };
        vector<int> dests = { 2, 3, 4 };

        //attempts to generate vehicles
        int spawnCount, threshold;
        //3 attempts, 80%
        if (peakMode) { spawnCount = 5; threshold = 60; }
        //normal=1, 40%
        else { spawnCount = 3; threshold = 50; }

        for (int i = 0; i < spawnCount; i++) {
            //run spawn attempts
            if (Utility::randomInt(1, 150) > threshold) continue;
            //stop creating after 100 vehicles
            if (totalGenerated >= 100) break;

            //random source and destination
            int src = sources[Utility::randomInt(0, (int)sources.size() - 1)];
            int dst = dests[Utility::randomInt(0, (int)dests.size() - 1)];
            if (src == 2 && dst == 4) continue;
            if (src == dst) continue;

            //vehicle
            Vehicle v(nextVehicleId++, src, dst, currentStep);
            vector<int> path = graph.shortestPathDijkstra(src, dst);
            if (path.empty()) continue;

            v.path = path;
            v.currentNode = src;
            v.status = WAITING;
            vehicles.push_back(v);
            totalGenerated++;

            cout << "  [+] Vehicle " << v.id << " spawned: " << src << "->" << dst << " | Path: ";
            for (int n : path) cout << n << " ";
            cout << endl;
        }
    }

    // Section 4.6: rv(t+1) = rv(t) - 1
    // FIX: a vehicle that finishes a road but has NOT reached its final
    // destination must join the queue Q_ij on the road it just left
    // (queueCount++) so that releaseFromQueues()/signals can gate it on
    // the next step, per Q_ij(t+1) = Q_ij(t) + x_ij(t) - d_ij(t).
    // Previously queueCount was only ever decremented and never
    // incremented anywhere, so it stayed at 0 forever and the whole
    // signal-gated release path was dead code.
    map<int, int> moveVehicles() {
        map<int, int> roadDepartures;

        for (Vehicle& v : vehicles) {
            if (v.status == ARRIVED) continue;
            if (v.status == MOVING) {
                bool finished = v.update();
                if (finished) {
                    int rid = v.currentRoad;
                    roadDepartures[rid]++;

                    int destNode = graph.roads[rid].destination;

                    if (destNode != v.destination) {
                        // Vehicle reached an intermediate intersection:
                        // it now waits in that road's queue Q_ij(t),
                        // not "for free" on the next road.
                        graph.roads[rid].queueCount++;
                    }

                    v.arriveAtNode(destNode);

                    if (destNode == v.destination) {
                        v.markArrived(currentStep);   // only once
                        totalCompleted++;              // only once

                        int tt = v.getTravelTime();
                        completedTravelTimes.push_back(tt);

                        // Free travel time (no congestion, no waiting)
                        double freeTime = 0.0;
                        for (int i = 0; i + 1 < (int)v.path.size(); i++) {
                            int roadId = graph.findRoadIndex(v.path[i], v.path[i + 1]);
                            if (roadId >= 0)
                                freeTime += graph.roads[roadId].travelTime;
                        }
                        completedFreeTimes.push_back(freeTime);
                    }
                }
            }
        }
        return roadDepartures;
    }

    // Section 4.8: Green signal to road with max queue
    void updateSignals() {
        map<int, int> roadQueues;
        for (Road& r : graph.roads)
            roadQueues[r.id] = r.queueCount;
        for (auto& kv : signals)
            kv.second.update(roadQueues);
    }

    // Section 4.2: dij(t) = gij(t) * min(Qij, muij, cjk - fjk)
    // Releases vehicles that are queued at a node's incoming road and
    // moves them onto the next road in their path if the signal is green.
    // FIX: now that queueCount is actually populated (see moveVehicles),
    // this function is no longer dead code, and r.currentFlow++ / 
    // r.queueCount-- correctly balance the flow/queue equations.
    void releaseFromQueues() {
        for (Road& r : graph.roads) {
            if (r.queueCount <= 0) continue;

            int destNode = r.destination;
            int sig = 1;
            if (signals.count(destNode))
                sig = signals[destNode].getSignal(r.id);
            if (sig == 0) continue;

            int released = 0;
            int maxRelease = (int)r.dischargeRate;

            for (Vehicle& v : vehicles) {
                if (released >= maxRelease) break;
                if (v.status != WAITING) continue;
                if (v.currentNode != destNode) continue;
                // Only vehicles that actually arrived via a road (i.e.
                // are sitting in THIS road's queue) get released here.
                // Vehicles still at their original source node (never
                // queued) are handled by dispatchWaitingVehicles().
                if (v.pathIndex == 0 && v.currentNode == v.source) continue;
                if (!v.hasPath()) continue;

                int nextNode = v.getNextNode();
                if (nextNode < 0) continue;

                int nextRoadId = graph.findRoadIndex(destNode, nextNode);
                if (nextRoadId < 0) continue;

                Road& nextRoad = graph.roads[nextRoadId];
                int nextAvailCap = nextRoad.capacity - nextRoad.currentFlow;
                if (nextAvailCap <= 0) continue;

                r.queueCount--;
                nextRoad.currentFlow++;
                v.enterRoad(nextRoadId, nextRoad.travelTime);
                released++;
            }
        }
    }

    // Section 4.2: fij(t+1) = fij(t) + aij(t) - xij(t)
    // Section 4.3: rho = fij / cij
    // Section 4.4: wij = wfree * (1 + alpha*(f/c)^beta)
    void updateRoadStates(map<int, int>& roadDepartures) {

        for (Road& r : graph.roads) {

            // Departures leaving the road (a_ij handled separately at the
            // point of entry in dispatchWaitingVehicles()/releaseFromQueues())
            if (roadDepartures.count(r.id))
                r.currentFlow -= roadDepartures[r.id];

            // safety clamp
            if (r.currentFlow < 0)
                r.currentFlow = 0;

            // recompute properly
            r.updateCongestion();
            r.updateTravelTime();
        }

        // tracking remains same
        for (Road& r : graph.roads) {
            if (r.congestion > maxCongTracked) {
                maxCongTracked = r.congestion;
                mostCongestedRoadTracked = r.id;
            }
        }
    }

    void rerouteWaitingVehicles() {
        for (Vehicle& v : vehicles) {
            if (v.status != WAITING) continue;
            if (v.currentNode == v.destination) continue;
            bool needsReroute = !v.hasPath();
            if (!needsReroute && v.hasPath()) {
                int nextNode = v.getNextNode();
                if (nextNode >= 0) {
                    int rid = graph.findRoadIndex(v.currentNode, nextNode);
                    if (rid >= 0 && graph.roads[rid].capacity == 0)
                        needsReroute = true;
                }
            }
            if (needsReroute) {
                vector<int> newPath = graph.shortestPathDijkstra(v.currentNode, v.destination);
                if (!newPath.empty()) {
                    v.path = newPath;
                    v.pathIndex = 0;
                }
            }
        }
    }

    // FIX: this now only dispatches vehicles that are still sitting at
    // their ORIGINAL source node and have never been queued on a road
    // (pathIndex == 0 && currentNode == source). Every other WAITING
    // vehicle (i.e. one that has already traversed at least one road)
    // is queued on the road it arrived via and must go through the
    // signal-gated releaseFromQueues() instead. This prevents a vehicle
    // from being queued and immediately un-queued bypassing the signal
    // in the same step, and restores meaning to r.capacity checks.
    // FIX: r.currentFlow is now actually incremented when a vehicle
    // enters a road here -- previously it was only ever decremented in
    // updateRoadStates(), so congestion (f/c) could never rise above 0.
    void dispatchWaitingVehicles() {
        for (Vehicle& v : vehicles) {

            if (v.status != WAITING) continue;
            if (v.currentNode == v.destination) continue;
            if (!v.hasPath()) continue;

            // Only vehicles still at their original source, never queued.
            if (!(v.pathIndex == 0 && v.currentNode == v.source)) continue;

            int nextNode = v.getNextNode();
            if (nextNode < 0) continue;

            int roadId = graph.findRoadIndex(v.currentNode, nextNode);
            if (roadId < 0) continue;

            Road& r = graph.roads[roadId];

            if (r.currentFlow >= r.capacity) continue;

            r.currentFlow++;
            v.enterRoad(roadId, r.travelTime);
        }
    }

    // Section 4.10: Performance metrics per step
    void recordAndPrintMetrics() {
        int moving = 0, waiting = 0, arrived = 0;
        double sumCong = 0.0;

        for (Vehicle& v : vehicles) {
            if (v.status == MOVING) moving++;
            else if (v.status == WAITING && v.currentNode != v.destination) waiting++;
            else if (v.status == ARRIVED) arrived++;
        }

        for (Road& r : graph.roads)
            sumCong += r.congestion;

        double avgCong = graph.roads.size() > 0 ? sumCong / graph.roads.size() : 0.0;
        double avgTT = TrafficFormula::averageTravelTime(completedTravelTimes);
        stepAvgCongestion.push_back(avgCong);

        cout << "  Vehicles Moving  : " << moving << endl;
        cout << "  Vehicles Waiting : " << waiting << endl;
        cout << "  Completed        : " << arrived << endl;
        cout << "  Avg Congestion   : " << Utility::formatDouble(avgCong) << endl;
        cout << "  Avg Travel Time  : " << Utility::formatDouble(avgTT) << " steps" << endl;

        FileManager::appendTrafficLog(currentStep, moving, waiting, arrived, avgCong, avgTT);
        if (currentStep % 10 == 0)
            FileManager::saveRoadsTxt(currentStep, graph.roads);
    }

    // Section 4.10: Final simulation report
    void printFinalReport() {
        Utility::printHeader("FINAL SIMULATION REPORT");

        double avgTT = TrafficFormula::averageTravelTime(completedTravelTimes);
        double delay = TrafficFormula::totalDelay(completedTravelTimes, completedFreeTimes);
        double throughput;
        if (totalSteps > 0) {
            throughput = (double)totalCompleted / totalSteps;
        }
        else {
            throughput = 0.0;
        }
        double avgCong = TrafficFormula::averageCongestion(stepAvgCongestion);

        int mostCongestedRoad = mostCongestedRoadTracked;
        int mostBusyNode = mostBusyNodeTracked;

        int stillWaiting = totalGenerated - totalCompleted;

        cout << "Total Simulation Steps : " << totalSteps << endl;
        cout << "Total Vehicles         : " << totalGenerated << endl;
        cout << "Vehicles Completed     : " << totalCompleted << endl;
        cout << "Vehicles Still Waiting : " << stillWaiting << endl;
        cout << "Average Travel Time    : " << Utility::formatDouble(avgTT) << " steps" << endl;
        cout << "Total Delay            : " << Utility::formatDouble(delay) << " steps" << endl;
        cout << "Throughput             : " << Utility::formatDouble(throughput) << " vehicles/step" << endl;
        cout << "Average Congestion     : " << Utility::formatDouble(avgCong) << endl;
        cout << "Most Congested Road ID : " << mostCongestedRoad << endl;
        cout << "Most Busy Node ID      : " << mostBusyNode << endl;

        FileManager::saveTrafficState(currentStep, graph.roads);
        FileManager::saveVehicleData(vehicles);
        FileManager::saveRoadData(graph.roads);
        FileManager::saveVehiclesTxt(vehicles);
        FileManager::exportReport(totalSteps, totalCompleted, stillWaiting,
            avgTT, delay, throughput, avgCong, mostCongestedRoad, mostBusyNode);

        cout << "\nFinal Signal States:" << endl;
        for (auto& kv : signals) kv.second.display();
    }

    void run(int steps = 50) {
        totalSteps = steps;
        FileManager::clearLogFile("traffic_log.txt");
        FileManager::clearLogFile("roads.txt");

        Utility::printHeader("TRAFFIC FLOW OPTIMIZATION SIMULATION");
        cout << "Running " << totalSteps << " simulation steps..." << endl;
        for (currentStep = 1; currentStep <= totalSteps; currentStep++) {
            Utility::printStepHeader(currentStep);
            processEvents();
            generateVehicles();

            map<int, int> departures = moveVehicles();
            updateRoadStates(departures);
            updateSignals();
            releaseFromQueues();
            rerouteWaitingVehicles();
            dispatchWaitingVehicles();
            recordAndPrintMetrics();
        }

        printFinalReport();
    }
};