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
#include <map>

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

// Builds a SimSnapshot from the simulator's live state.
// stepFraction (0..1) is how far we are into the CURRENT sim tick,
// used purely to interpolate vehicle dots forward visually; it never
// touches actual simulation state.
SimSnapshot buildSnapshot(const Simulator& sim, float stepFraction) {
    SimSnapshot snap;
    snap.step = sim.currentStep;

    int moving = 0, waiting = 0;
    for (const Vehicle& v : sim.vehicles) {
        if (v.status == MOVING) moving++;
        else if (v.status == WAITING && v.currentNode != v.destination) waiting++;
    }
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

    // Roads
    for (const Road& r : sim.graph.roads) {
        RoadView rv;
        rv.id = r.id;
        rv.srcNode = r.source;
        rv.dstNode = r.destination;
        rv.flow = r.currentFlow;
        rv.capacity = r.capacity;
        rv.queueLen = r.queueCount;
        rv.congestion = static_cast<float>(r.congestion);
        rv.blocked = (r.capacity == 0);
        snap.roads.push_back(rv);
    }

    // Vehicles: only ones actually MOVING have a meaningful
    // road+progress (waiting/arrived vehicles aren't drawn as dots
    // on a road, matching the original VehicleView contract).
    for (const Vehicle& v : sim.vehicles) {
        if (v.status != MOVING) continue;

        float progress = 0.f;
        if (v.entryTravelTime > 0.0) {
            float base = 1.f - static_cast<float>(v.remainingTravelTime / v.entryTravelTime);
            float extra = stepFraction / static_cast<float>(v.entryTravelTime);
            // Cap just under 1 so a car never visually reaches the
            // node before the sim tick that actually delivers it there.
            progress = std::clamp(base + extra, 0.f, 0.98f);
        }
        snap.vehicles.push_back({ v.id, v.currentRoad, progress });
    }

    // Signals
    for (const auto& kv : sim.signals) {
        snap.signals.push_back({ kv.first, kv.second.currentGreenRoad });
    }

    return snap;
}

int main() {
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