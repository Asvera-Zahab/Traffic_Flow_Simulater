// ============================================================
// main.cpp - DEMO ONLY.
// Shows how to drive Renderer::render() from your own loop.
// Replace buildFakeSnapshot() with a function that reads your
// real Simulator/Graph/Vehicle state and fills a SimSnapshot.
// ============================================================

#include "Renderer.h"
#include <cmath>
#include <vector>

// Builds a snapshot for the demo city graph (matches Simulator::buildCityGraph):
// 0 Karachi, 1 Islamabad, 2 Lahore, 3 Murree, 4 Kashmir
// roads: 0:(0->1) 1:(0->2) 2:(1->2) 3:(1->3) 4:(2->3) 5:(3->4) 6:(2->4)
SimSnapshot buildFakeSnapshot(int step) {
    SimSnapshot snap;
    snap.step = step;
    snap.movingCount = 12;
    snap.waitingCount = 5;
    snap.arrivedCount = 61;
    snap.generatedCount = 78;
    snap.avgCongestion = 0.42f;

    snap.nodes = {
        { 0, "Karachi",   120.f, 90.f },
        { 1, "Islamabad", 380.f, 60.f },
        { 2, "Lahore",    220.f, 320.f },
        { 3, "Murree",    620.f, 220.f },
        { 4, "Kashmir",   700.f, 480.f },
    };

    bool road1Blocked = (step >= 15 && step < 25); // matches scheduleEvents() in Simulator.h

    snap.roads = {
        { 0, 0, 1, 4, 12, 0, 0.20f, false },
        { 1, 0, 2, road1Blocked ? 0 : 5, 8, road1Blocked ? 0 : 1, road1Blocked ? 0.f : 0.55f, road1Blocked },
        { 2, 1, 2, 3, 6, 0, 0.30f, false },
        { 3, 1, 3, 13, 14, 4, 0.90f, false },
        { 4, 2, 3, 4, 5, 1, 0.60f, false },
        { 5, 3, 4, 7, 10, 1, 0.25f, false },
        { 6, 2, 4, 5, 7, 0, 0.55f, false },
    };

    snap.vehicles = {
        { 100, 3, 0.35f },
        { 101, 2, 0.60f },
        { 102, 4, 0.20f },
    };

    // signals: which road currently has green at each intersection
    snap.signals = {
        { 1, 0 }, // Islamabad: only incoming road (0) is green
        { 2, 2 }, // Lahore: road 2 green (road 1 is blocked, out of contention)
        { 3, 3 }, // Murree: road 3 green (highest queue), road 4 red
        { 4, 5 }, // Kashmir: road 5 green, road 6 red
    };

    return snap;
}

int main() {
    Renderer renderer(1280, 720, "Traffic Flow Simulation");

    int step = 0;
    sf::Clock stepClock;
    const float secondsPerStep = 0.5f;

    while (renderer.isOpen()) {
        renderer.pollEvents();

        if (!renderer.isPaused() &&
            stepClock.getElapsedTime().asSeconds() >= secondsPerStep / renderer.getSpeedMultiplier()) {
            step++;
            if (step > 200) step = 0; // loop the demo
            stepClock.restart();
        }

        SimSnapshot snap = buildFakeSnapshot(step);
        renderer.render(snap);
    }

    return 0;
}