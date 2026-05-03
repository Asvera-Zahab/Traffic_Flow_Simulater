

#include <SFML/Graphics.hpp>
#include <cmath>
#include <map>
#include "Simulator.h"

using namespace std;

// Window
const int WIDTH = 1000;
const int HEIGHT = 700;

// Simulation
float stepInterval = 0.6f;
const int TOTAL_STEPS = 100;

// Node layout positions on screen
map<int, sf::Vector2f> nodePos = {
    {0, {150.f, 350.f}},
    {1, {400.f, 120.f}},
    {2, {650.f, 350.f}},
    {3, {400.f, 580.f}},
    {4, {850.f, 350.f}}
};

// Road color based on congestion (green->red)
sf::Color getRoadColor(float c)
{
    if (c > 1.f) c = 1.f;
    return sf::Color(
        (uint8_t)(255 * c),
        (uint8_t)(255 * (1 - c)),
        60
    );
}

// Draw a line between two points
void drawRoad(sf::RenderWindow& win, sf::Vector2f a, sf::Vector2f b, sf::Color col)
{
    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = a;
    line[0].color = col;
    line[1].position = b;
    line[1].color = col;
    win.draw(line);
}

int main()
{
    Simulator sim;
    sim.buildCityGraph();
    sim.setupSignals();
    sim.scheduleEvents();

    sf::RenderWindow window(
        sf::VideoMode({ WIDTH, HEIGHT }),
        "Traffic Simulation"
    );
    window.setFramerateLimit(60);

    sf::Clock clock;
    float accumulator = 0.f;
    bool paused = false;

    // Smooth positions for each vehicle (id -> screen pos)
    map<int, sf::Vector2f> vehiclePos;

    // -------------------------------------------------------
    // STEP FUNCTION - runs one simulation step
    // -------------------------------------------------------
    auto step = [&]()
        {
            if (sim.currentStep >= TOTAL_STEPS) return;

            sim.currentStep++;

            sim.processEvents();
            sim.generateVehicles();

            auto dep = sim.moveVehicles();
            sim.updateRoadStates(dep);
            sim.updateSignals();
            sim.releaseFromQueues();
            sim.rerouteWaitingVehicles();
            sim.dispatchWaitingVehicles();

            // Initialize position for newly spawned vehicles
            for (auto& v : sim.vehicles)
            {
                if (!vehiclePos.count(v.id))
                    vehiclePos[v.id] = nodePos[v.source];
            }
        };

    // -------------------------------------------------------
    // MAIN LOOP
    // -------------------------------------------------------
    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        // --- EVENTS ---
        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                    window.close();

                if (key->scancode == sf::Keyboard::Scancode::Space)
                    paused = !paused;

                if (key->scancode == sf::Keyboard::Scancode::Right)
                    step(); // manual step with arrow key
            }
        }

        // --- AUTO STEP ---
        if (!paused)
        {
            accumulator += dt;
            if (accumulator >= stepInterval)
            {
                accumulator = 0.f;
                step();
            }
        }

        // -------------------------------------------------------
        // UPDATE VEHICLE POSITIONS (smooth lerp toward target)
        // -------------------------------------------------------
        for (auto& v : sim.vehicles)
        {
            sf::Vector2f target;

            if (v.status == WAITING || v.status == ARRIVED)
            {
                // Vehicle is at a node — snap to node position
                target = nodePos[v.currentNode];
            }
            else // MOVING
            {
                auto& r = sim.graph.roads[v.currentRoad];

                sf::Vector2f a = nodePos[r.source];
                sf::Vector2f b = nodePos[r.destination];

                // FIX: use entryTravelTime (fixed at road entry)
                // NOT r.travelTime (which changes every step due to congestion update)
                // Old bug: progress = 1 - remaining/r.travelTime → jumps when r.travelTime changes
                // Fix:     progress = 1 - remaining/entryTravelTime → always correct 0->1 range
                float progress = 0.f;
                if (v.entryTravelTime > 0.0)
                    progress = 1.f - (float)(v.remainingTravelTime / v.entryTravelTime);

                if (progress < 0.f) progress = 0.f;
                if (progress > 1.f) progress = 1.f;

                target = a + (b - a) * progress;
            }

            // Smooth lerp toward target
            vehiclePos[v.id] += (target - vehiclePos[v.id]) * min(dt * 5.f, 1.f);
        }

        // -------------------------------------------------------
        // DRAW
        // -------------------------------------------------------
        window.clear(sf::Color(20, 20, 30));

        // --- ROADS ---
        for (auto& r : sim.graph.roads)
        {
            float cong = (r.capacity > 0)
                ? (float)r.currentFlow / r.capacity
                : 0.f;

            sf::Color col = getRoadColor(cong);

            // Override color based on signal state
            if (sim.signals.count(r.destination))
            {
                auto& sig = sim.signals[r.destination];
                if (sig.getSignal(r.id) == 1)
                    col = sf::Color::Green;        // green signal
                else
                    col = sf::Color(150, 0, 0);    // red signal
            }

            drawRoad(window, nodePos[r.source], nodePos[r.destination], col);
        }

        // --- NODES (intersections) ---
        for (auto& n : sim.graph.nodes)
        {
            sf::CircleShape c(18.f);
            c.setOrigin(sf::Vector2f(18.f, 18.f));
            c.setPosition(nodePos[n.first]);
            c.setFillColor(sf::Color(80, 100, 180));
            window.draw(c);
        }

        // --- TRAFFIC SIGNALS ---
        for (auto& kv : sim.signals)
        {
            int nodeId = kv.first;
            auto& signal = kv.second;

            sf::Vector2f base = nodePos[nodeId];

            // Draw pole
            sf::RectangleShape pole(sf::Vector2f(5.f, 30.f));
            pole.setOrigin(sf::Vector2f(2.5f, 15.f));
            pole.setPosition(sf::Vector2f(base.x + 25.f, base.y));
            pole.setFillColor(sf::Color(100, 100, 100));
            window.draw(pole);

            int greenRoad = signal.currentGreenRoad;

            // Draw 3 lights: top=red, mid=yellow, bot=green
            for (int i = 0; i < 3; i++)
            {
                sf::CircleShape light(4.f);
                light.setOrigin(sf::Vector2f(4.f, 4.f));
                light.setPosition(sf::Vector2f(base.x + 25.f, base.y - 10.f + i * 10.f));

                sf::Color col(50, 50, 50); // off by default

                if (i == 0)
                    col = sf::Color::Red;                              // top = always red

                if (i == 1 && signal.greenTimer >= 4)
                    col = sf::Color::Yellow;                           // mid = yellow near switch

                if (i == 2 && greenRoad != -1)
                    col = sf::Color::Green;                            // bot = green when active

                light.setFillColor(col);
                window.draw(light);
            }
        }

        // --- VEHICLES (CARS) ---
        for (auto& v : sim.vehicles)
        {
            if (v.status == ARRIVED) continue;

            sf::Vector2f pos = vehiclePos[v.id];

            // Calculate rotation angle along road direction
            float angle = 0.f;
            if (v.status == MOVING && v.currentRoad >= 0)
            {
                auto& r = sim.graph.roads[v.currentRoad];
                sf::Vector2f d = nodePos[r.destination] - nodePos[r.source];
                angle = std::atan2(d.y, d.x) * 180.f / 3.14159265f;
            }

            // Car body
            sf::RectangleShape body(sf::Vector2f(16.f, 8.f));
            body.setOrigin(sf::Vector2f(8.f, 4.f));
            body.setPosition(pos);
            body.setRotation(sf::degrees(angle));
            body.setFillColor(sf::Color::Cyan);

            // Car roof
            sf::RectangleShape top(sf::Vector2f(10.f, 6.f));
            top.setOrigin(sf::Vector2f(5.f, 3.f));
            top.setPosition(pos);
            top.setRotation(sf::degrees(angle));
            top.setFillColor(sf::Color(0, 150, 200));

            window.draw(body);
            window.draw(top);
        }

        window.display();
    }

    return 0;
}
