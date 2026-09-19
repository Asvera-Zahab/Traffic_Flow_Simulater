#pragma once
// ============================================================
// Renderer.h
// Pure rendering/front-end layer for the traffic simulation.
// Contains NO simulation logic (no routing, no flow updates,
// no spawning). Call Renderer::render(snapshot) once per step
// from your own simulation loop.
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

    // True while the window is still open. Drive your loop with this.
    bool isOpen() const;

    // Poll and handle SFML events: window close, ESC to quit,
    // SPACE to toggle pause, +/- to adjust speed. Call once per frame,
    // BEFORE render(), from your own loop.
    void pollEvents();

    // Draw one full frame from the given snapshot. Does one
    // clear -> draw everything -> display cycle internally.
    void render(const SimSnapshot& snapshot);

    // Renderer-tracked UI state your simulation loop can read.
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
    static constexpr float NODE_RADIUS = 16.f;
    static constexpr float ROAD_THICKNESS = 6.f;
    static constexpr float VEHICLE_RADIUS = 5.f;
    static constexpr float SIGNAL_RADIUS = 7.f;
    static constexpr float LEFT_PANEL_WIDTH = 210.f;
    static constexpr float BOTTOM_BAR_HEIGHT = 30.f;
    static constexpr float TRIM = NODE_RADIUS + 4.f; // keep roads off node circles

    sf::Vector2f mapOffset() const { return { LEFT_PANEL_WIDTH, 0.f }; }
    sf::Vector2f toScreen(sf::Vector2f p) const { return p + mapOffset(); }
    sf::Vector2f toScreen(float x, float y) const { return toScreen(sf::Vector2f{ x, y }); }

    bool loadBestAvailableFont();
    sf::Text makeText(const std::string& str, unsigned int size, sf::Color color) const;

    sf::Color congestionColor(float congestion, bool blocked) const;

    void drawGrid();
    void drawThickLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void drawDashedThickLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void drawThinDashedConnector(sf::Vector2f a, sf::Vector2f b, sf::Color color);
    void drawArrowHead(sf::Vector2f tip, sf::Vector2f dir, sf::Color color);

    void drawRoads(const SimSnapshot& snap, const std::map<int, NodeView>& nodeById);
    void drawNodes(const SimSnapshot& snap);
    void drawVehicles(const SimSnapshot& snap,
        const std::map<int, RoadView>& roadById,
        const std::map<int, NodeView>& nodeById);
    void drawSignals(const SimSnapshot& snap,
        const std::map<int, RoadView>& roadById,
        const std::map<int, NodeView>& nodeById);

    void drawLegend();
    void drawHud(const SimSnapshot& snap);
    void drawControlsStrip();
};