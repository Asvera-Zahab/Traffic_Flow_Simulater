#include "Renderer.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>

// ---- construction ----

Renderer::Renderer(unsigned int width, unsigned int height, const std::string& title)
    : window(sf::VideoMode({ width, height }), title),
    fontLoaded(false),
    paused(false),
    speedMultiplier(1.0f) {
    window.setFramerateLimit(60);
    fontLoaded = loadBestAvailableFont();
    if (!fontLoaded) {
        std::cerr << "[Renderer] WARNING: could not load a .ttf font from any known path.\n"
            "[Renderer] Text will not be visible. Place a font at ./assets/DejaVuSans.ttf\n"
            "[Renderer] (or edit loadBestAvailableFont() in Renderer.cpp) to fix this.\n";
    }
}

bool Renderer::loadBestAvailableFont() {
    // Try a bundled font first, then common system locations (Linux/Windows/macOS).
    const std::vector<std::string> candidates = {
        "assets/DejaVuSans.ttf",
        "DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
    for (const auto& path : candidates) {
        if (font.openFromFile(path)) return true;
    }
    return false;
}

sf::Text Renderer::makeText(const std::string& str, unsigned int size, sf::Color color) const {
    sf::Text text(font, str, size);
    text.setFillColor(color);
    return text;
}

bool Renderer::isOpen() const {
    return window.isOpen();
}

void Renderer::close() {
    window.close();
}

// ---- events ----

void Renderer::pollEvents() {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            switch (keyPressed->code) {
            case sf::Keyboard::Key::Escape:
                window.close();
                break;
            case sf::Keyboard::Key::Space:
                paused = !paused;
                break;
            case sf::Keyboard::Key::Equal:      // '+' (unshifted '=' on most layouts)
            case sf::Keyboard::Key::Add:
                speedMultiplier = std::min(4.0f, speedMultiplier + 0.25f);
                break;
            case sf::Keyboard::Key::Hyphen:
            case sf::Keyboard::Key::Subtract:
                speedMultiplier = std::max(0.25f, speedMultiplier - 0.25f);
                break;
            default:
                break;
            }
        }
    }
}

// ---- color scale (single source of truth, used by roads, HUD and legend) ----

sf::Color Renderer::congestionColor(float congestion, bool blocked) const {
    if (blocked) return sf::Color(107, 107, 104);       // gray  - Blocked
    if (congestion < 0.3f) return sf::Color(79, 157, 61);   // green - Free
    if (congestion < 0.7f) return sf::Color(217, 154, 31);  // amber - Moderate
    return sf::Color(201, 68, 67);                            // red   - Congested
}

// ---- low-level drawing helpers ----

void Renderer::drawThickLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

    sf::RectangleShape rect({ len, thickness });
    rect.setOrigin({ 0.f, thickness / 2.f });
    rect.setPosition(a);
    rect.setRotation(sf::degrees(angleDeg));
    rect.setFillColor(color);
    window.draw(rect);

    // rounded caps
    sf::CircleShape capA(thickness / 2.f);
    capA.setOrigin({ thickness / 2.f, thickness / 2.f });
    capA.setPosition(a);
    capA.setFillColor(color);
    window.draw(capA);

    sf::CircleShape capB(thickness / 2.f);
    capB.setOrigin({ thickness / 2.f, thickness / 2.f });
    capB.setPosition(b);
    capB.setFillColor(color);
    window.draw(capB);
}

void Renderer::drawDashedThickLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::Vector2f u = d / len;
    float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

    const float dashLen = 10.f, gapLen = 7.f;
    float travelled = 0.f;
    while (travelled < len) {
        float thisDash = std::min(dashLen, len - travelled);
        sf::Vector2f start = a + u * travelled;
        sf::RectangleShape rect({ thisDash, thickness });
        rect.setOrigin({ 0.f, thickness / 2.f });
        rect.setPosition(start);
        rect.setRotation(sf::degrees(angleDeg));
        rect.setFillColor(color);
        window.draw(rect);
        travelled += dashLen + gapLen;
    }
}

void Renderer::drawThinDashedConnector(sf::Vector2f a, sf::Vector2f b, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::Vector2f u = d / len;
    float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

    const float dashLen = 3.f, gapLen = 4.f, thickness = 1.2f;
    float travelled = 0.f;
    while (travelled < len) {
        float thisDash = std::min(dashLen, len - travelled);
        sf::Vector2f start = a + u * travelled;
        sf::RectangleShape rect({ thisDash, thickness });
        rect.setOrigin({ 0.f, thickness / 2.f });
        rect.setPosition(start);
        rect.setRotation(sf::degrees(angleDeg));
        rect.setFillColor(color);
        window.draw(rect);
        travelled += dashLen + gapLen;
    }
}

void Renderer::drawArrowHead(sf::Vector2f tip, sf::Vector2f dir, sf::Color color) {
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 0.01f) return;
    sf::Vector2f u = dir / len;
    sf::Vector2f perp(-u.y, u.x);

    const float arrowLen = 11.f, arrowWidth = 8.f;
    sf::Vector2f back = tip - u * arrowLen;

    sf::ConvexShape triangle(3);
    triangle.setPoint(0, tip);
    triangle.setPoint(1, back + perp * (arrowWidth / 2.f));
    triangle.setPoint(2, back - perp * (arrowWidth / 2.f));
    triangle.setFillColor(color);
    window.draw(triangle);
}

// ---- background grid ----

void Renderer::drawGrid() {
    const float spacing = 28.f;
    sf::Vector2f off = mapOffset();
    float mapWidth = window.getSize().x - off.x;
    float mapHeight = window.getSize().y - BOTTOM_BAR_HEIGHT;

    sf::CircleShape dot(1.f);
    dot.setFillColor(sf::Color(255, 255, 255, 13)); // ~5% opacity
    for (float y = 0.f; y < mapHeight; y += spacing) {
        for (float x = 0.f; x < mapWidth; x += spacing) {
            dot.setPosition({ off.x + x, y });
            window.draw(dot);
        }
    }
}

// ---- roads ----

void Renderer::drawRoads(const SimSnapshot& snap, const std::map<int, NodeView>& nodeById) {
    for (const RoadView& r : snap.roads) {
        auto itSrc = nodeById.find(r.srcNode);
        auto itDst = nodeById.find(r.dstNode);
        if (itSrc == nodeById.end() || itDst == nodeById.end()) continue;

        sf::Vector2f a = toScreen(itSrc->second.x, itSrc->second.y);
        sf::Vector2f b = toScreen(itDst->second.x, itDst->second.y);
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1.f) continue;
        sf::Vector2f u = d / len;

        sf::Vector2f trimmedA = a + u * TRIM;
        sf::Vector2f trimmedB = b - u * TRIM;
        sf::Color color = congestionColor(r.congestion, r.blocked);

        if (r.blocked) {
            drawDashedThickLine(trimmedA, trimmedB, ROAD_THICKNESS, color);
        }
        else {
            drawThickLine(trimmedA, trimmedB, ROAD_THICKNESS, color);
            drawArrowHead(trimmedB, u, color);
        }

        // label: flow/capacity + queue length, at the midpoint
        sf::Vector2f mid = (trimmedA + trimmedB) / 2.f;
        std::ostringstream oss;
        oss << r.flow << "/" << r.capacity << "  Q:" << r.queueLen;
        if (r.blocked) oss << "  (blocked)";

        sf::Text label = makeText(oss.str(), 13, r.blocked ? sf::Color(140, 140, 134) : sf::Color(200, 200, 195));
        label.setPosition(mid + sf::Vector2f(6.f, -18.f));
        window.draw(label);

        // mini flow bar under the label (skip for blocked roads - value is meaningless)
        if (!r.blocked) {
            float ratio = r.capacity > 0 ? std::clamp((float)r.flow / (float)r.capacity, 0.f, 1.f) : 0.f;
            sf::Vector2f barPos = mid + sf::Vector2f(6.f, 0.f);
            sf::RectangleShape track({ 40.f, 4.f });
            track.setPosition(barPos);
            track.setFillColor(sf::Color(50, 50, 48));
            window.draw(track);

            sf::RectangleShape fill({ 40.f * ratio, 4.f });
            fill.setPosition(barPos);
            fill.setFillColor(color);
            window.draw(fill);
        }
    }
}

// ---- nodes ----

void Renderer::drawNodes(const SimSnapshot& snap) {
    for (const NodeView& n : snap.nodes) {
        sf::Vector2f pos = toScreen(n.x, n.y);

        sf::CircleShape circle(NODE_RADIUS);
        circle.setOrigin({ NODE_RADIUS, NODE_RADIUS });
        circle.setPosition(pos);
        circle.setFillColor(sf::Color(32, 36, 42));
        circle.setOutlineThickness(2.f);
        circle.setOutlineColor(sf::Color(127, 168, 214)); // steel blue
        window.draw(circle);

        sf::Text label = makeText(n.name, 15, sf::Color(220, 220, 217));
        sf::FloatRect bounds = label.getLocalBounds();
        label.setOrigin({ bounds.size.x / 2.f, bounds.size.y / 2.f });
        label.setPosition(pos - sf::Vector2f(0.f, NODE_RADIUS + 14.f));
        window.draw(label);
    }
}

// ---- vehicles ----

void Renderer::drawVehicles(const SimSnapshot& snap,
    const std::map<int, RoadView>& roadById,
    const std::map<int, NodeView>& nodeById) {
    for (const VehicleView& v : snap.vehicles) {
        auto itRoad = roadById.find(v.roadId);
        if (itRoad == roadById.end()) continue;
        const RoadView& r = itRoad->second;

        auto itSrc = nodeById.find(r.srcNode);
        auto itDst = nodeById.find(r.dstNode);
        if (itSrc == nodeById.end() || itDst == nodeById.end()) continue;

        sf::Vector2f a = toScreen(itSrc->second.x, itSrc->second.y);
        sf::Vector2f b = toScreen(itDst->second.x, itDst->second.y);
        float t = std::clamp(v.progress, 0.f, 1.f);
        sf::Vector2f pos = a + (b - a) * t;

        sf::CircleShape dot(VEHICLE_RADIUS);
        dot.setOrigin({ VEHICLE_RADIUS, VEHICLE_RADIUS });
        dot.setPosition(pos);
        dot.setFillColor(sf::Color(242, 242, 239));
        window.draw(dot);
    }
}

// ---- signals: one indicator per incoming road, grouped per intersection ----

void Renderer::drawSignals(const SimSnapshot& snap,
    const std::map<int, RoadView>& roadById,
    const std::map<int, NodeView>& nodeById) {
    // Group incoming road ids by destination node (derived from the road
    // list itself, so EVERY incoming road gets a light, not just the active one).
    std::map<int, std::vector<int>> incomingByNode;
    for (const auto& kv : roadById) {
        incomingByNode[kv.second.dstNode].push_back(kv.first);
    }

    std::map<int, int> activeByNode;
    for (const SignalView& s : snap.signals) activeByNode[s.nodeId] = s.activeRoadId;

    for (auto& kv : incomingByNode) {
        int nodeId = kv.first;
        std::vector<int> roadIds = kv.second;
        std::sort(roadIds.begin(), roadIds.end());
        int activeRoad = activeByNode.count(nodeId) ? activeByNode[nodeId] : -1;

        std::vector<sf::Vector2f> points;
        points.reserve(roadIds.size());

        for (size_t i = 0; i < roadIds.size(); i++) {
            const RoadView& r = roadById.at(roadIds[i]);
            auto itSrc = nodeById.find(r.srcNode);
            auto itDst = nodeById.find(r.dstNode);
            if (itSrc == nodeById.end() || itDst == nodeById.end()) continue;

            sf::Vector2f a = toScreen(itSrc->second.x, itSrc->second.y);
            sf::Vector2f b = toScreen(itDst->second.x, itDst->second.y);
            sf::Vector2f d = b - a;
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len < 1.f) continue;
            sf::Vector2f u = d / len;
            sf::Vector2f perp(-u.y, u.x);

            float t = 0.80f + 0.03f * (float)i;           // fan out along the road
            float side = (i % 2 == 0) ? 1.f : -1.f;         // alternate which side of the road
            sf::Vector2f pos = a + u * (len * t) + perp * (14.f * side);
            points.push_back(pos);

            bool isGreen = (roadIds[i] == activeRoad);
            sf::Color ringColor = r.blocked ? sf::Color(107, 107, 104)
                : (isGreen ? sf::Color(79, 157, 61) : sf::Color(201, 68, 67));

            sf::CircleShape plate(SIGNAL_RADIUS);
            plate.setOrigin({ SIGNAL_RADIUS, SIGNAL_RADIUS });
            plate.setPosition(pos);
            plate.setFillColor(sf::Color(13, 15, 17));
            plate.setOutlineThickness(2.f);
            plate.setOutlineColor(ringColor);
            window.draw(plate);

            sf::CircleShape dot(2.8f);
            dot.setOrigin({ 2.8f, 2.8f });
            dot.setPosition(pos);
            dot.setFillColor(ringColor);
            window.draw(dot);
        }

        // faint connector linking every signal that belongs to the same intersection
        for (size_t i = 1; i < points.size(); i++) {
            drawThinDashedConnector(points[i - 1], points[i], sf::Color(85, 85, 85));
        }
    }
}

// ---- legend (always-visible, top-right, semi-transparent) ----

void Renderer::drawLegend() {
    const float w = 168.f, h = 178.f, pad = 10.f;
    sf::Vector2f pos = { window.getSize().x - w - 10.f, 10.f };

    sf::RectangleShape bg({ w, h });
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(20, 22, 25, 225));
    window.draw(bg);

    struct Row { std::string label; sf::Color color; int kind; }; // kind: 0=square, 1=filled dot, 2=ring
    std::vector<Row> rows = {
        { "Free",         sf::Color(79, 157, 61),  0 },
        { "Moderate",     sf::Color(217, 154, 31), 0 },
        { "Congested",    sf::Color(201, 68, 67),  0 },
        { "Blocked",      sf::Color(107, 107, 104),0 },
        { "Vehicle",      sf::Color(242, 242, 239),1 },
        { "Signal: go",   sf::Color(79, 157, 61),  2 },
        { "Signal: stop", sf::Color(201, 68, 67),  2 },
    };

    float rowH = (h - pad * 2.f) / (float)rows.size();
    for (size_t i = 0; i < rows.size(); i++) {
        float cy = pos.y + pad + rowH * (float)i + rowH / 2.f;
        sf::Vector2f iconPos = { pos.x + pad + 5.f, cy };

        if (rows[i].kind == 0) {
            sf::RectangleShape sq({ 10.f, 10.f });
            sq.setOrigin({ 5.f, 5.f });
            sq.setPosition(iconPos);
            sq.setFillColor(rows[i].color);
            window.draw(sq);
        }
        else if (rows[i].kind == 1) {
            sf::CircleShape dot(4.f);
            dot.setOrigin({ 4.f, 4.f });
            dot.setPosition(iconPos);
            dot.setFillColor(rows[i].color);
            window.draw(dot);
        }
        else {
            sf::CircleShape ring(5.f);
            ring.setOrigin({ 5.f, 5.f });
            ring.setPosition(iconPos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(2.f);
            ring.setOutlineColor(rows[i].color);
            window.draw(ring);
        }

        sf::Text label = makeText(rows[i].label, 13, sf::Color(230, 230, 227));
        label.setPosition({ pos.x + pad + 18.f, cy - 9.f });
        window.draw(label);
    }
}

// ---- HUD (fixed left sidebar) ----

void Renderer::drawHud(const SimSnapshot& snap) {
    sf::RectangleShape bg({ LEFT_PANEL_WIDTH, (float)window.getSize().y - BOTTOM_BAR_HEIGHT });
    bg.setPosition({ 0.f, 0.f });
    bg.setFillColor(sf::Color(10, 12, 14, 235));
    window.draw(bg);

    float x = 16.f, y = 16.f;
    auto line = [&](const std::string& s, unsigned int size, sf::Color color, float gap) {
        sf::Text t = makeText(s, size, color);
        t.setPosition({ x, y });
        window.draw(t);
        y += gap;
        };

    line("Step: " + std::to_string(snap.step), 22, sf::Color(240, 240, 237), 40.f);
    line("Moving: " + std::to_string(snap.movingCount), 15, sf::Color(200, 200, 197), 26.f);
    line("Waiting: " + std::to_string(snap.waitingCount), 15, sf::Color(200, 200, 197), 26.f);
    line("Arrived: " + std::to_string(snap.arrivedCount), 15, sf::Color(200, 200, 197), 26.f);
    line("Generated: " + std::to_string(snap.generatedCount), 15, sf::Color(200, 200, 197), 34.f);

    std::ostringstream congStr;
    congStr << "Avg congestion: " << std::fixed << std::setprecision(0) << (snap.avgCongestion * 100.f) << "%";
    line(congStr.str(), 15, congestionColor(snap.avgCongestion, false), 34.f);

    line(paused ? "|| PAUSED" : "> RUNNING", 15, paused ? sf::Color(217, 154, 31) : sf::Color(143, 214, 122), 26.f);

    std::ostringstream speedStr;
    speedStr << "Speed: " << std::fixed << std::setprecision(2) << speedMultiplier << "x";
    line(speedStr.str(), 15, sf::Color(200, 200, 197), 26.f);
}

// ---- bottom controls strip ----

void Renderer::drawControlsStrip() {
    float y = (float)window.getSize().y - BOTTOM_BAR_HEIGHT;
    sf::RectangleShape bg({ (float)window.getSize().x, BOTTOM_BAR_HEIGHT });
    bg.setPosition({ 0.f, y });
    bg.setFillColor(sf::Color(10, 12, 14, 235));
    window.draw(bg);

    sf::Text t = makeText("[SPACE] Pause/Resume    [+/-] Speed    [ESC] Quit", 14, sf::Color(190, 190, 186));
    sf::FloatRect bounds = t.getLocalBounds();
    t.setOrigin({ bounds.size.x / 2.f, bounds.size.y / 2.f });
    t.setPosition({ (float)window.getSize().x / 2.f, y + BOTTOM_BAR_HEIGHT / 2.f });
    window.draw(t);
}

// ---- top-level render ----

void Renderer::render(const SimSnapshot& snapshot) {
    std::map<int, NodeView> nodeById;
    for (const NodeView& n : snapshot.nodes) nodeById[n.id] = n;

    std::map<int, RoadView> roadById;
    for (const RoadView& r : snapshot.roads) roadById[r.id] = r;

    window.clear(sf::Color(20, 23, 26));

    drawGrid();
    drawRoads(snapshot, nodeById);
    drawVehicles(snapshot, roadById, nodeById);
    drawSignals(snapshot, roadById, nodeById);
    drawNodes(snapshot);

    drawLegend();
    drawHud(snapshot);
    drawControlsStrip();

    window.display();
}