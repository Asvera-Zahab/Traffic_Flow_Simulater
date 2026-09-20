#include "Renderer.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>

// ---- construction ----

Renderer::Renderer(unsigned int width, unsigned int height, const std::string& title)
    : window(sf::VideoMode({ width, height }), title),
    fontLoaded(false),
    paused(false),
    speedMultiplier(1.0f) {
    // setFramerateLimit + vsync fight each other and can cause visible
    // stutter; pick vsync (matches the monitor's own refresh) and skip
    // the manual frame cap.
    window.setVerticalSyncEnabled(true);
    fontLoaded = loadBestAvailableFont();
    if (!fontLoaded) {
        std::cerr << "[Renderer] WARNING: could not load a .ttf font from any known path.\n"
            "[Renderer] All text will be skipped this run (shapes still draw normally).\n"
            "[Renderer] Fix: drop any .ttf file at ./assets/DejaVuSans.ttf next to the exe.\n";
    }
    generateBackgroundDecor();
}

bool Renderer::loadBestAvailableFont() {
    // Project-local fonts first (portable across machines if you ship
    // one), then the common install locations on each OS. Checked with
    // a wide net so this doesn't silently fail on machines that only
    // have some of these installed.
    const std::vector<std::string> candidates = {
        "assets/DejaVuSans.ttf",
        "assets/Arial.ttf",
        "DejaVuSans.ttf",
        // Windows
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        // macOS
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    };
    for (const auto& path : candidates) {
        if (font.openFromFile(path)) return true;
    }
    return false;
}

void Renderer::generateBackgroundDecor() {
    // Fixed seed -> same "grass texture" every run, computed once, drawn every frame.
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> xDist(0.f, (float)window.getSize().x);
    std::uniform_real_distribution<float> yDist(0.f, (float)window.getSize().y);
    std::uniform_real_distribution<float> rDist(3.f, 9.f);

    const int count = 140;
    decorPositions.reserve(count);
    decorRadii.reserve(count);
    for (int i = 0; i < count; i++) {
        decorPositions.push_back({ xDist(rng), yDist(rng) });
        decorRadii.push_back(rDist(rng));
    }
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
            case sf::Keyboard::Key::Equal:
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

// ---- color scale (single source of truth: roads, stats box, legend) ----

sf::Color Renderer::congestionColor(float congestion, bool blocked) const {
    if (blocked) return sf::Color(120, 120, 116);
    if (congestion < 0.3f) return sf::Color(70, 170, 80);
    if (congestion < 0.7f) return sf::Color(230, 180, 40);
    return sf::Color(210, 60, 55);
}

sf::RectangleShape Renderer::roundedLabelBox(sf::Vector2f center, sf::Vector2f size, sf::Color fill) const {
    // SFML has no built-in rounded rect; a plain dark rect reads the same
    // at this scale and keeps the drawing code simple.
    sf::RectangleShape box(size);
    box.setOrigin({ size.x / 2.f, size.y / 2.f });
    box.setPosition(center);
    box.setFillColor(fill);
    return box;
}

// ---- background: grass + scattered bushes ----

void Renderer::drawBackground() {
    sf::RectangleShape grass((sf::Vector2f)window.getSize());
    grass.setPosition({ 0.f, 0.f });
    grass.setFillColor(sf::Color(58, 97, 45));
    window.draw(grass);

    for (size_t i = 0; i < decorPositions.size(); i++) {
        sf::CircleShape bush(decorRadii[i]);
        bush.setOrigin({ decorRadii[i], decorRadii[i] });
        bush.setPosition(decorPositions[i]);
        bush.setFillColor(sf::Color(45, 82, 38, 160));
        window.draw(bush);
    }
}

// ---- road surface + dashed lane line ----

void Renderer::drawRoadSurface(sf::Vector2f a, sf::Vector2f b, float thickness) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

    // dark border first, so the road reads as a crisp, prominent shape
    // against the grass instead of blending into it
    const float borderExtra = 6.f;
    sf::RectangleShape border({ len, thickness + borderExtra });
    border.setOrigin({ 0.f, (thickness + borderExtra) / 2.f });
    border.setPosition(a);
    border.setRotation(sf::degrees(angleDeg));
    border.setFillColor(sf::Color(28, 30, 28));
    window.draw(border);

    sf::CircleShape borderCapA((thickness + borderExtra) / 2.f), borderCapB((thickness + borderExtra) / 2.f);
    borderCapA.setOrigin({ (thickness + borderExtra) / 2.f, (thickness + borderExtra) / 2.f });
    borderCapB.setOrigin({ (thickness + borderExtra) / 2.f, (thickness + borderExtra) / 2.f });
    borderCapA.setPosition(a);
    borderCapB.setPosition(b);
    borderCapA.setFillColor(sf::Color(28, 30, 28));
    borderCapB.setFillColor(sf::Color(28, 30, 28));
    window.draw(borderCapA);
    window.draw(borderCapB);

    // asphalt on top of the border
    sf::RectangleShape rect({ len, thickness });
    rect.setOrigin({ 0.f, thickness / 2.f });
    rect.setPosition(a);
    rect.setRotation(sf::degrees(angleDeg));
    rect.setFillColor(sf::Color(84, 88, 92));
    window.draw(rect);

    sf::CircleShape capA(thickness / 2.f), capB(thickness / 2.f);
    capA.setOrigin({ thickness / 2.f, thickness / 2.f });
    capB.setOrigin({ thickness / 2.f, thickness / 2.f });
    capA.setPosition(a);
    capB.setPosition(b);
    capA.setFillColor(sf::Color(84, 88, 92));
    capB.setFillColor(sf::Color(84, 88, 92));
    window.draw(capA);
    window.draw(capB);

    // solid edge lines (like painted road shoulders) for a crisper, more
    // deliberate "this is a road" read than a flat gray band alone
    sf::Vector2f u = d / len;
    sf::Vector2f perp(-u.y, u.x);
    float edgeOffset = thickness / 2.f - 3.f;
    for (float side : { -1.f, 1.f }) {
        sf::RectangleShape edge({ len, 1.6f });
        edge.setOrigin({ 0.f, 0.8f });
        edge.setPosition(a + perp * (edgeOffset * side));
        edge.setRotation(sf::degrees(angleDeg));
        edge.setFillColor(sf::Color(210, 210, 200, 150));
        window.draw(edge);
    }
}

void Renderer::drawJunctionPad(sf::Vector2f pos, float thickness) {
    // A single disc under each junction so multiple road end-caps meeting
    // at the same point blend into one clean surface instead of visible seams.
    const float borderExtra = 6.f;
    sf::CircleShape border((thickness + borderExtra) / 2.f);
    border.setOrigin({ (thickness + borderExtra) / 2.f, (thickness + borderExtra) / 2.f });
    border.setPosition(pos);
    border.setFillColor(sf::Color(28, 30, 28));
    window.draw(border);

    sf::CircleShape pad(thickness / 2.f);
    pad.setOrigin({ thickness / 2.f, thickness / 2.f });
    pad.setPosition(pos);
    pad.setFillColor(sf::Color(84, 88, 92));
    window.draw(pad);
}

void Renderer::drawDashedLaneLine(sf::Vector2f a, sf::Vector2f b) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::Vector2f u = d / len;
    float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

    const float dashLen = 14.f, gapLen = 10.f, laneWidth = 3.f;
    float travelled = 0.f;
    while (travelled < len) {
        float thisDash = std::min(dashLen, len - travelled);
        sf::Vector2f start = a + u * travelled;
        sf::RectangleShape rect({ thisDash, laneWidth });
        rect.setOrigin({ 0.f, laneWidth / 2.f });
        rect.setPosition(start);
        rect.setRotation(sf::degrees(angleDeg));
        rect.setFillColor(sf::Color(235, 235, 230, 210));
        window.draw(rect);
        travelled += dashLen + gapLen;
    }
}

void Renderer::drawDashedThickLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::Vector2f u = d / len;
    float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

    const float dashLen = 16.f, gapLen = 10.f;
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

void Renderer::drawArrowHead(sf::Vector2f tip, sf::Vector2f dir, sf::Color color, float size) {
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 0.01f) return;
    sf::Vector2f u = dir / len;
    sf::Vector2f perp(-u.y, u.x);
    sf::Vector2f back = tip - u * size;

    sf::ConvexShape triangle(3);
    triangle.setPoint(0, tip);
    triangle.setPoint(1, back + perp * (size * 0.7f));
    triangle.setPoint(2, back - perp * (size * 0.7f));
    triangle.setFillColor(color);
    window.draw(triangle);
}

// ---- title box (top-left): "{n} Roads | {n} Junctions" ----

void Renderer::drawTitleBox(const SimSnapshot& snap) {
    std::ostringstream oss;
    oss << snap.roads.size() << " Roads   |   " << snap.nodes.size() << " Junctions";
    sf::Text t = makeText(oss.str(), 17, sf::Color(235, 235, 232));
    sf::FloatRect b = t.getLocalBounds();

    sf::Vector2f size(b.size.x + 32.f, 40.f);
    sf::Vector2f center(20.f + size.x / 2.f, 16.f + size.y / 2.f);
    window.draw(roundedLabelBox(center, size, sf::Color(18, 32, 46, 235)));

    t.setOrigin({ b.size.x / 2.f, b.size.y / 2.f + b.position.y });
    t.setPosition(center);
    window.draw(t);
}

// ---- stats box (top-right): live simulation metrics ----

void Renderer::drawStatsBox(const SimSnapshot& snap) {
    const float w = 190.f, h = 168.f, pad = 12.f;
    sf::Vector2f pos = { window.getSize().x - w - 16.f, 16.f };
    sf::RectangleShape bg({ w, h });
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(18, 32, 46, 225));
    window.draw(bg);

    float x = pos.x + pad, y = pos.y + pad;
    auto line = [&](const std::string& s, unsigned int size, sf::Color color, float gap) {
        sf::Text t = makeText(s, size, color);
        t.setPosition({ x, y });
        window.draw(t);
        y += gap;
        };

    line("Step: " + std::to_string(snap.step), 18, sf::Color(240, 240, 237), 28.f);
    line("Moving: " + std::to_string(snap.movingCount), 13, sf::Color(210, 210, 206), 19.f);
    line("Waiting: " + std::to_string(snap.waitingCount), 13, sf::Color(210, 210, 206), 19.f);
    line("Arrived: " + std::to_string(snap.arrivedCount), 13, sf::Color(210, 210, 206), 19.f);
    line("Generated: " + std::to_string(snap.generatedCount), 13, sf::Color(210, 210, 206), 24.f);

    std::ostringstream congStr;
    congStr << "Avg congestion: " << std::fixed << std::setprecision(0) << (snap.avgCongestion * 100.f) << "%";
    line(congStr.str(), 13, congestionColor(snap.avgCongestion, false), 22.f);

    line(paused ? "|| PAUSED" : "> RUNNING", 13, paused ? sf::Color(230, 180, 40) : sf::Color(150, 220, 130), 19.f);

    std::ostringstream speedStr;
    speedStr << "Speed: " << std::fixed << std::setprecision(2) << speedMultiplier << "x";
    line(speedStr.str(), 13, sf::Color(210, 210, 206), 19.f);
}

// ---- roads ----

void Renderer::drawRoads(const SimSnapshot& snap, const std::map<int, NodeView>& nodeById) {
    // Pass 1: asphalt + dashed lane lines for every road (drawn first so
    // labels/arrows/signals always sit on top and stay legible).
    for (const RoadView& r : snap.roads) {
        auto itSrc = nodeById.find(r.srcNode);
        auto itDst = nodeById.find(r.dstNode);
        if (itSrc == nodeById.end() || itDst == nodeById.end()) continue;

        sf::Vector2f a = toScreen(itSrc->second.x, itSrc->second.y);
        sf::Vector2f b = toScreen(itDst->second.x, itDst->second.y);

        if (r.blocked) {
            drawDashedThickLine(a, b, ROAD_THICKNESS, sf::Color(90, 90, 86));
        }
        else {
            drawRoadSurface(a, b, ROAD_THICKNESS);
            drawDashedLaneLine(a, b);
        }
    }

    // Junction pads: one clean disc per node so multiple roads meeting at
    // the same point blend seamlessly instead of showing jagged overlaps.
    for (const NodeView& n : snap.nodes) {
        drawJunctionPad(toScreen(n.x, n.y), ROAD_THICKNESS);
    }

    // Pass 2: direction arrow + flow label + mini bar for every road.
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
        sf::Color color = congestionColor(r.congestion, r.blocked);

        if (!r.blocked) {
            sf::Vector2f arrowTip = a + u * (len * 0.5f + 12.f);
            drawArrowHead(arrowTip, u, sf::Color(240, 240, 236), 10.f);
        }

        sf::Vector2f mid = (a + b) / 2.f;
        std::ostringstream oss;
        oss << "Road " << r.id << " (" << r.flow << "/" << r.capacity << " * Q:" << r.queueLen << ")";
        if (r.blocked) oss << " BLOCKED";

        sf::Text label = makeText(oss.str(), 12, r.blocked ? sf::Color(190, 190, 185) : sf::Color(20, 22, 24));
        sf::FloatRect lb = label.getLocalBounds();
        label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
        sf::Vector2f labelBg = mid - u * 0.f; // keep at midpoint, offset perpendicular below
        sf::Vector2f perp(-u.y, u.x);
        sf::Vector2f labelPos = mid + perp * 20.f;
        window.draw(roundedLabelBox(labelPos, { lb.size.x + 16.f, 20.f }, sf::Color(18, 32, 46, 220)));
        label.setPosition(labelPos);
        label.setFillColor(sf::Color(235, 235, 232));
        window.draw(label);
    }
}

// ---- external stub roads: nodes with no incoming or no outgoing road ----
// (derived purely from the road list -- a node with zero incoming roads is
//  a true network entry point, zero outgoing is a true exit point)

void Renderer::drawExternalStubs(const SimSnapshot& snap, const std::map<int, NodeView>& nodeById) {
    std::map<int, int> incomingCount, outgoingCount;
    for (const RoadView& r : snap.roads) {
        outgoingCount[r.srcNode]++;
        incomingCount[r.dstNode]++;
    }

    for (const NodeView& n : snap.nodes) {
        sf::Vector2f pos = toScreen(n.x, n.y);
        bool hasIncoming = incomingCount.count(n.id) && incomingCount[n.id] > 0;
        bool hasOutgoing = outgoingCount.count(n.id) && outgoingCount[n.id] > 0;

        if (!hasIncoming) {
            sf::Vector2f tip = pos - sf::Vector2f(0.f, STUB_LENGTH);
            drawRoadSurface(tip, pos, STUB_ROAD_THICKNESS);
            drawDashedLaneLine(tip, pos);
            drawArrowHead(pos - sf::Vector2f(0.f, STUB_LENGTH * 0.35f), { 0.f, 1.f }, sf::Color(240, 240, 236), 10.f);

            sf::Text label = makeText("External Inflow", 12, sf::Color(235, 235, 232));
            sf::FloatRect lb = label.getLocalBounds();
            label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
            sf::Vector2f labelPos = tip - sf::Vector2f(0.f, 16.f);
            window.draw(roundedLabelBox(labelPos, { lb.size.x + 16.f, 22.f }, sf::Color(18, 32, 46, 230)));
            label.setPosition(labelPos);
            window.draw(label);
        }

        if (!hasOutgoing) {
            sf::Vector2f tip = pos + sf::Vector2f(0.f, STUB_LENGTH);
            drawRoadSurface(pos, tip, STUB_ROAD_THICKNESS);
            drawDashedLaneLine(pos, tip);
            drawArrowHead(pos + sf::Vector2f(0.f, STUB_LENGTH * 0.9f), { 0.f, 1.f }, sf::Color(240, 240, 236), 10.f);

            sf::Text label = makeText("External Outflow", 12, sf::Color(235, 235, 232));
            sf::FloatRect lb = label.getLocalBounds();
            label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
            sf::Vector2f labelPos = tip + sf::Vector2f(0.f, 16.f);
            window.draw(roundedLabelBox(labelPos, { lb.size.x + 16.f, 22.f }, sf::Color(18, 32, 46, 230)));
            label.setPosition(labelPos);
            window.draw(label);
        }
    }
}

// ---- junction labels ----

void Renderer::drawJunctionLabels(const SimSnapshot& snap) {
    for (const NodeView& n : snap.nodes) {
        sf::Vector2f pos = toScreen(n.x, n.y);
        std::string text = n.name.empty() ? ("Junction " + std::to_string(n.id)) : n.name;

        sf::Text label = makeText(text, 14, sf::Color(235, 235, 232));
        sf::FloatRect lb = label.getLocalBounds();
        sf::Vector2f size(lb.size.x + 20.f, 24.f);
        window.draw(roundedLabelBox(pos, size, sf::Color(18, 32, 46, 235)));

        label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
        label.setPosition(pos);
        window.draw(label);
    }
}

// ---- vehicles: black dots on the road ----

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
        dot.setFillColor(sf::Color(15, 15, 15));
        window.draw(dot);
    }
}

// ---- signals: one square per incoming road, not just the active one ----
// NOTE: TrafficSignal.h only models two states (green=1 / red=0). If you
// later add a yellow "transition" phase to the backend, plug its color in
// here (e.g. check an `amberRoadIds` set from the snapshot before falling
// back to the red/green check below).

void Renderer::drawSignals(const SimSnapshot& snap,
    const std::map<int, RoadView>& roadById,
    const std::map<int, NodeView>& nodeById) {
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

            // Anchor each signal a FIXED distance back from the destination
            // node (not a fraction of road length) so short roads (e.g. a
            // busy junction with several short incoming roads) still get
            // clean separation instead of every diamond crowding the label.
            float distBack = 46.f + 20.f * (float)i;
            if (distBack > len - 12.f) distBack = std::max(12.f, len * 0.5f);
            float side = (i % 2 == 0) ? 1.f : -1.f;
            sf::Vector2f pos = b - u * distBack + perp * (ROAD_THICKNESS * 0.6f * side);

            bool isGreen = (roadIds[i] == activeRoad);
            sf::Color fill = r.blocked ? sf::Color(120, 120, 116)
                : (isGreen ? sf::Color(60, 170, 70) : sf::Color(205, 55, 50));

            float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;
            sf::RectangleShape square({ SIGNAL_SIZE, SIGNAL_SIZE });
            square.setOrigin({ SIGNAL_SIZE / 2.f, SIGNAL_SIZE / 2.f });
            square.setPosition(pos);
            square.setRotation(sf::degrees(angleDeg + 45.f)); // diamond orientation, like the reference
            square.setFillColor(fill);
            square.setOutlineThickness(2.f);
            square.setOutlineColor(sf::Color(15, 15, 15));
            window.draw(square);
        }
    }
}

// ---- legend (bottom-left, always visible) ----

void Renderer::drawLegend(const SimSnapshot&) {
    const float w = 250.f, h = 148.f, pad = 12.f;
    sf::Vector2f pos = { 16.f, window.getSize().y - h - BOTTOM_BAR_HEIGHT - 16.f };

    sf::RectangleShape bg({ w, h });
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(18, 32, 46, 230));
    window.draw(bg);

    sf::Text title = makeText("Legend", 15, sf::Color(240, 240, 237));
    title.setPosition({ pos.x + pad, pos.y + 8.f });
    window.draw(title);

    float x = pos.x + pad, y = pos.y + 36.f, rowH = 22.f;

    // = Car on road
    sf::CircleShape carDot(4.f);
    carDot.setOrigin({ 4.f, 4.f });
    carDot.setPosition({ x + 5.f, y + 8.f });
    carDot.setFillColor(sf::Color(15, 15, 15));
    window.draw(carDot);
    window.draw([&] { auto t = makeText("= Car on road", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 22.f, y }); return t; }());
    y += rowH;

    // -> Incoming road
    drawArrowHead({ x + 16.f, y + 8.f }, { 1.f, 0.f }, sf::Color(230, 230, 226), 8.f);
    window.draw([&] { auto t = makeText("= Incoming road", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 22.f, y }); return t; }());
    y += rowH;

    // <- Outgoing road
    drawArrowHead({ x, y + 8.f }, { -1.f, 0.f }, sf::Color(230, 230, 226), 8.f);
    window.draw([&] { auto t = makeText("= Outgoing road", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 22.f, y }); return t; }());
    y += rowH;

    // signal squares: green + red, one per incoming road at a junction
    sf::RectangleShape goSq({ 12.f, 12.f });
    goSq.setOrigin({ 6.f, 6.f });
    goSq.setPosition({ x + 6.f, y + 6.f });
    goSq.setRotation(sf::degrees(45.f));
    goSq.setFillColor(sf::Color(60, 170, 70));
    window.draw(goSq);
    window.draw([&] { auto t = makeText("= Signal: Go", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 22.f, y }); return t; }());
    y += rowH;

    sf::RectangleShape stopSq({ 12.f, 12.f });
    stopSq.setOrigin({ 6.f, 6.f });
    stopSq.setPosition({ x + 6.f, y + 6.f });
    stopSq.setRotation(sf::degrees(45.f));
    stopSq.setFillColor(sf::Color(205, 55, 50));
    window.draw(stopSq);
    window.draw([&] { auto t = makeText("= Signal: Stop  (one shown per incoming road)", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 22.f, y }); return t; }());
}

// ---- compass (bottom-right) ----

void Renderer::drawCompass() {
    sf::Vector2f base = { window.getSize().x - 46.f, window.getSize().y - BOTTOM_BAR_HEIGHT - 46.f };

    sf::ConvexShape arrow(3);
    arrow.setPoint(0, { base.x, base.y - 16.f });
    arrow.setPoint(1, { base.x - 8.f, base.y + 8.f });
    arrow.setPoint(2, { base.x + 8.f, base.y + 8.f });
    arrow.setFillColor(sf::Color(240, 240, 236));
    window.draw(arrow);

    sf::Text n = makeText("N", 13, sf::Color(240, 240, 236));
    sf::FloatRect b = n.getLocalBounds();
    n.setOrigin({ b.size.x / 2.f, b.size.y / 2.f + b.position.y });
    n.setPosition({ base.x, base.y + 20.f });
    window.draw(n);
}

// ---- bottom controls strip ----

void Renderer::drawControlsStrip() {
    float y = (float)window.getSize().y - BOTTOM_BAR_HEIGHT;
    sf::RectangleShape bg({ (float)window.getSize().x, BOTTOM_BAR_HEIGHT });
    bg.setPosition({ 0.f, y });
    bg.setFillColor(sf::Color(10, 12, 14, 210));
    window.draw(bg);

    sf::Text t = makeText("[SPACE] Pause/Resume    [+/-] Speed    [ESC] Quit", 13, sf::Color(220, 220, 216));
    sf::FloatRect bounds = t.getLocalBounds();
    t.setOrigin({ bounds.size.x / 2.f, bounds.size.y / 2.f + bounds.position.y });
    t.setPosition({ (float)window.getSize().x / 2.f, y + BOTTOM_BAR_HEIGHT / 2.f });
    window.draw(t);
}

// ---- top-level render ----

void Renderer::render(const SimSnapshot& snapshot) {
    std::map<int, NodeView> nodeById;
    for (const NodeView& n : snapshot.nodes) nodeById[n.id] = n;

    std::map<int, RoadView> roadById;
    for (const RoadView& r : snapshot.roads) roadById[r.id] = r;

    window.clear(sf::Color(58, 97, 45));
    drawBackground();
    drawExternalStubs(snapshot, nodeById);
    drawRoads(snapshot, nodeById);
    drawVehicles(snapshot, roadById, nodeById);
    drawSignals(snapshot, roadById, nodeById);
    drawJunctionLabels(snapshot);

    drawTitleBox(snapshot);
    drawStatsBox(snapshot);
    drawLegend(snapshot);
    drawCompass();
    drawControlsStrip();

    window.display();
}