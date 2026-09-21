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

bool Renderer::isOpen() const { return window.isOpen(); }
void Renderer::close() { window.close(); }

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

// ---- colors ----

sf::Color Renderer::congestionColor(float congestion, bool blocked) const {
    if (blocked) return sf::Color(214, 60, 60); // reference always shows a blocked road as red-dashed
    if (congestion < 0.3f) return sf::Color(60, 190, 100);
    if (congestion < 0.7f) return sf::Color(230, 180, 40);
    return sf::Color(214, 60, 60);
}

sf::Color Renderer::nodeColor(const std::string& name, int id) const {
    static const std::map<std::string, sf::Color> known = {
        { "Islamabad", sf::Color(70, 130, 220) },
        { "Karachi",   sf::Color(214, 64, 64) },
        { "Lahore",    sf::Color(230, 180, 40) },
        { "Murree",    sf::Color(150, 90, 205) },
        { "Kashmir",   sf::Color(70, 190, 110) },
    };
    auto it = known.find(name);
    if (it != known.end()) return it->second;

    static const sf::Color palette[] = {
        sf::Color(70, 130, 220), sf::Color(214, 64, 64), sf::Color(230, 180, 40),
        sf::Color(150, 90, 205), sf::Color(70, 190, 110), sf::Color(220, 130, 60)
    };
    return palette[((id % 6) + 6) % 6];
}

sf::RectangleShape Renderer::roundedLabelBox(sf::Vector2f center, sf::Vector2f size, sf::Color fill) const {
    sf::RectangleShape box(size);
    box.setOrigin({ size.x / 2.f, size.y / 2.f });
    box.setPosition(center);
    box.setFillColor(fill);
    return box;
}

// ---- background: plain dark navy, like the reference ----

void Renderer::drawBackground() {
    sf::RectangleShape bg((sf::Vector2f)window.getSize());
    bg.setPosition({ 0.f, 0.f });
    bg.setFillColor(sf::Color(16, 22, 34));
    window.draw(bg);
}

// ---- thin line helpers ----

void Renderer::drawLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
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

    sf::CircleShape capA(thickness / 2.f), capB(thickness / 2.f);
    capA.setOrigin({ thickness / 2.f, thickness / 2.f });
    capB.setOrigin({ thickness / 2.f, thickness / 2.f });
    capA.setPosition(a);
    capB.setPosition(b);
    capA.setFillColor(color);
    capB.setFillColor(color);
    window.draw(capA);
    window.draw(capB);
}

void Renderer::drawDashedLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::Vector2f u = d / len;
    float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

    const float dashLen = 12.f, gapLen = 8.f;
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
    triangle.setPoint(1, back + perp * (size * 0.65f));
    triangle.setPoint(2, back - perp * (size * 0.65f));
    triangle.setFillColor(color);
    window.draw(triangle);
}

// ---- signal indicator: a small box laid directly ON the road, like a
// stop line, colored red/green and labeled with its road id so it's
// unambiguous which road it belongs to.
// NOTE: TrafficSignal.h only models two states (green=1 / red=0) -- this
// is a straight red/green box, no fabricated amber state.

void Renderer::drawSignalBox(sf::Vector2f pos, float angleDeg, int roadId, bool green, bool blocked) {
    sf::Color fill = blocked ? sf::Color(120, 120, 116) : (green ? sf::Color(60, 190, 100) : sf::Color(214, 60, 60));

    sf::RectangleShape box({ SIGNAL_BOX_W, SIGNAL_BOX_H });
    box.setOrigin({ SIGNAL_BOX_W / 2.f, SIGNAL_BOX_H / 2.f });
    box.setPosition(pos);
    box.setRotation(sf::degrees(angleDeg)); // laid across the lane, following the road's own angle
    box.setFillColor(fill);
    box.setOutlineThickness(2.f);
    box.setOutlineColor(sf::Color(15, 15, 15));
    window.draw(box);

    // Road id label, kept upright (not rotated) so it stays readable
    // regardless of the road's angle.
    sf::Text label = makeText("R" + std::to_string(roadId), 10, sf::Color(255, 255, 255));
    sf::FloatRect lb = label.getLocalBounds();
    label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
    label.setPosition(pos);
    window.draw(label);
}

// ---- title box (top-left): "{n} Roads | {n} Junctions" ----

void Renderer::drawTitleBox(const SimSnapshot& snap) {
    std::ostringstream oss;
    oss << snap.roads.size() << " Roads   |   " << snap.nodes.size() << " Junctions";
    sf::Text t = makeText(oss.str(), 17, sf::Color(235, 235, 232));
    sf::FloatRect b = t.getLocalBounds();

    sf::Vector2f size(b.size.x + 32.f, 40.f);
    sf::Vector2f center(20.f + size.x / 2.f, 16.f + size.y / 2.f);
    window.draw(roundedLabelBox(center, size, sf::Color(24, 30, 44, 235)));

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
    bg.setFillColor(sf::Color(24, 30, 44, 225));
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

        // The line runs center-to-center; the ring's own fill (same color
        // as the background) is drawn on top afterward and cleanly covers
        // the segment inside it, so the road visually terminates at the
        // ring's outline with no extra trimming needed here.
        if (r.blocked) {
            drawDashedLine(a, b, ROAD_THICKNESS, color);
        }
        else {
            drawLine(a, b, ROAD_THICKNESS, color);

            // Small, dim, static direction indicator -- deliberately NOT
            // bright/white so it can't be mistaken for a moving car dot.
            sf::Vector2f arrowTip = b - u * (RING_RADIUS + 8.f);
            drawArrowHead(arrowTip, u, sf::Color(140, 145, 150, 200), 7.f);
        }

        sf::Vector2f mid = (a + b) / 2.f;
        std::ostringstream oss;
        oss << "Road " << r.id << "  |  Cars: " << r.flow << " / " << r.capacity << " max  |  Q:" << r.queueLen;
        if (r.blocked) oss << "  BLOCKED";

        sf::Text label = makeText(oss.str(), 12, sf::Color(225, 225, 220));
        sf::FloatRect lb = label.getLocalBounds();
        sf::Vector2f perp(-u.y, u.x);
        // Always on the SAME side (perp+) for every road, so it never
        // shares space with the signal dots, which always sit on perp-.
        sf::Vector2f labelPos = mid + perp * 18.f;
        window.draw(roundedLabelBox(labelPos, { lb.size.x + 16.f, 20.f }, sf::Color(24, 30, 44, 220)));
        label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
        label.setPosition(labelPos);
        window.draw(label);
    }
}

// ---- external stub roads: nodes with no incoming or no outgoing road ----

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
            drawLine(tip, pos - sf::Vector2f(0.f, RING_RADIUS + 4.f), ROAD_THICKNESS, sf::Color(160, 165, 170));
            drawArrowHead(pos - sf::Vector2f(0.f, RING_RADIUS + 8.f), { 0.f, 1.f }, sf::Color(235, 235, 230), 9.f);

            sf::Text label = makeText("External Inflow", 12, sf::Color(230, 230, 226));
            sf::FloatRect lb = label.getLocalBounds();
            sf::Vector2f labelPos = tip - sf::Vector2f(0.f, 16.f);
            window.draw(roundedLabelBox(labelPos, { lb.size.x + 16.f, 22.f }, sf::Color(24, 30, 44, 230)));
            label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
            label.setPosition(labelPos);
            window.draw(label);
        }

        if (!hasOutgoing) {
            sf::Vector2f tip = pos + sf::Vector2f(0.f, STUB_LENGTH);
            drawLine(pos + sf::Vector2f(0.f, RING_RADIUS + 4.f), tip, ROAD_THICKNESS, sf::Color(160, 165, 170));
            drawArrowHead(pos + sf::Vector2f(0.f, STUB_LENGTH * 0.85f), { 0.f, 1.f }, sf::Color(235, 235, 230), 9.f);

            sf::Text label = makeText("External Outflow", 12, sf::Color(230, 230, 226));
            sf::FloatRect lb = label.getLocalBounds();
            sf::Vector2f labelPos = tip + sf::Vector2f(0.f, 16.f);
            window.draw(roundedLabelBox(labelPos, { lb.size.x + 16.f, 22.f }, sf::Color(24, 30, 44, 230)));
            label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
            label.setPosition(labelPos);
            window.draw(label);
        }
    }
}

// ---- junction labels: hollow colored ring + name pill above it ----

void Renderer::drawJunctionLabels(const SimSnapshot& snap) {
    for (const NodeView& n : snap.nodes) {
        sf::Vector2f pos = toScreen(n.x, n.y);
        sf::Color color = nodeColor(n.name, n.id);

        // Fill matches the background exactly, so any road line passing
        // under the ring gets cleanly covered except for the ring outline.
        sf::CircleShape ring(RING_RADIUS);
        ring.setOrigin({ RING_RADIUS, RING_RADIUS });
        ring.setPosition(pos);
        ring.setFillColor(sf::Color(16, 22, 34));
        ring.setOutlineThickness(RING_THICKNESS);
        ring.setOutlineColor(color);
        window.draw(ring);

        std::string text = n.name.empty() ? ("J" + std::to_string(n.id)) : n.name;
        sf::Text label = makeText(text, 13, sf::Color(235, 235, 232));
        sf::FloatRect lb = label.getLocalBounds();
        sf::Vector2f labelPos = pos - sf::Vector2f(0.f, RING_RADIUS + 20.f);
        window.draw(roundedLabelBox(labelPos, { lb.size.x + 18.f, 24.f }, sf::Color(24, 30, 44, 235)));
        label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
        label.setPosition(labelPos);
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
        dot.setFillColor(sf::Color(10, 10, 10));
        dot.setOutlineThickness(1.f);
        dot.setOutlineColor(sf::Color(230, 230, 225));
        window.draw(dot);
    }
}

// ---- signals: one traffic-light icon per incoming road ----

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

            // Sits directly ON the road's own line (a genuine "stop line"),
            // right at the edge of the ring -- each incoming road has its
            // own line, so boxes from different roads never collide.
            float distBack = std::min(RING_RADIUS + SIGNAL_BOX_W / 2.f + 6.f, len * 0.4f);
            sf::Vector2f pos = b - u * distBack;
            float angleDeg = std::atan2(d.y, d.x) * 180.f / 3.14159265f;

            bool isGreen = (roadIds[i] == activeRoad);
            drawSignalBox(pos, angleDeg, roadIds[i], isGreen, r.blocked);
        }
    }
}

// ---- legend (bottom-left, always visible) ----

void Renderer::drawLegend(const SimSnapshot&) {
    const float w = 260.f, h = 180.f, pad = 12.f;
    sf::Vector2f pos = { 16.f, window.getSize().y - h - BOTTOM_BAR_HEIGHT - 16.f };

    sf::RectangleShape bg({ w, h });
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(24, 30, 44, 235));
    window.draw(bg);

    sf::Text title = makeText("TRAFFIC MONITOR", 14, sf::Color(240, 240, 237));
    title.setPosition({ pos.x + pad, pos.y + 8.f });
    window.draw(title);

    float x = pos.x + pad, y = pos.y + 36.f, rowH = 26.f;

    drawLine({ x, y + 8.f }, { x + 30.f, y + 8.f }, 4.f, sf::Color(60, 190, 100));
    window.draw([&] { auto t = makeText("Road (active)", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 40.f, y }); return t; }());
    y += rowH;

    drawDashedLine({ x, y + 8.f }, { x + 30.f, y + 8.f }, 4.f, sf::Color(214, 60, 60));
    window.draw([&] { auto t = makeText("Blocked road", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 40.f, y }); return t; }());
    y += rowH;

    drawSignalBox({ x + 12.f, y + 8.f }, 0.f, 0, true, false);
    window.draw([&] { auto t = makeText("Signal box on road (labeled by road id)", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 40.f, y }); return t; }());
    y += rowH + 8.f;

    sf::CircleShape ring(11.f);
    ring.setOrigin({ 11.f, 11.f });
    ring.setPosition({ x + 10.f, y + 10.f });
    ring.setFillColor(sf::Color(16, 22, 34));
    ring.setOutlineThickness(3.f);
    ring.setOutlineColor(sf::Color(120, 160, 220));
    window.draw(ring);
    window.draw([&] { auto t = makeText("Intersection", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 40.f, y }); return t; }());
    y += rowH;

    sf::CircleShape carDot(4.f);
    carDot.setOrigin({ 4.f, 4.f });
    carDot.setPosition({ x + 10.f, y + 8.f });
    carDot.setFillColor(sf::Color(10, 10, 10));
    carDot.setOutlineThickness(1.f);
    carDot.setOutlineColor(sf::Color(230, 230, 225));
    window.draw(carDot);
    window.draw([&] { auto t = makeText("Car on road (stops at red)", 13, sf::Color(225, 225, 221)); t.setPosition({ x + 40.f, y }); return t; }());
}

// ---- compass (bottom-right) ----

void Renderer::drawCompass() {
    sf::Vector2f base = { window.getSize().x - 46.f, window.getSize().y - BOTTOM_BAR_HEIGHT - 46.f };

    sf::ConvexShape arrow(3);
    arrow.setPoint(0, { base.x, base.y - 16.f });
    arrow.setPoint(1, { base.x - 8.f, base.y + 8.f });
    arrow.setPoint(2, { base.x + 8.f, base.y + 8.f });
    arrow.setFillColor(sf::Color(230, 230, 226));
    window.draw(arrow);

    sf::Text n = makeText("N", 13, sf::Color(230, 230, 226));
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
    bg.setFillColor(sf::Color(10, 14, 20, 220));
    window.draw(bg);

    sf::Text t = makeText("[SPACE] Pause/Resume    [+/-] Speed    [ESC] Quit", 13, sf::Color(215, 215, 210));
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

    window.clear(sf::Color(16, 22, 34));
    drawBackground();
    drawExternalStubs(snapshot, nodeById);
    drawRoads(snapshot, nodeById);
    drawSignals(snapshot, roadById, nodeById);
    drawVehicles(snapshot, roadById, nodeById);
    drawJunctionLabels(snapshot);

    drawTitleBox(snapshot);
    drawStatsBox(snapshot);
    drawLegend(snapshot);
    drawCompass();
    drawControlsStrip();

    window.display();
}