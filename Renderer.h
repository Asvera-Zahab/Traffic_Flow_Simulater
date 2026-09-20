#pragma once
// ============================================================
// Renderer.h
// Pure rendering/front-end layer for the traffic simulation.
// Contains NO simulation logic (no routing, no flow updates,
// no spawning). Call Renderer::render(snapshot) once per step
// from your own simulation loop.
//
// Visual style: top-down "game map" look (grass background,
// asphalt roads with dashed lane markings, dark label boxes,
// one colored signal square per incoming road, black car dots).
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

    // Decorative background (bushes/trees), generated once so it
    // doesn't jitter or cost anything per-frame.
    std::vector<sf::Vector2f> decorPositions;
    std::vector<float> decorRadii;
    void generateBackgroundDecor();

    // Layout constants (pixels)
    static constexpr float ROAD_THICKNESS = 34.f;
    static constexpr float STUB_ROAD_THICKNESS = 24.f;
    static constexpr float STUB_LENGTH = 90.f;
    static constexpr float VEHICLE_RADIUS = 4.f;
    static constexpr float SIGNAL_SIZE = 18.f;          // side length of the signal square
    static constexpr float BOTTOM_BAR_HEIGHT = 26.f;
    static constexpr float TRIM = 34.f;                  // keep road surfaces off junction label boxes

    sf::Vector2f toScreen(sf::Vector2f p) const { return p; }
    sf::Vector2f toScreen(float x, float y) const { return { x, y }; }

    bool loadBestAvailableFont();
    sf::Text makeText(const std::string& str, unsigned int size, sf::Color color) const;

    sf::Color congestionColor(float congestion, bool blocked) const;
    sf::RectangleShape roundedLabelBox(sf::Vector2f center, sf::Vector2f size, sf::Color fill) const;

    void drawBackground();
    void drawRoadSurface(sf::Vector2f a, sf::Vector2f b, float thickness);
    void drawJunctionPad(sf::Vector2f pos, float thickness);
    void drawDashedLaneLine(sf::Vector2f a, sf::Vector2f b);
    void drawDashedThickLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void drawArrowHead(sf::Vector2f tip, sf::Vector2f dir, sf::Color color, float size = 12.f);

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