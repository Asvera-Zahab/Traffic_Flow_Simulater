// ============================================================
// main.cpp
// Drives Renderer::render() from the REAL Simulator (Graph +
// Vehicle + TrafficSignal state), not a hardcoded fake snapshot.
//
// Two independent clocks:
//   - sim clock: fires roughly every `secondsPerStep` and calls
//     sim.stepOnce() ONCE. This is where flow/queue/signal state
//     actually changes -- exactly as it did in the console version.
//   - render clock: runs every frame (60 fps via the window's
//     frame limiter) and draws a snapshot built from the sim's
//     CURRENT state, with vehicle progress interpolated forward
//     by however much of the current step-interval has elapsed.
//     That's what makes cars glide smoothly instead of teleporting
//     once per tick.
// ============================================================

#include "Renderer.h"
#include "Simulator.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>

// Set true to print per-step dot counts to the console while debugging.
static const bool kDebugSnapshot = false;

// A car that reaches its destination keeps driving INTO its junction for this
// many sim steps (shrinking and fading as it goes) instead of stopping at the
// signal and popping out of existence.
static const int kArrivedLingerSteps = 1;

// Screen layout for the 5-node demo city graph built by
// Simulator::buildCityGraph(). If you change that graph's nodes,
// update this table (falls back to a default spot for anything
// not listed here so it never crashes on a bigger graph).
// Same "fan" layout used by the old fake snapshot: Lahore (2) is
// the hub connecting to all four others, which sit on an arc.
static const std::map<int, sf::Vector2f> kNodeLayout = {
    { 0, { 330.f, 190.f } },  // Karachi
    { 1, { 640.f, 130.f } },  // Islamabad
    { 2, { 560.f, 420.f } },  // Lahore (hub)
    { 3, { 920.f, 220.f } },  // Murree
    { 4, { 1060.f, 430.f } }, // Kashmir
};

// Keeps first-come-first-served order of the cars parked on each road so a
// queue never reshuffles itself between frames: cars that are still parked keep
// their place, cars that left drop out (everyone behind moves up), and new
// arrivals join the back of the line.
static void reconcileOrder(std::map<int, std::vector<int>>& order,
    const std::map<int, std::vector<int>>& present) {
    for (auto it = order.begin(); it != order.end();) {
        auto pr = present.find(it->first);
        std::vector<int>& ids = it->second;
        ids.erase(std::remove_if(ids.begin(), ids.end(), [&](int id) {
            if (pr == present.end()) return true;
            return std::find(pr->second.begin(), pr->second.end(), id) == pr->second.end();
            }), ids.end());
        if (ids.empty()) it = order.erase(it);
        else ++it;
    }
    for (const auto& kv : present) {
        std::vector<int>& ids = order[kv.first];
        for (int id : kv.second)
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
}

static int slotIn(const std::map<int, std::vector<int>>& order, int roadId, int vehicleId) {
    auto it = order.find(roadId);
    if (it == order.end()) return 0;
    auto pos = std::find(it->second.begin(), it->second.end(), vehicleId);
    return pos == it->second.end() ? 0 : (int)(pos - it->second.begin());
}

// The road a not-yet-started car should be drawn on: the first road of its route.
static int roadForUnstartedVehicle(const Simulator& sim, const Vehicle& v) {
    int next = v.getNextNode();
    if (next >= 0) {
        int rid = sim.graph.findRoadIndex(v.currentNode, next);
        if (rid >= 0) return rid;
    }
    auto it = sim.graph.nodes.find(v.currentNode);   // fallback: any road leaving this node
    if (it != sim.graph.nodes.end() && !it->second.outgoingRoads.empty())
        return it->second.outgoingRoads.front();
    return -1;
}

// Builds a SimSnapshot from the simulator's live state.
// stepFraction (0..1) is how far we are into the CURRENT sim tick,
// used purely to interpolate vehicle dots forward visually; it never
// touches actual simulation state.
//
// DRAW CONTRACT -- every car that is still in the network gets exactly one dot,
// always on a road, never hidden:
//   MOVING, light green (or already past the stop line) -> glides along its road   (Moving)
//   MOVING, light RED, still before the stop line        -> glides up to the line,
//                                                           stops behind any cars already there (ApproachRed)
//   MOVING, light RED, sitting on the stop line          -> parked                  (StoppedAtLine)
//   WAITING in a junction queue (lastRoadId >= 0)         -> parked on the line     (StoppedAtLine)
//   WAITING at its source, not yet on a road              -> parked at road start   (WaitingAtStart)
//   just ARRIVED                                          -> drives into the junction and fades out (Exiting)
// A dot in a "moving" mode is therefore never drawn moving through a red light.
SimSnapshot buildSnapshot(const Simulator& sim, float stepFraction) {
    SimSnapshot snap;
    snap.step = sim.currentStep;
    snap.stopLineProgress = static_cast<float>(STOP_LINE_PROGRESS);
    const float stopP = snap.stopLineProgress;

    // ---- Vehicles (pass 1: decide each dot's mode and road) ----
    std::vector<VehicleView> dots;
    std::map<int, std::vector<int>> parkedIds;  // roadId -> ids parked on its stop line this frame
    std::map<int, std::vector<int>> startIds;   // roadId -> ids waiting at its start this frame
    const int roadCount = (int)sim.graph.roads.size();

    for (const Vehicle& v : sim.vehicles) {
        if (v.status == MOVING) {
            if (v.currentRoad < 0 || v.currentRoad >= roadCount) continue;

            double entry = v.entryTravelTime;
            float base = entry > 0.0 ? 1.f - static_cast<float>(v.remainingTravelTime / entry) : 0.f;
            float extra = entry > 0.0 ? stepFraction / static_cast<float>(entry) : 0.f;
            float progress = std::clamp(base + extra, 0.f, 1.f);

            bool beforeLine = v.remainingTravelTime >= sim.stopLineRemaining(v) - STOP_LINE_EPS;
            if (sim.isRed(v.currentRoad) && beforeLine) {
                if (sim.isHeldAtLine(v)) {
                    dots.push_back({ v.id, v.currentRoad, stopP, DotMode::StoppedAtLine, 0, 1.f });
                    parkedIds[v.currentRoad].push_back(v.id);
                }
                else {
                    dots.push_back({ v.id, v.currentRoad, std::min(progress, stopP), DotMode::ApproachRed, 0, 1.f });
                }
            }
            else {
                dots.push_back({ v.id, v.currentRoad, progress, DotMode::Moving, 0, 1.f });
            }
        }
        else if (v.status == WAITING && v.currentNode != v.destination) {
            if (v.lastRoadId >= 0) {
                // queued at a junction: waits on the stop line of the road it arrived on
                dots.push_back({ v.id, v.lastRoadId, stopP, DotMode::StoppedAtLine, 0, 1.f });
                parkedIds[v.lastRoadId].push_back(v.id);
            }
            else {
                // spawned but could not enter its first road yet: wait at the start of it
                int rid = roadForUnstartedVehicle(sim, v);
                if (rid < 0) continue;
                dots.push_back({ v.id, rid, 0.f, DotMode::WaitingAtStart, 0, 1.f });
                startIds[rid].push_back(v.id);
            }
        }
        else if (v.status == ARRIVED && v.lastRoadId >= 0) {
            int age = sim.currentStep - v.stepArrived;
            if (age >= 0 && age < kArrivedLingerSteps) {
                // progress = how far into the junction it has driven this step (0..1)
                dots.push_back({ v.id, v.lastRoadId, stepFraction, DotMode::Exiting, 0, 1.f });
            }
        }
    }

    // ---- Vehicles (pass 2: stable queue positions) ----
    static std::map<int, std::vector<int>> parkedOrder, startOrder;
    reconcileOrder(parkedOrder, parkedIds);
    reconcileOrder(startOrder, startIds);

    int moving = 0, waiting = 0;
    for (VehicleView& d : dots) {
        if (d.mode == DotMode::StoppedAtLine) { d.slot = slotIn(parkedOrder, d.roadId, d.id); waiting++; }
        else if (d.mode == DotMode::WaitingAtStart) { d.slot = slotIn(startOrder, d.roadId, d.id); waiting++; }
        else if (d.mode != DotMode::Exiting) moving++;
        snap.vehicles.push_back(d);
    }

    // Stats now describe exactly what is on screen: "Moving" = dots that are
    // travelling, "Waiting" = dots that are standing still.
    snap.movingCount = moving;
    snap.waitingCount = waiting;
    snap.arrivedCount = sim.totalCompleted;
    snap.generatedCount = sim.totalGenerated;

    double sumCong = 0.0;
    for (const Road& r : sim.graph.roads) sumCong += r.congestion;
    snap.avgCongestion = sim.graph.roads.empty()
        ? 0.f
        : static_cast<float>(sumCong / sim.graph.roads.size());

    // Nodes
    for (const auto& kv : sim.graph.nodes) {
        const Node& n = kv.second;
        sf::Vector2f pos{ 100.f, 100.f };
        auto it = kNodeLayout.find(n.id);
        if (it != kNodeLayout.end()) pos = it->second;
        snap.nodes.push_back({ n.id, n.name, pos.x, pos.y });
    }

    // Roads (Q = cars standing on that road's stop line)
    for (const Road& r : sim.graph.roads) {
        RoadView rv;
        rv.id = r.id;
        rv.srcNode = r.source;
        rv.dstNode = r.destination;
        rv.flow = r.currentFlow;
        rv.capacity = r.capacity;
        auto pk = parkedIds.find(r.id);
        rv.queueLen = pk == parkedIds.end() ? 0 : (int)pk->second.size();
        rv.congestion = static_cast<float>(r.congestion);
        rv.blocked = (r.capacity == 0);
        snap.roads.push_back(rv);
    }

    if (kDebugSnapshot) {
        static int lastPrintedStep = -1;
        if (sim.currentStep != lastPrintedStep) {
            lastPrintedStep = sim.currentStep;
            std::cout << "[SNAPSHOT DEBUG] step=" << sim.currentStep
                << " movingDots=" << moving << " stoppedDots=" << waiting << std::endl;
        }
    }

    // Signals
    for (const auto& kv : sim.signals) {
        snap.signals.push_back({ kv.first, kv.second.currentGreenRoad });
    }

    return snap;
}

int main() {
    std::cout << "\n############################################\n"
        << "  MAIN.CPP BUILD MARKER: stopline-v1\n"
        << "  If you do not see this line, your project\n"
        << "  is NOT using this main.cpp.\n"
        << "############################################\n" << std::endl;

    Simulator sim;
    sim.initialize(300); // step budget; auto-loops via stepOnce() when reached

    Renderer renderer(1280, 720, "Traffic Flow Simulation");

    sf::Clock stepClock;
    const float secondsPerStep = 0.6f; // wall-clock seconds per sim tick at 1x speed

    while (renderer.isOpen()) {
        renderer.pollEvents();

        const float interval = secondsPerStep / renderer.getSpeedMultiplier();
        float elapsed = stepClock.getElapsedTime().asSeconds();

        if (!renderer.isPaused() && elapsed >= interval) {
            sim.stepOnce();
            stepClock.restart();
            elapsed = 0.f;
        }

        float stepFraction = renderer.isPaused()
            ? 0.f
            : std::clamp(elapsed / interval, 0.f, 1.f);

        SimSnapshot snap = buildSnapshot(sim, stepFraction);
        renderer.render(snap);
    }

    return 0;
}