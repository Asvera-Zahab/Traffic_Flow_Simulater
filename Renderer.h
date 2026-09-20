#pragma once
// ============================================================
// Renderer.h
// Pure rendering/front-end layer for the traffic simulation.
// Contains NO simulation logic (no routing, no flow updates,
// no spawning). Call Renderer::render(snapshot) once per step
// from your own simulation loop.
//
// Visual style: "traffic monitor" dashboard look -- plain dark
// navy background, hollow colored ring per junction (fixed
// radius, city name in a small pill above it), thin roads
// colored by congestion (green/amber/red), red dashed line for
// a blocked road, a small arrow at the destination end of each
// road (stopped short of the ring, never overlapping it), and a
// real red/yellow/green traffic-light icon per incoming road.
//
// Built against SFML 3.1 (C++17).
// ============================================================

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <map>

// ---- Snapshot data (filled in by YOUR simulation each frame) ----

struct NodeView {
    int id;
    std::string name;
    float x, y;
};

struct RoadView {
    int id;
    int srcNode;
    int dstNode;
    int flow;
    int capacity;
    int queueLen;
    float congestion; // expected range 0..1
    bool blocked;
};

struct VehicleView {
    int id;
    int roadId;
    float progress; // 0 = at srcNode, 1 = at dstNode
};

struct SignalView {
    int nodeId;
    int activeRoadId; // which incoming road currently has green
};

struct SimSnapshot {
    int step;
    int movingCount, waitingCount, arrivedCount, generatedCount;
    float avgCongestion; // expected range 0..1
    std::vector<NodeView> nodes;
    std::vector<RoadView> roads;
    std::vector<VehicleView> vehicles;
    std::vector<SignalView> signals;
};

// ---- Renderer ----

class Renderer {
public:
    explicit Renderer(unsigned int width = 1280, unsigned int height = 720,
        const std::string& title = "Traffic Flow Simulation");

    bool isOpen() const;
    void pollEvents();
    void render(const SimSnapshot& snapshot);

    bool isPaused() const { return paused; }
    float getSpeedMultiplier() const { return speedMultiplier; }

    void close();

private:
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded;

    bool paused;
    float speedMultiplier;

    // Layout constants (pixels)
    static constexpr float RING_RADIUS = 26.f;          // fixed -- the name lives in a separate pill, not inside the ring
    static constexpr float RING_THICKNESS = 4.5f;
    static constexpr float ROAD_THICKNESS = 5.f;
    static constexpr float VEHICLE_RADIUS = 4.5f;
    static constexpr float SIGNAL_DOT_RADIUS = 7.f;      // simple colored dot, not a 3-light housing
    static constexpr float STUB_LENGTH = 80.f;
    static constexpr float BOTTOM_BAR_HEIGHT = 26.f;

    sf::Vector2f toScreen(sf::Vector2f p) const { return p; }
    sf::Vector2f toScreen(float x, float y) const { return { x, y }; }

    bool loadBestAvailableFont();
    sf::Text makeText(const std::string& str, unsigned int size, sf::Color color) const;

    sf::Color congestionColor(float congestion, bool blocked) const;
    sf::Color nodeColor(const std::string& name, int id) const;
    sf::RectangleShape roundedLabelBox(sf::Vector2f center, sf::Vector2f size, sf::Color fill) const;

    void drawBackground();
    void drawLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void drawDashedLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void drawArrowHead(sf::Vector2f tip, sf::Vector2f dir, sf::Color color, float size = 10.f);
    void drawSignalDot(sf::Vector2f pos, int roadId, bool green, bool blocked);

    void drawTitleBox(const SimSnapshot& snap);
    void drawStatsBox(const SimSnapshot& snap);
    void drawRoads(const SimSnapshot& snap, const std::map<int, NodeView>& nodeById);
    void drawExternalStubs(const SimSnapshot& snap, const std::map<int, NodeView>& nodeById);
    void drawJunctionLabels(const SimSnapshot& snap);
    void drawVehicles(const SimSnapshot& snap,
        const std::map<int, RoadView>& roadById,
        const std::map<int, NodeView>& nodeById);
    void drawSignals(const SimSnapshot& snap,
        const std::map<int, RoadView>& roadById,
        const std::map<int, NodeView>& nodeById);

    void drawLegend(const SimSnapshot& snap);
    void drawCompass();
    void drawControlsStrip();
};