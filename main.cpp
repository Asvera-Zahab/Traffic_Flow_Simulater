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

    // Layout note: this graph is a "fan" -- Lahore connects to all four
    // other nodes, while Karachi-Islamabad-Murree-Kashmir form a simple
    // chain (0-1, 1-3, 3-4) around it. Placing Lahore as a central hub
    // with the other four arranged along an arc in that chain order gives
    // a ZERO-CROSSING layout: the four spokes go straight to the hub, and
    // the three chain edges only ever connect adjacent points on the arc.
    snap.nodes = {
        { 0, "Karachi",   330.f, 190.f },  // arc point 1
        { 1, "Islamabad", 640.f, 130.f },  // arc point 2
        { 2, "Lahore",    560.f, 420.f },  // hub (connects to all 4 others)
        { 3, "Murree",    920.f, 220.f },  // arc point 3
        { 4, "Kashmir",  1060.f, 430.f },  // arc point 4
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

    // A handful of vehicles per road (black dots), spread along each road
    // so the map reads as "busy" the way the reference image does.
    snap.vehicles = {
        { 100, 0, 0.15f }, { 101, 0, 0.55f },
        { 102, 2, 0.30f }, { 103, 2, 0.70f },
        { 104, 3, 0.20f }, { 105, 3, 0.45f }, { 106, 3, 0.80f },
        { 107, 4, 0.35f }, { 108, 4, 0.65f },
        { 109, 5, 0.50f },
        { 110, 6, 0.25f }, { 111, 6, 0.75f },
    };
    if (!road1Blocked) snap.vehicles.push_back({ 112, 1, 0.40f });

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