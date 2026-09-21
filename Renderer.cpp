#include "Renderer.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>

// ============================================================
// Look & feel. Change the colours here -- nothing else needs to move.
// ============================================================
namespace {
    const sf::Color BG_COLOR(84, 90, 99);              // the ONE background colour (soft slate grey)
    const sf::Color PANEL_FILL(20, 27, 42, 238);       // dark navy panels / pills
    const sf::Color PANEL_BORDER(96, 108, 134, 190);   // thin light border around panels
    const sf::Color ASPHALT(43, 47, 54);               // road surface
    const sf::Color TEXT_MAIN(238, 241, 247);
    const sf::Color TEXT_DIM(178, 186, 202);
    const sf::Color SIGNAL_GREEN(84, 224, 128);
    const sf::Color SIGNAL_RED(244, 74, 74);
    const sf::Color SIGNAL_OFF(128, 132, 140);
    const float PI_F = 3.14159265f;
}

// ---- construction ----

Renderer::Renderer(unsigned int width, unsigned int height, const std::string& title)
    : window(sf::VideoMode({ width, height }), title, sf::Style::Default, sf::State::Windowed,
        [] { sf::ContextSettings cs; cs.antiAliasingLevel = 8; return cs; }()),   // smooth edges on lines & circles
    fontLoaded(false),
    paused(false),
    speedMultiplier(1.0f) {
    std::cout << "\n############################################\n"
        << "  RENDERER BUILD MARKER: pretty-v1\n"
        << "  If you do not see this line, your project\n"
        << "  is NOT using this Renderer.cpp.\n"
        << "############################################\n" << std::endl;

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
        "C:/Windows/Fonts/segoeui.ttf",      // nicest of the built-in Windows fonts
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
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
    if (blocked) return sf::Color(232, 80, 80);
    if (congestion < 0.3f) return sf::Color(72, 205, 118);
    if (congestion < 0.7f) return sf::Color(246, 192, 62);
    return sf::Color(232, 80, 80);
}

sf::Color Renderer::nodeColor(const std::string& name, int id) const {
    static const std::map<std::string, sf::Color> known = {
        { "Islamabad", sf::Color(88, 150, 240) },
        { "Karachi",   sf::Color(232, 84, 84) },
        { "Lahore",    sf::Color(246, 192, 62) },
        { "Murree",    sf::Color(170, 112, 226) },
        { "Kashmir",   sf::Color(84, 208, 130) },
    };
    auto it = known.find(name);
    if (it != known.end()) return it->second;

    static const sf::Color palette[] = {
        sf::Color(88, 150, 240), sf::Color(232, 84, 84), sf::Color(246, 192, 62),
        sf::Color(170, 112, 226), sf::Color(84, 208, 130), sf::Color(236, 146, 74)
    };
    return palette[((id % 6) + 6) % 6];
}

// Rounded rectangle (SFML has none built in): a convex polygon with an arc at
// each corner. Returned already positioned at `center`; add an outline or a
// rotation afterwards if you want one.
sf::ConvexShape Renderer::roundedLabelBox(sf::Vector2f center, sf::Vector2f size, sf::Color fill, float radius) const {
    const int seg = 6;
    const float hw = size.x / 2.f, hh = size.y / 2.f;
    const float r = std::max(0.5f, std::min(radius, std::min(hw, hh)));
    const sf::Vector2f corner[4] = { { hw - r, hh - r }, { -hw + r, hh - r }, { -hw + r, -hh + r }, { hw - r, -hh + r } };

    sf::ConvexShape shape(static_cast<std::size_t>(4 * (seg + 1)));
    std::size_t idx = 0;
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i <= seg; i++) {
            float ang = (90.f * k + 90.f * i / seg) * PI_F / 180.f;
            shape.setPoint(idx++, { corner[k].x + r * std::cos(ang), corner[k].y + r * std::sin(ang) });
        }
    }
    shape.setPosition(center);
    shape.setFillColor(fill);
    return shape;
}

// A dark rounded panel with a soft shadow and a thin light border.
void Renderer::drawPanel(sf::Vector2f topLeft, sf::Vector2f size, float radius) {
    sf::Vector2f center = topLeft + size / 2.f;
    window.draw(roundedLabelBox(center + sf::Vector2f(0.f, 3.f), size, sf::Color(0, 0, 0, 55), radius));   // shadow
    sf::ConvexShape box = roundedLabelBox(center, size, PANEL_FILL, radius);
    box.setOutlineThickness(1.f);
    box.setOutlineColor(PANEL_BORDER);
    window.draw(box);
}

// ---- background: ONE flat colour ----

void Renderer::drawBackground() {
    sf::RectangleShape bg((sf::Vector2f)window.getSize());
    bg.setPosition({ 0.f, 0.f });
    bg.setFillColor(BG_COLOR);
    window.draw(bg);
}

// ---- layout: fit the junctions into whatever size the window is right now ----

void Renderer::updateLayout(const SimSnapshot& snap) {
    const sf::Vector2f win = (sf::Vector2f)window.getSize();
    if (snap.nodes.empty() || win.x < 2.f || win.y < 2.f) {
        layoutScale = 1.f;
        layoutOffset = { 0.f, 0.f };
        return;
    }

    float minX = snap.nodes[0].x, maxX = minX, minY = snap.nodes[0].y, maxY = minY;
    for (const NodeView& n : snap.nodes) {
        minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
        minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
    }
    const float spanX = std::max(maxX - minX, 1.f);
    const float spanY = std::max(maxY - minY, 1.f);

    // Fixed-size room (in pixels) needed around the junctions for their name
    // pills, the External Inflow/Outflow stubs and the road labels.
    const float padX = 100.f, padTop = 125.f, padBottom = 125.f;
    const float areaTop = 8.f;
    const float availW = win.x - 24.f;
    const float availH = win.y - BOTTOM_BAR_HEIGHT - 16.f;

    float s = std::min((availW - 2.f * padX) / spanX, (availH - padTop - padBottom) / spanY);
    layoutScale = std::clamp(s, 0.6f, 2.0f);

    const float contentH = spanY * layoutScale + padTop + padBottom;
    const float top = areaTop + (availH - contentH) / 2.f;
    layoutOffset.x = (win.x - spanX * layoutScale) / 2.f - minX * layoutScale;
    layoutOffset.y = top + padTop - minY * layoutScale;
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

// ---- signal indicator: a small glowing box laid directly ON the road, like
// a stop bar, colored red/green and labeled with its road id so it's
// unambiguous which road it belongs to.
// NOTE: TrafficSignal.h only models two states (green=1 / red=0) -- this
// is a straight red/green box, no fabricated amber state.

void Renderer::drawSignalBox(sf::Vector2f pos, float angleDeg, int roadId, bool green, bool blocked) {
    sf::Color lamp = blocked ? SIGNAL_OFF : (green ? SIGNAL_GREEN : SIGNAL_RED);

    // soft glow
    sf::ConvexShape glow = roundedLabelBox(pos, { SIGNAL_BOX_W + 12.f, SIGNAL_BOX_H + 10.f }, sf::Color(lamp.r, lamp.g, lamp.b, 70), 8.f);
    glow.setRotation(sf::degrees(angleDeg));
    window.draw(glow);

    // housing + lit face
    sf::ConvexShape box = roundedLabelBox(pos, { SIGNAL_BOX_W, SIGNAL_BOX_H }, lamp, 5.f);
    box.setRotation(sf::degrees(angleDeg)); // laid across the lane, following the road's own angle
    box.setOutlineThickness(2.f);
    box.setOutlineColor(sf::Color(12, 15, 22));
    window.draw(box);

    // Road id label, kept upright (not rotated) so it stays readable
    // regardless of the road's angle.
    sf::Color labelColor = green && !blocked ? sf::Color(8, 30, 16) : sf::Color(255, 255, 255);
    sf::Text label = makeText("R" + std::to_string(roadId), 11, labelColor);
    sf::FloatRect lb = label.getLocalBounds();
    label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
    label.setPosition(pos);
    window.draw(label);
}

// ---- title box (top-left): "{n} Roads | {n} Junctions" ----

void Renderer::drawTitleBox(const SimSnapshot& snap) {
    std::ostringstream oss;
    oss << snap.roads.size() << " Roads   |   " << snap.nodes.size() << " Junctions";
    sf::Text t = makeText(oss.str(), 17, TEXT_MAIN);
    sf::FloatRect b = t.getLocalBounds();

    sf::Vector2f size(b.size.x + 36.f, 40.f);
    drawPanel({ 20.f, 16.f }, size, 12.f);

    t.setOrigin({ b.size.x / 2.f, b.size.y / 2.f + b.position.y });
    t.setPosition({ 20.f + size.x / 2.f, 16.f + size.y / 2.f });
    window.draw(t);
}

// ---- stats box (top-right): live simulation metrics ----

void Renderer::drawStatsBox(const SimSnapshot& snap) {
    const float w = 200.f, h = 192.f, pad = 14.f;
    sf::Vector2f pos = { window.getSize().x - w - 16.f, 16.f };
    drawPanel(pos, { w, h }, 12.f);

    float x = pos.x + pad, y = pos.y + pad - 2.f;
    auto line = [&](const std::string& s, unsigned int size, sf::Color color, float gap) {
        sf::Text t = makeText(s, size, color);
        t.setPosition({ x, y });
        window.draw(t);
        y += gap;
        };

    line("Step: " + std::to_string(snap.step), 19, TEXT_MAIN, 30.f);
    line("Moving: " + std::to_string(snap.movingCount), 13, TEXT_DIM, 19.f);
    line("Waiting: " + std::to_string(snap.waitingCount), 13, TEXT_DIM, 19.f);
    line("Arrived: " + std::to_string(snap.arrivedCount), 13, TEXT_DIM, 19.f);
    line("Generated: " + std::to_string(snap.generatedCount), 13, TEXT_DIM, 25.f);

    std::ostringstream congStr;
    congStr << "Avg congestion: " << std::fixed << std::setprecision(0) << (snap.avgCongestion * 100.f) << "%";
    line(congStr.str(), 13, congestionColor(snap.avgCongestion, false), 22.f);

    line(paused ? "|| PAUSED" : "> RUNNING", 13, paused ? sf::Color(246, 192, 62) : sf::Color(150, 226, 132), 19.f);

    std::ostringstream speedStr;
    speedStr << "Speed: " << std::fixed << std::setprecision(2) << speedMultiplier << "x";
    line(speedStr.str(), 13, TEXT_DIM, 19.f);
}

// ---- one road: asphalt strip with a coloured edge (= congestion),
// white dashed centre line, and a red dashed edge when blocked ----

void Renderer::drawRoadStrip(sf::Vector2f a, sf::Vector2f b, sf::Color edge, bool blocked, float width) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1.f) return;
    sf::Vector2f u = d / len;

    if (blocked) drawDashedLine(a, b, width + 5.f, edge);
    else         drawLine(a, b, width + 4.f, edge);
    drawLine(a, b, width, ASPHALT);

    if (!blocked && width >= 10.f) {
        // centre dashes only on the visible stretch between the two rings
        float skip = RING_RADIUS + RING_THICKNESS + 3.f;
        if (len > 2.f * skip + 10.f)
            drawDashedLine(a + u * skip, b - u * skip, 1.6f, sf::Color(235, 238, 245, 150));
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
        sf::Color color = congestionColor(r.congestion, r.blocked);

        // The strip runs center-to-center; the junction ring's opaque fill is
        // drawn on top afterward and cleanly covers the part inside it.
        drawRoadStrip(a, b, color, r.blocked, ROAD_WIDTH);

        sf::Vector2f mid = (a + b) / 2.f;
        if (!r.blocked) {
            // Direction arrow half-way along the road (soft white, clearly not a car).
            drawArrowHead(mid + u * 6.f, u, sf::Color(240, 243, 250, 210), 9.f);
        }

        std::ostringstream oss;
        oss << "Road " << r.id << "  |  Cars: " << r.flow << " / " << r.capacity << " max  |  Q:" << r.queueLen;
        if (r.blocked) oss << "  BLOCKED";

        sf::Text label = makeText(oss.str(), 12, TEXT_MAIN);
        sf::FloatRect lb = label.getLocalBounds();
        sf::Vector2f perp(-u.y, u.x);
        // Always on the SAME side (perp+) for every road, so it never
        // shares space with the signal boxes' own labels.
        sf::Vector2f labelPos = mid + perp * 30.f;
        const float pillW = lb.size.x + 36.f;
        window.draw(roundedLabelBox(labelPos + sf::Vector2f(0.f, 2.f), { pillW, 24.f }, sf::Color(0, 0, 0, 45), 12.f)); // shadow
        sf::ConvexShape pill = roundedLabelBox(labelPos, { pillW, 24.f }, PANEL_FILL, 12.f);
        pill.setOutlineThickness(1.f);
        pill.setOutlineColor(PANEL_BORDER);
        window.draw(pill);

        // congestion dot at the left end of the pill
        sf::CircleShape dot(4.5f);
        dot.setOrigin({ 4.5f, 4.5f });
        dot.setPosition({ labelPos.x - pillW / 2.f + 14.f, labelPos.y });
        dot.setFillColor(color);
        window.draw(dot);

        label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
        label.setPosition({ labelPos.x + 8.f, labelPos.y });
        window.draw(label);
    }
}

// ---- external stub roads: nodes with no incoming or no outgoing road ----

void Renderer::drawExternalStubs(const SimSnapshot& snap, const std::map<int, NodeView>& /*nodeById*/) {
    std::map<int, int> incomingCount, outgoingCount;
    for (const RoadView& r : snap.roads) {
        outgoingCount[r.srcNode]++;
        incomingCount[r.dstNode]++;
    }

    const sf::Color stubEdge(168, 174, 184);
    auto pill = [&](const std::string& text, sf::Vector2f pos) {
        sf::Text label = makeText(text, 12, TEXT_MAIN);
        sf::FloatRect lb = label.getLocalBounds();
        sf::ConvexShape box = roundedLabelBox(pos, { lb.size.x + 22.f, 24.f }, PANEL_FILL, 12.f);
        box.setOutlineThickness(1.f);
        box.setOutlineColor(PANEL_BORDER);
        window.draw(box);
        label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
        label.setPosition(pos);
        window.draw(label);
        };

    for (const NodeView& n : snap.nodes) {
        sf::Vector2f pos = toScreen(n.x, n.y);
        bool hasIncoming = incomingCount.count(n.id) && incomingCount[n.id] > 0;
        bool hasOutgoing = outgoingCount.count(n.id) && outgoingCount[n.id] > 0;

        if (!hasIncoming) {
            sf::Vector2f tip = pos - sf::Vector2f(0.f, STUB_LENGTH);
            sf::Vector2f end = pos - sf::Vector2f(0.f, RING_RADIUS + 4.f);
            drawLine(tip, end, ROAD_WIDTH + 4.f, stubEdge);
            drawLine(tip, end, ROAD_WIDTH, ASPHALT);
            drawArrowHead(pos - sf::Vector2f(0.f, RING_RADIUS + 12.f), { 0.f, 1.f }, sf::Color(240, 243, 250, 230), 10.f);
            pill("External Inflow", tip - sf::Vector2f(0.f, 20.f));
        }

        if (!hasOutgoing) {
            sf::Vector2f tip = pos + sf::Vector2f(0.f, STUB_LENGTH);
            sf::Vector2f start = pos + sf::Vector2f(0.f, RING_RADIUS + 4.f);
            drawLine(start, tip, ROAD_WIDTH + 4.f, stubEdge);
            drawLine(start, tip, ROAD_WIDTH, ASPHALT);
            drawArrowHead(pos + sf::Vector2f(0.f, STUB_LENGTH * 0.85f), { 0.f, 1.f }, sf::Color(240, 243, 250, 230), 10.f);
            pill("External Outflow", tip + sf::Vector2f(0.f, 20.f));
        }
    }
}

// ---- junctions: coloured ring with a hub dot + name pill ----
// The name pill is NOT always above the ring: it goes on whichever side is
// furthest from every road / stub touching this junction (and still fits inside
// the window), so it can never sit on top of a signal box or a road.

void Renderer::drawJunctionLabels(const SimSnapshot& snap) {
    std::map<int, sf::Vector2f> posById;
    for (const NodeView& n : snap.nodes) posById[n.id] = toScreen(n.x, n.y);

    std::map<int, int> inCount, outCount;
    for (const RoadView& r : snap.roads) { outCount[r.srcNode]++; inCount[r.dstNode]++; }

    const sf::Vector2f win = (sf::Vector2f)window.getSize();

    for (const NodeView& n : snap.nodes) {
        sf::Vector2f pos = posById[n.id];
        sf::Color color = nodeColor(n.name, n.id);

        // soft drop shadow
        sf::CircleShape shadow(RING_RADIUS + RING_THICKNESS, 64);
        shadow.setOrigin({ RING_RADIUS + RING_THICKNESS, RING_RADIUS + RING_THICKNESS });
        shadow.setPosition(pos + sf::Vector2f(0.f, 4.f));
        shadow.setFillColor(sf::Color(0, 0, 0, 60));
        window.draw(shadow);

        // Opaque dark fill: any road line passing under the ring is cleanly
        // covered except for the ring outline.
        sf::CircleShape ring(RING_RADIUS, 64);
        ring.setOrigin({ RING_RADIUS, RING_RADIUS });
        ring.setPosition(pos);
        ring.setFillColor(sf::Color(20, 27, 42));   // must be fully opaque
        ring.setOutlineThickness(RING_THICKNESS);
        ring.setOutlineColor(color);
        window.draw(ring);

        sf::CircleShape hub(7.f, 32);
        hub.setOrigin({ 7.f, 7.f });
        hub.setPosition(pos);
        hub.setFillColor(sf::Color(color.r, color.g, color.b, 200));
        window.draw(hub);

        std::string text = n.name.empty() ? ("J" + std::to_string(n.id)) : n.name;
        sf::Text label = makeText(text, 13, TEXT_MAIN);
        sf::FloatRect lb = label.getLocalBounds();
        const float pillW = lb.size.x + 24.f, pillH = 26.f;

        // Directions (degrees, screen coords: 0 = right, 90 = down, -90 = up)
        // occupied by roads or External Inflow/Outflow stubs at this junction.
        std::vector<float> busy;
        auto angleTo = [&](sf::Vector2f target) {
            sf::Vector2f d = target - pos;
            return std::atan2(d.y, d.x) * 180.f / PI_F;
            };
        for (const RoadView& r : snap.roads) {
            if (r.srcNode == n.id && posById.count(r.dstNode)) busy.push_back(angleTo(posById[r.dstNode]));
            if (r.dstNode == n.id && posById.count(r.srcNode)) busy.push_back(angleTo(posById[r.srcNode]));
        }
        if (inCount[n.id] == 0)  busy.push_back(-90.f);  // inflow stub is drawn above
        if (outCount[n.id] == 0) busy.push_back(90.f);   // outflow stub is drawn below

        // Try 8 spots around the ring; pick the one furthest from anything busy.
        // "Above" gets a head start so the classic look is kept where it is free.
        struct Cand { float deg, bonus; };
        const Cand cands[8] = { { -90.f, 35.f }, { 90.f, 8.f }, { 180.f, 4.f }, { 0.f, 4.f },
                                { -45.f, 0.f }, { -135.f, 0.f }, { 45.f, 0.f }, { 135.f, 0.f } };
        sf::Vector2f bestCenter = pos - sf::Vector2f(0.f, RING_RADIUS + 22.f);
        float bestScore = -1e9f;
        for (const Cand& c : cands) {
            float rad = c.deg * PI_F / 180.f;
            sf::Vector2f dir(std::cos(rad), std::sin(rad));
            // distance so the pill's nearest point clears the ring by 8px
            float support = std::fabs(dir.x) * pillW / 2.f + std::fabs(dir.y) * pillH / 2.f;
            sf::Vector2f center = pos + dir * (RING_RADIUS + RING_THICKNESS + 8.f + support);

            bool fits = center.x - pillW / 2.f >= 6.f && center.x + pillW / 2.f <= win.x - 6.f
                && center.y - pillH / 2.f >= 6.f && center.y + pillH / 2.f <= win.y - BOTTOM_BAR_HEIGHT - 6.f;

            float minDiff = 180.f;
            for (float b : busy) {
                float d = std::fmod(std::fabs(c.deg - b), 360.f);
                if (d > 180.f) d = 360.f - d;
                minDiff = std::min(minDiff, d);
            }
            float score = minDiff + c.bonus - (fits ? 0.f : 1000.f);
            if (score > bestScore) { bestScore = score; bestCenter = center; }
        }

        sf::ConvexShape pill = roundedLabelBox(bestCenter, { pillW, pillH }, PANEL_FILL, 13.f);
        pill.setOutlineThickness(1.5f);
        pill.setOutlineColor(color);
        window.draw(pill);
        label.setOrigin({ lb.size.x / 2.f, lb.size.y / 2.f + lb.position.y });
        label.setPosition(bestCenter);
        window.draw(label);
    }
}

// ---- vehicles: black dots on the road ----
//
// A dot is ALWAYS placed on the visible part of its road -- between the
// two junction rings -- so it can never hide underneath a ring and "vanish".
// Along that visible stretch:
//   startD  = just outside the source ring
//   stopD   = just BEHIND the signal box (the stop line)
//   endD    = just outside the destination ring
// Progress 0..stopLineProgress maps onto startD..stopD, and
// stopLineProgress..1 maps onto stopD..endD (i.e. the dot crosses the signal
// box only during the last part of the trip, and only if it was allowed to).

void Renderer::drawVehicles(const SimSnapshot& snap,
    const std::map<int, RoadView>& roadById,
    const std::map<int, NodeView>& nodeById) {

    // How many cars are already parked on each road's stop line, so a car
    // still driving up to a red light stops BEHIND them instead of on top of them.
    std::map<int, int> parkedOnRoad;
    for (const VehicleView& v : snap.vehicles)
        if (v.mode == DotMode::StoppedAtLine) parkedOnRoad[v.roadId]++;

    const float spacing = VEHICLE_RADIUS * 2.f + 3.f;
    const float ringClear = RING_RADIUS + RING_THICKNESS + VEHICLE_RADIUS + 2.f;
    const float stopP = std::clamp(snap.stopLineProgress, 0.10f, 0.95f);

    for (const VehicleView& v : snap.vehicles) {
        auto itRoad = roadById.find(v.roadId);
        if (itRoad == roadById.end()) continue;
        const RoadView& r = itRoad->second;

        auto itSrc = nodeById.find(r.srcNode);
        auto itDst = nodeById.find(r.dstNode);
        if (itSrc == nodeById.end() || itDst == nodeById.end()) continue;

        sf::Vector2f a = toScreen(itSrc->second.x, itSrc->second.y);
        sf::Vector2f b = toScreen(itDst->second.x, itDst->second.y);
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1.f) continue;
        sf::Vector2f u = d / len;

        // Visible stretch of the road (outside both rings).
        float startD = ringClear;
        float endD = len - ringClear;
        if (endD <= startD) { startD = len * 0.25f; endD = len * 0.75f; }

        // The signal box sits `sigDist` before the junction (same formula as
        // drawSignals); the stop line is just behind it.
        float sigDist = std::min(RING_RADIUS + SIGNAL_BOX_W / 2.f + 6.f, len * 0.4f);
        float stopD = len - sigDist - SIGNAL_BOX_W / 2.f - VEHICLE_RADIUS - 3.f;
        stopD = std::clamp(stopD, startD, endD);

        // Where progress alone would put the dot.
        float p = std::clamp(v.progress, 0.f, 1.f);
        float dist = (p <= stopP)
            ? startD + (p / stopP) * (stopD - startD)
            : stopD + ((p - stopP) / (1.f - stopP)) * (endD - stopD);

        switch (v.mode) {
        case DotMode::Moving:
            break;
        case DotMode::ApproachRed: {
            int ahead = parkedOnRoad.count(v.roadId) ? parkedOnRoad[v.roadId] : 0;
            dist = std::min(dist, stopD - ahead * spacing);
            break;
        }
        case DotMode::StoppedAtLine:
            dist = stopD - v.slot * spacing;
            break;
        case DotMode::WaitingAtStart:
            dist = startD + v.slot * spacing;
            break;
        case DotMode::Exiting:
            break;   // handled below
        }

        float radius = VEHICLE_RADIUS;
        float fade = v.alpha;
        if (v.mode == DotMode::Exiting) {
            // Arrived: keep driving from the end of the road straight INTO the
            // junction (to its centre), shrinking and fading out on the way.
            float t = std::clamp(v.progress, 0.f, 1.f);
            dist = endD + (len - endD) * t;
            radius = VEHICLE_RADIUS * (1.f - 0.45f * t);
            fade *= std::clamp((1.f - t) / 0.45f, 0.f, 1.f);   // solid for the first ~55%, then fades
        }
        else {
            dist = std::clamp(dist, startD, endD);
        }

        sf::Vector2f pos = a + u * dist;
        const std::uint8_t alpha = static_cast<std::uint8_t>(std::clamp(fade, 0.f, 1.f) * 255.f);

        sf::CircleShape dot(radius);
        dot.setOrigin({ radius, radius });
        dot.setPosition(pos);
        dot.setFillColor(sf::Color(10, 10, 10, alpha));
        dot.setOutlineThickness(1.f);
        dot.setOutlineColor(sf::Color(230, 230, 225, alpha));
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
    const float w = 250.f, h = 186.f, pad = 14.f;
    sf::Vector2f pos = { 16.f, window.getSize().y - h - BOTTOM_BAR_HEIGHT - 16.f };
    drawPanel(pos, { w, h }, 12.f);

    sf::Text title = makeText("TRAFFIC MONITOR", 14, TEXT_MAIN);
    title.setPosition({ pos.x + pad, pos.y + 9.f });
    window.draw(title);

    float x = pos.x + pad, y = pos.y + 38.f, rowH = 26.f;
    auto label = [&](const std::string& s) {
        sf::Text t = makeText(s, 13, TEXT_DIM);
        t.setPosition({ x + 44.f, y });
        window.draw(t);
        };

    // congestion swatches
    const sf::Color cols[3] = { congestionColor(0.f, false), congestionColor(0.5f, false), congestionColor(1.f, false) };
    for (int i = 0; i < 3; i++) {
        sf::CircleShape c(5.f);
        c.setOrigin({ 5.f, 5.f });
        c.setPosition({ x + 6.f + i * 14.f, y + 9.f });
        c.setFillColor(cols[i]);
        window.draw(c);
    }
    label("Road: low / mid / high load");
    y += rowH;

    drawRoadStrip({ x, y + 9.f }, { x + 34.f, y + 9.f }, congestionColor(1.f, true), true, 8.f);
    label("Blocked road");
    y += rowH;

    drawSignalBox({ x + 17.f, y + 9.f }, 0.f, 0, true, false);
    label("Signal (labeled by road id)");
    y += rowH;

    sf::CircleShape ring(10.f, 32);
    ring.setOrigin({ 10.f, 10.f });
    ring.setPosition({ x + 17.f, y + 9.f });
    ring.setFillColor(sf::Color(20, 27, 42));
    ring.setOutlineThickness(3.f);
    ring.setOutlineColor(sf::Color(120, 160, 230));
    window.draw(ring);
    label("Intersection");
    y += rowH;

    sf::CircleShape carDot(4.5f);
    carDot.setOrigin({ 4.5f, 4.5f });
    carDot.setPosition({ x + 17.f, y + 9.f });
    carDot.setFillColor(sf::Color(10, 10, 10));
    carDot.setOutlineThickness(1.f);
    carDot.setOutlineColor(sf::Color(230, 230, 225));
    window.draw(carDot);
    label("Car (stops at red)");
}

// ---- compass (bottom-right) ----

void Renderer::drawCompass() {
    // tucked under the stats box (the bottom-right corner is where the last junction's Outflow label can sit)
    sf::Vector2f base = { window.getSize().x - 46.f, 16.f + 192.f + 44.f };

    sf::CircleShape disc(26.f, 48);
    disc.setOrigin({ 26.f, 26.f });
    disc.setPosition(base + sf::Vector2f(0.f, 3.f));
    disc.setFillColor(PANEL_FILL);
    disc.setOutlineThickness(1.f);
    disc.setOutlineColor(PANEL_BORDER);
    window.draw(disc);

    sf::ConvexShape arrow(3);
    arrow.setPoint(0, { base.x, base.y - 15.f });
    arrow.setPoint(1, { base.x - 7.f, base.y + 3.f });
    arrow.setPoint(2, { base.x + 7.f, base.y + 3.f });
    arrow.setFillColor(sf::Color(238, 241, 247));
    window.draw(arrow);

    sf::Text n = makeText("N", 13, TEXT_MAIN);
    sf::FloatRect b = n.getLocalBounds();
    n.setOrigin({ b.size.x / 2.f, b.size.y / 2.f + b.position.y });
    n.setPosition({ base.x, base.y + 14.f });
    window.draw(n);
}

// ---- bottom controls strip ----

void Renderer::drawControlsStrip() {
    float y = (float)window.getSize().y - BOTTOM_BAR_HEIGHT;
    sf::RectangleShape bg({ (float)window.getSize().x, BOTTOM_BAR_HEIGHT });
    bg.setPosition({ 0.f, y });
    bg.setFillColor(sf::Color(14, 19, 30, 240));
    window.draw(bg);

    sf::RectangleShape rule({ (float)window.getSize().x, 1.f });
    rule.setPosition({ 0.f, y });
    rule.setFillColor(PANEL_BORDER);
    window.draw(rule);

    sf::Text t = makeText("[SPACE] Pause/Resume    [+/-] Speed    [ESC] Quit", 13, TEXT_DIM);
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

    // Keep the view exactly 1:1 with the window's pixels every frame. Without
    // this SFML keeps the ORIGINAL window's view after a resize/maximise, so
    // anything anchored to the window edges (stats, legend, compass, controls)
    // ends up outside the visible area and "disappears".
    const sf::Vector2f winSize = (sf::Vector2f)window.getSize();
    if (winSize.x < 2.f || winSize.y < 2.f) return;   // minimised
    window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, winSize)));

    updateLayout(snapshot);

    window.clear(BG_COLOR);
    drawBackground();
    drawExternalStubs(snapshot, nodeById);
    drawRoads(snapshot, nodeById);
    drawSignals(snapshot, roadById, nodeById);
    drawJunctionLabels(snapshot);
    drawVehicles(snapshot, roadById, nodeById);   // after the rings: a car that has arrived is drawn driving INTO its junction

    drawTitleBox(snapshot);
    drawStatsBox(snapshot);
    if (winSize.x >= 960.f && winSize.y >= 600.f) drawLegend(snapshot);   // hidden in very small windows so it can't cover the map
    drawCompass();
    drawControlsStrip();

    window.display();
}