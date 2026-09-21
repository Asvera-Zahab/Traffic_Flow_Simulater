#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cmath>
#include <algorithm>
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

    // ---- GUI-facing entry points ----

    // Call ONCE before the render loop starts.
    void initialize(int steps = 300) {
        cout << "\n############################################\n"
            << "  SIMULATOR BUILD MARKER: stopline-v1\n"
            << "  If you do not see this exact line in your\n"
            << "  console output, your project is NOT using\n"
            << "  this Simulator.h -- stop and fix that first.\n"
            << "############################################\n" << endl;

        totalSteps = steps;
        buildCityGraph();
        setupSignals();
        scheduleEvents();
        FileManager::clearLogFile("traffic_log.txt");
        FileManager::clearLogFile("roads.txt");
        currentStep = 0;
    }

    // Wipes vehicle/flow state and starts over on the SAME graph, so the
    // GUI can loop forever instead of freezing once totalSteps is hit.
    void reset() {
        vehicles.clear();
        events.clear();
        completedTravelTimes.clear();
        completedFreeTimes.clear();
        stepAvgCongestion.clear();
        nextVehicleId = 1;
        peakMode = false;
        totalCompleted = 0;
        totalGenerated = 0;
        mostCongestedRoadTracked = 0;
        maxCongTracked = -1.0;
        mostBusyNodeTracked = 0;
        maxFlowTracked = -1;
        currentStep = 0;

        for (Road& r : graph.roads) {
            r.currentFlow = 0;
            r.queueCount = 0;
            r.congestion = 0.0;
            r.travelTime = r.freeTravelTimeInSteps;
            if (r.capacity == 0) r.capacity = 8; // undo any lingering block from last run
        }
        for (auto& kv : signals) {
            TrafficSignal fresh(kv.first, kv.second.incomingRoadIds, true);
            kv.second = fresh;
        }
        scheduleEvents();
    }

    // Advances the simulation by exactly ONE step. This is the body of the
    // old run() loop, pulled out so a render loop can call it on its own
    // clock instead of blocking for N steps. Auto-resets and loops once
    // totalSteps is reached, per the GUI's expectations.
    void stepOnce() {
        if (currentStep >= totalSteps) {
            printFinalReport();
            reset();
        }

        currentStep++;
        processEvents();
        generateVehicles();

        map<int, int> departures = moveVehicles();
        updateRoadStates(departures);
        // ORDER MATTERS: cars are moved AND released using the signal state
        // that was on screen during the step that just finished. Only then do
        // the lights advance to their next state. This is what guarantees a
        // car is never let through a light that was red while it was driving up to it.
        releaseFromQueues();
        updateSignals();
        rerouteWaitingVehicles();
        dispatchWaitingVehicles();
        recordAndPrintMetrics();
    }

    // Console-mode convenience: blocks and runs `steps` steps in one call,
    // exactly like the original version. Not used by the SFML GUI loop.
    void run(int steps = 50) {
        initialize(steps);
        for (int i = 0; i < totalSteps; i++) stepOnce();
        printFinalReport();
    }

    // ---- per-step mechanics ----

    void processEvents() {
        for (SimEvent& e : events) {
            if (e.step != currentStep) continue;
            cout << "  [EVENT] " << e.description << endl;
            if (e.type == ROAD_BLOCK && e.roadId >= 0 && e.roadId < (int)graph.roads.size())
                graph.roads[e.roadId].capacity = 0;
            if (e.type == ROAD_CLEAR && e.roadId >= 0 && e.roadId < (int)graph.roads.size())
                graph.roads[e.roadId].capacity = 8;
            if (e.type == PEAK_TRAFFIC)
                peakMode = true;
        }
    }

    void generateVehicles() {
        vector<int> sources = { 0, 1, 2 };
        vector<int> dests = { 2, 3, 4 };

        int spawnCount, threshold;
        if (peakMode) { spawnCount = 5; threshold = 60; }
        else { spawnCount = 3; threshold = 50; }

        // Cap on cars IN THE NETWORK at once (not a lifetime total). The old
        // "stop after 100 generated" cap was hit around step ~90, after which
        // no new cars appeared and the map sat empty until the step-300 reset.
        static const int MAX_ACTIVE_VEHICLES = 60;
        int active = 0;
        for (const Vehicle& v : vehicles)
            if (v.status != ARRIVED) active++;

        for (int i = 0; i < spawnCount; i++) {
            if (Utility::randomInt(1, 150) > threshold) continue;
            if (active >= MAX_ACTIVE_VEHICLES) break;

            int src = sources[Utility::randomInt(0, (int)sources.size() - 1)];
            int dst = dests[Utility::randomInt(0, (int)dests.size() - 1)];
            if (src == 2 && dst == 4) continue;
            if (src == dst) continue;

            Vehicle v(nextVehicleId++, src, dst, currentStep);
            vector<int> path = graph.shortestPathDijkstra(src, dst);
            if (path.empty()) continue;

            v.path = path;
            v.currentNode = src;
            v.status = WAITING;
            vehicles.push_back(v);
            totalGenerated++;
            active++;
        }
    }

    // ---- stop-line helpers ----

    // True if the signal that guards the END of this road is RED right now.
    bool isRed(int roadId) const {
        if (roadId < 0 || roadId >= (int)graph.roads.size()) return false;
        auto it = signals.find(graph.roads[roadId].destination);
        if (it == signals.end()) return false;   // no signal -> never red
        return it->second.getSignal(roadId) == 0;
    }

    // remainingTravelTime a vehicle has left when it is exactly ON the stop line.
    double stopLineRemaining(const Vehicle& v) const {
        return (1.0 - STOP_LINE_PROGRESS) * v.entryTravelTime;
    }

    // A MOVING vehicle that is sitting on the stop line of a red road.
    bool isHeldAtLine(const Vehicle& v) const {
        if (v.status != MOVING || !isRed(v.currentRoad)) return false;
        return std::fabs(v.remainingTravelTime - stopLineRemaining(v)) <= STOP_LINE_EPS;
    }

    // Section 4.6: rv(t+1) = rv(t) - 1
    // STOP LINE: while the light at the end of a road is RED, a vehicle may drive
    // up to the stop line but NOT past it (its remaining time is frozen there).
    // The moment the light is green it continues as normal.
    // FIX (kept from earlier review): a vehicle that finishes a road but
    // has NOT reached its final destination joins the queue Q_ij on the
    // road it just left (queueCount++), so releaseFromQueues()/signals can
    // gate it on the next step. Also passes the finished road's id into
    // arriveAtNode() so Vehicle::lastRoadId stays correct for rendering,
    // even through later reroutes.
    map<int, int> moveVehicles() {
        map<int, int> roadDepartures;

        for (Vehicle& v : vehicles) {
            if (v.status != MOVING) continue;

            int rid = v.currentRoad;
            bool red = isRed(rid);

            // RED light: drive up to the stop line, never through it.
            if (red) {
                double stopRem = stopLineRemaining(v);
                if (v.remainingTravelTime >= stopRem - STOP_LINE_EPS) {
                    v.remainingTravelTime = max(stopRem, v.remainingTravelTime - 1.0);
                    continue;
                }
                // else: the car had already crossed the line while it was
                // still green -- it is allowed to finish crossing the junction.
            }

            bool finished = v.update();
            if (!finished) continue;

            roadDepartures[rid]++;

            int destNode = graph.roads[rid].destination;

            if (destNode != v.destination) {
                graph.roads[rid].queueCount++;
            }

            v.arriveAtNode(destNode, rid);
            v.clearedStopLine = red;   // crossed on green, light flipped since -> may still clear

            if (destNode == v.destination) {
                v.markArrived(currentStep);
                totalCompleted++;

                int tt = v.getTravelTime();
                completedTravelTimes.push_back(tt);

                double freeTime = 0.0;
                for (int i = 0; i + 1 < (int)v.path.size(); i++) {
                    int roadId = graph.findRoadIndex(v.path[i], v.path[i + 1]);
                    if (roadId >= 0)
                        freeTime += graph.roads[roadId].travelTime;
                }
                completedFreeTimes.push_back(freeTime);
            }
        }
        return roadDepartures;
    }

    void updateSignals() {
        map<int, int> roadQueues;
        for (Road& r : graph.roads)
            roadQueues[r.id] = r.queueCount;
        // cars parked on the stop line of a red road are waiting too, so they
        // count toward that road's queue when deciding who gets the next green
        for (const Vehicle& v : vehicles)
            if (isHeldAtLine(v)) roadQueues[v.currentRoad]++;
        for (auto& kv : signals)
            kv.second.update(roadQueues);
    }

    // FIX (kept from earlier review): only releases vehicles that are
    // actually queued on THIS road (i.e. arrived via a road, not still at
    // their original source), and only when this road's signal is green.
    // This is the mechanism that makes cars stop at a red light.
    //
    // DEBUG_SIGNALS: prints ground-truth signal/queue state every step,
    // independent of any rendering. Set to false once verified.
    static const bool DEBUG_SIGNALS = false;

    void releaseFromQueues() {
        for (Road& r : graph.roads) {
            if (r.queueCount <= 0) continue;

            int destNode = r.destination;
            bool red = isRed(r.id);
            int sig = red ? 0 : 1;

            if (DEBUG_SIGNALS) {
                cout << "[SIGNAL DEBUG] step=" << currentStep
                    << " road=" << r.id << " ->node=" << destNode
                    << " queue=" << r.queueCount
                    << " signal=" << (sig ? "GREEN" : "RED") << endl;
            }

            // RED: nothing leaves this road's queue, except cars that had already
            // crossed the stop line on green (clearedStopLine) -- see below.

            int released = 0;
            int maxRelease = (int)r.dischargeRate;

            for (Vehicle& v : vehicles) {
                if (released >= maxRelease) break;
                if (v.status != WAITING) continue;
                if (v.currentNode != destNode) continue;
                if (v.lastRoadId != r.id) continue; // THE FIX: only release vehicles that actually arrived via THIS road -- without this, a road with a GREEN signal could steal and release a vehicle that arrived via a DIFFERENT road at the same junction whose signal is RED
                if (v.pathIndex == 0 && v.currentNode == v.source) continue; // handled by dispatchWaitingVehicles instead
                if (red && !v.clearedStopLine) continue; // red light: this car waits
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

            if (DEBUG_SIGNALS && released > 0) {
                cout << "[SIGNAL DEBUG]   -> released " << released << " vehicle(s) from road " << r.id << endl;
            }
        }
    }

    void updateRoadStates(map<int, int>& roadDepartures) {
        for (Road& r : graph.roads) {
            if (roadDepartures.count(r.id))
                r.currentFlow -= roadDepartures[r.id];
            if (r.currentFlow < 0)
                r.currentFlow = 0;

            r.updateCongestion();
            r.updateTravelTime();
        }

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

    // FIX (kept from earlier review): only dispatches vehicles still at
    // their ORIGINAL source (pathIndex == 0, never queued). Every other
    // WAITING vehicle already went through a road at least once and must
    // go through the signal-gated releaseFromQueues() above instead --
    // otherwise it would bypass the red light in the same step it queued.
    void dispatchWaitingVehicles() {
        for (Vehicle& v : vehicles) {
            if (v.status != WAITING) continue;
            if (v.currentNode == v.destination) continue;
            if (!v.hasPath()) continue;
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

        FileManager::appendTrafficLog(currentStep, moving, waiting, arrived, avgCong, avgTT);
        if (currentStep % 10 == 0)
            FileManager::saveRoadsTxt(currentStep, graph.roads);
    }

    void printFinalReport() {
        double avgTT = TrafficFormula::averageTravelTime(completedTravelTimes);
        double delay = TrafficFormula::totalDelay(completedTravelTimes, completedFreeTimes);
        double throughput = totalSteps > 0 ? (double)totalCompleted / totalSteps : 0.0;
        double avgCong = TrafficFormula::averageCongestion(stepAvgCongestion);
        int stillWaiting = totalGenerated - totalCompleted;

        FileManager::saveTrafficState(currentStep, graph.roads);
        FileManager::saveVehicleData(vehicles);
        FileManager::saveRoadData(graph.roads);
        FileManager::saveVehiclesTxt(vehicles);
        FileManager::exportReport(totalSteps, totalCompleted, stillWaiting,
            avgTT, delay, throughput, avgCong, mostCongestedRoadTracked, mostBusyNodeTracked);
    }
};