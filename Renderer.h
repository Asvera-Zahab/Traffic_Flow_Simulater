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

// How a vehicle dot should be placed on its road.
enum class DotMode {
    Moving,          // driving freely: placed from `progress`
    ApproachRed,     // driving up to a RED light: never drawn past the stop line / the queue in front of it
    StoppedAtLine,   // parked on the stop line (or in the line of cars behind it), `slot` = place in the line, 0 = front
    WaitingAtStart,  // spawned but not yet on the road: parked at the start of it, `slot` = place in line
    Exiting          // just reached its destination: `progress` (0..1) = how far it has driven INTO the junction; fades out on the way
};

struct VehicleView {
    int id;
    int roadId;
    float progress;                 // 0 = start of the road, 1 = the junction (STOP line is at snap.stopLineProgress)
    DotMode mode = DotMode::Moving;
    int slot = 0;
    float alpha = 1.f;              // 1 = solid, 0 = invisible
    bool highlight = false;         // true = the user-generated car from the "Run" button; drawn green
};

struct SignalView {
    int nodeId;
    int activeRoadId; // which incoming road currently has green
};

struct SimSnapshot {
    int step;
    int movingCount, waitingCount, arrivedCount, generatedCount;
    float avgCongestion; // expected range 0..1
    float stopLineProgress = 0.80f; // where the stop line sits along every road (0..1); must match the simulator
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
    void drawContainer(sf::Vector2f pos, bool blocked);
    bool isPaused() const { return paused; }
    float getSpeedMultiplier() const { return speedMultiplier; }

    // "Run" button (bottom-left, under the title box). Returns true exactly
    // once per click, then resets itself.
    bool consumeRunClicked();

    // "Generate Car" flow: clicking the button opens an on-screen dialog
    // (drawn inside render()) that asks for source then destination node
    // id, typed right into the SFML window -- no console interaction.
    // Call this once per frame; it returns true exactly once, with src/dst
    // filled in, the moment the person finishes typing both. Caller
    // (main.cpp) then computes the route and reports back with showMessage().
    bool consumeGeneratedRoute(int& src, int& dst);

    // Shows a short-lived banner near the top of the window (e.g. the
    // computed shortest path, or an error). Replaces console-only feedback.
    void showMessage(const std::string& text, bool isError = false);

    void close();

private:
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded;

    bool paused;
    float speedMultiplier;

    // ---- "Generate Car" / "Run" buttons ----
    sf::FloatRect btnGenerateRect{ { 20.f, 70.f },  { 170.f, 42.f } };
    sf::FloatRect btnRunRect{ { 20.f, 122.f }, { 170.f, 42.f } };
    bool runClicked = false;
    void drawSideButtons();

    // ---- on-screen "enter source/destination" dialog ----
    enum class InputStage { None, Source, Destination };
    InputStage inputStage = InputStage::None;
    std::string inputBuffer;     // digits typed so far for the current field
    int capturedSrc = -1;
    int capturedDst = -1;
    bool routeReady = false;     // both fields captured, waiting for consumeGeneratedRoute()
    sf::Clock cursorBlinkClock;
    void drawInputDialog(const SimSnapshot& snap);

    // ---- short-lived message banner (result of Generate Car / Run) ----
    std::string bannerText;
    bool bannerIsError = false;
    bool bannerActive = false;
    sf::Clock bannerClock;
    static constexpr float BANNER_SECONDS = 6.f;
    void drawBanner();

    // Layout constants (pixels)
    static constexpr float RING_RADIUS = 26.f;          // fixed -- the name lives in a separate pill, not inside the ring
    static constexpr float RING_THICKNESS = 4.5f;
    static constexpr float ROAD_THICKNESS = 5.f;
    static constexpr float ROAD_WIDTH = 14.f;           // asphalt width (the coloured edge adds 2px each side)
    static constexpr float VEHICLE_RADIUS = 4.5f;
    static constexpr float SIGNAL_DOT_RADIUS = 7.f;      // (kept for sizing reference)
    static constexpr float SIGNAL_BOX_W = 24.f, SIGNAL_BOX_H = 18.f; // signal box laid across the lane, like a stop line (W must stay 24: the stop-line maths uses it)
    static constexpr float STUB_LENGTH = 80.f;
    static constexpr float BOTTOM_BAR_HEIGHT = 26.f;
    static constexpr float LANE_OFFSET = 9.f;   // sideways shift of each road's own lane, so a road and its reverse partner don't sit on top of each other

    // Sideways offset to apply to BOTH endpoints of a road drawn from
    // `from` to `to`, so it renders as its own lane instead of sharing the
    // node-to-node centerline with the road running the opposite way.
    // Offsetting to the right of travel direction means the offset flips
    // sign automatically between a road and its reverse (u flips sign),
    // so the two lanes end up on opposite sides with no extra bookkeeping.
    sf::Vector2f laneOffsetFor(sf::Vector2f from, sf::Vector2f to) const;

    // The demo graph is laid out in "layout units" (main.cpp's kNodeLayout).
    // Every frame updateLayout() works out a uniform scale + offset that fits
    // the junctions into the CURRENT window size and centres them, and
    // toScreen() applies it. Only the distances BETWEEN junctions scale --
    // text, rings, signals and cars keep their crisp pixel size.
    float layoutScale = 1.f;
    sf::Vector2f layoutOffset{ 0.f, 0.f };

    sf::Vector2f toScreen(sf::Vector2f p) const { return { p.x * layoutScale + layoutOffset.x, p.y * layoutScale + layoutOffset.y }; }
    sf::Vector2f toScreen(float x, float y) const { return { x * layoutScale + layoutOffset.x, y * layoutScale + layoutOffset.y }; }
    void updateLayout(const SimSnapshot& snap);

    bool loadBestAvailableFont();
    sf::Text makeText(const std::string& str, unsigned int size, sf::Color color) const;

    sf::Color congestionColor(float congestion, bool blocked) const;
    sf::Color nodeColor(const std::string& name, int id) const;
    sf::ConvexShape roundedLabelBox(sf::Vector2f center, sf::Vector2f size, sf::Color fill, float radius = 8.f) const;
    void drawPanel(sf::Vector2f topLeft, sf::Vector2f size, float radius = 10.f);
    void drawRoadStrip(sf::Vector2f a, sf::Vector2f b, sf::Color edge, bool blocked, float width);

    void drawBackground();
    void drawLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void drawDashedLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void drawArrowHead(sf::Vector2f tip, sf::Vector2f dir, sf::Color color, float size = 10.f);
    void drawSignalBox(sf::Vector2f pos, float angleDeg, int roadId, bool green, bool blocked);

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