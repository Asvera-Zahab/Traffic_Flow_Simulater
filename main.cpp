#include <SFML/Graphics.hpp>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Simulator.h"
using namespace std;

const int   WIDTH = 1200, HEIGHT = 780;     //canvas ka scale
const float PI = 3.14159265f;
const float STEP_INTERVAL = 1.20f, STEP_FAST = 0.40f;       //step time 1.2s for nroaml and 0.4s for fast
const float STOP_LINE_DIST = 32.f, YELLOW_DURATION = 0.55f;   //decides how many pixels b4 car shld stop

map<int, sf::Vector2f> nodePos = {              //cities(x,y)
    {0,{170.f,390.f}},{1,{450.f,140.f}},{2,{700.f,390.f}},
    {3,{450.f,630.f}},{4,{960.f,390.f}}
};
const map<int, string> NODE_LABELS = {
    {0,"Karachi"},{1,"Islamabad"},{2,"Lahore"},{3,"Murree"},{4,"Kashmir"}
};

float vlen(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }//pythagoras gives vector length
sf::Vector2f vnorm(sf::Vector2f v) { float l = vlen(v); return l > 0 ? v / l : sf::Vector2f{ 0,0 }; }//normalize used for arrow length 
float dirAngle(sf::Vector2f a, sf::Vector2f b) { sf::Vector2f d = b - a; return std::atan2(d.y, d.x) * 180.f / PI; }//road ka angle which cars also follow
float lerpF(float dt, float rate) { return 1.f - std::exp(-rate * dt); }//smooth sim

struct VehicleRender {      //carsdisplay
    sf::Vector2f pos, lastTarget, stopLinePos;
    float angle = 0, colourBlend = 0;
    bool active = false;
};
map<int, VehicleRender> vRender;

struct SignalVisual { int prevGreenRoad = -1, currGreenRoad = -1; float yellowTimer = 0; };
map<int, SignalVisual> sigVis;   //remember roads that were g,r or y

// Stop Line where car should stop when waiting for g light
sf::Vector2f computeStopLine(int nodeId, int rid, const Graph& graph)
{
    if (!nodePos.count(nodeId)) return { 0,0 };
    if (rid < 0 || rid >= (int)graph.roads.size()) return nodePos[nodeId];
    const Road& r = graph.roads[rid];
    if (!nodePos.count(r.source)) return nodePos[nodeId];
    sf::Vector2f npos = nodePos[nodeId], dir = vnorm(npos - nodePos[r.source]);
    sf::Vector2f perp(-dir.y, dir.x);
    return npos - dir * STOP_LINE_DIST + perp * 6.f;
}

bool isGreenForVehicle(const Vehicle& v, const map<int, TrafficSignal>& signals, const Graph&)
{
    if (!signals.count(v.currentNode) || !v.hasPath() || v.currentRoad < 0) return true;
    return signals.at(v.currentNode).currentGreenRoad == v.currentRoad;
}   //agar green, move

//  drawRoad
void drawRoad(sf::RenderWindow& win, sf::Vector2f a, sf::Vector2f b,
    float congestion, bool blocked)
{
    sf::Vector2f d = b - a; float len = vlen(d); if (len < 1)return;
    sf::Vector2f u = d / len, p(-u.y, u.x);
    const float RW = 13.f;
    sf::Vector2f tl = a + p * RW, tr = b + p * RW, bl = a - p * RW, br = b - p * RW;

    auto quad = [&](sf::Color c) {         //Color
        sf::Vertex v[6]; for (auto& x : v)x.color = c;
        v[0].position = tl;v[1].position = tr;v[2].position = bl;
        v[3].position = tr;v[4].position = br;v[5].position = bl;
        win.draw(v, 6, sf::PrimitiveType::Triangles);
        };
    auto line = [&](sf::Vector2f s, sf::Vector2f e, sf::Color c) {
        sf::Vertex v[2] = { {s,c},{e,c} };
        win.draw(v, 2, sf::PrimitiveType::Lines);
        };

    quad({ 28,28,35,245 });

    sf::Color col;
    if (blocked) col = { 180,20,20,90 };
    else { uint8_t r = (uint8_t)std::min(255.f, 510.f * congestion), g = (uint8_t)std::min(255.f, 510.f * (1 - congestion)); col = { r,g,35,55 }; }
    quad(col);

    sf::Color kerb(90, 100, 130, 200);
    line(a + p * RW, b + p * RW, kerb);
    line(a - p * RW, b - p * RW, kerb); //kerb lines on road edges

    sf::Color dash(200, 200, 100, 130);
    bool drawing = true; float travelled = 0;
    while (travelled < len - 2) {    //road's dashes
        float seg = std::min(travelled + (drawing ? 14.f : 10.f), len);
        if (drawing) line(a + u * travelled, a + u * seg, dash); travelled = seg; drawing = !drawing;
    }

    sf::Color lane(55, 60, 80, 120);
    line(a + p * (RW * 0.45f), b + p * (RW * 0.45f), lane);
    line(a - p * (RW * 0.45f), b - p * (RW * 0.45f), lane);

    if (blocked) {          //if road is blocked colors red
        sf::Vector2f mid = (a + b) * 0.5f; sf::Color xc(220, 40, 40, 210);
        line(mid + p * 10.f - u * 14.f, mid - p * 10.f + u * 14.f, xc);
        line(mid - p * 10.f - u * 14.f, mid + p * 10.f + u * 14.f, xc);
        sf::Color hatch(180, 30, 30, 60);
        for (float t = 0;t < len;t += 22.f) line(a + u * t + p * RW, a + u * t - p * RW, hatch);
    }

    sf::Color arrowCol(200, 210, 255, 110); float as = 7.f;     //arrowcolor
    for (float t : {0.25f, 0.75f}) {
        sf::Vector2f tip = a + d * t, L = tip - u * as + p * (as * 0.5f), R = tip - u * as - p * (as * 0.5f);
        sf::Vertex arr[3] = { {tip,arrowCol},{L,arrowCol},{R,arrowCol} };
        win.draw(arr, 3, sf::PrimitiveType::Triangles);
    }
}

//  drawNode
void drawNode(sf::RenderWindow& win, sf::Vector2f pos,
    const string& label, sf::Font& font, bool fl)
{
    auto circle = [&](float r, sf::Color fill, sf::Color out = { 0,0,0,0 }, float ot = 0) {
        sf::CircleShape c(r); c.setFillColor(fill); c.setOrigin({ r,r }); c.setPosition(pos);
        if (ot > 0) { c.setOutlineColor(out);c.setOutlineThickness(ot); }
        win.draw(c);
        };
    circle(17, { 22,28,55 }, { 110,170,255 }, 3);    //city wala circle
    sf::CircleShape dot(5); dot.setFillColor({ 160,200,255,180 });
    dot.setOrigin({ 5.f,5.f }); dot.setPosition(pos + sf::Vector2f(-3.f, -4.f)); win.draw(dot);
    if (fl) {
        sf::Text t(font, label, 12);
        t.setFillColor(sf::Color::White);

        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin({ b.position.x + b.size.x / 2.f,
                      b.position.y + b.size.y / 2.f });

        t.setPosition(pos + sf::Vector2f(0.f, 27.f));

        win.draw(t);
    }
}

//  drawLightBox
void drawLightBox(sf::RenderWindow& win, sf::Vector2f base, int state)
{
    const float BW = 20.f, BH = 58.f, BR = 6.5f;
    auto rect = [&](sf::Vector2f p, sf::Vector2f sz, sf::Color c) {
        sf::RectangleShape r(sz); r.setFillColor(c); r.setPosition(p); win.draw(r);
        };
    rect({ base.x + 8.f,base.y + BH }, { 4.f,36.f }, { 55,55,65 });
    rect({ base.x + 3.f,base.y + BH + 33.f }, { 14.f,4.f }, { 40,40,50 });
    rect({ base.x + 2.f,base.y + 2.f }, { BW,BH }, { 0,0,0,80 });
    sf::RectangleShape box({ BW,BH }); box.setFillColor({ 18,18,22 });
    box.setOutlineColor({ 70,72,85 }); box.setOutlineThickness(2.f);
    box.setPosition(base); win.draw(box);
    for (int i = 0;i < 3;i++) rect({ base.x + 1.f,base.y + 5.f + (float)i * 18.f }, { BW - 2.f,4.f }, { 10,10,12 });

    struct BulbDef { sf::Color on, off; };
    BulbDef defs[3] = { {{255,45,45},{55,8,8}},{{255,210,0},{55,45,0}},{{40,230,70},{5,55,12}} };
    for (int i = 0;i < 3;i++) {
        bool lit = (state == i);
        sf::Vector2f bp = { base.x + BW / 2.f,base.y + 11.f + (float)i * 18.f };
        if (lit) {
            float hr = (i == 0) ? 20.f : 15.f, hr2 = (i == 0) ? 13.f : 10.f;
            auto halo = [&](float r, uint8_t a) {
                sf::CircleShape h(r); h.setOrigin({ r,r }); h.setPosition(bp);
                sf::Color hc = defs[i].on; hc.a = a; h.setFillColor(hc); win.draw(h);
                };
        }
        sf::CircleShape bulb(BR); bulb.setOrigin({ BR,BR }); bulb.setPosition(bp);
        bulb.setFillColor(lit ? defs[i].on : defs[i].off);
        if (lit) { bulb.setOutlineColor({ 255,255,255,60 });bulb.setOutlineThickness(1); }
        win.draw(bulb);
    }
}
//  drawNodeSignals
void drawNodeSignals(sf::RenderWindow& win, int nodeId, const Graph& graph,
    const map<int, TrafficSignal>& signals, const SignalVisual& sv)
{
    if (!signals.count(nodeId) || !nodePos.count(nodeId)) return;
    const TrafficSignal& sig = signals.at(nodeId);
    sf::Vector2f npos = nodePos[nodeId];
    auto nit = graph.nodes.find(nodeId);
    if (nit == graph.nodes.end()) return;
    const vector<int>& inR = nit->second.incomingRoads;
    float total = (float)inR.size();
    for (int idx = 0;idx < (int)inR.size();idx++) {
        int rid = inR[idx];
        if (rid < 0 || rid >= (int)graph.roads.size() || !nodePos.count(graph.roads[rid].source)) continue;
        sf::Vector2f dir = vnorm(npos - nodePos[graph.roads[rid].source]);
        sf::Vector2f perp(-dir.y, dir.x);
        sf::Vector2f anchor = npos - dir * (60.f + total * 10.f + (float)idx * 6.f) + perp * (18.f + total * 2.f);
        sf::Vector2f bpos = anchor - sf::Vector2f(10.f, 29.f);
        int state = 0;// red (0) by default, yellow (1) if it just turned red, green (2) if it's the active road.
        if (sv.yellowTimer > 0 && sv.prevGreenRoad == rid) state = 1;
        else if (sig.currentGreenRoad == rid) state = 2;
        drawLightBox(win, bpos, state);
        sf::Color tagCol = (state == 2) ? sf::Color(40, 230, 70, 220) : (state == 1) ? sf::Color(255, 210, 0, 220) : sf::Color(255, 45, 45, 220);
        sf::RectangleShape tag({ 20.f,4.f }); tag.setFillColor(tagCol);
        tag.setPosition({ bpos.x,bpos.y + 58.f + 36.f + 4.f }); win.draw(tag);
    }
}
//  drawCar
void drawCar(sf::RenderWindow& win, sf::Vector2f pos, float angle,
    sf::Color col, bool braking)
{
    sf::Transform tf; tf.translate(pos); tf.rotate(sf::degrees(angle));
    sf::RenderStates rs; rs.transform = tf;

    auto rr = [&](sf::Vector2f p, sf::Vector2f sz, sf::Color c, sf::Vector2f ori = { 11.f,5.5f }) {
        sf::RectangleShape r(sz); r.setOrigin(ori); r.setFillColor(c); r.setPosition(p); win.draw(r, rs);
        };
    rr({ 0.f,0.f }, { 22.f,11.f }, col);
    rr({ 1.f,0.f }, { 10.f,8.f }, { 140,195,255,200 }, { 5.f,4.f });

    sf::Color tlc = braking ? sf::Color(255, 20, 20) : sf::Color(200, 30, 30); float tlR = braking ? 2.5f : 1.6f;
    sf::CircleShape tl(tlR); tl.setOrigin({ tlR,tlR }); tl.setFillColor(tlc);
    if (braking) { tl.setOutlineColor({ 255,80,80,120 });tl.setOutlineThickness(2.f); }
    tl.setPosition({ -11.f,4.5f }); win.draw(tl, rs);
    tl.setPosition({ -11.f,-4.5f }); win.draw(tl, rs);
}
//vehColor
sf::Color vehColor(int id, int status, float blend)
{
    static const sf::Color pal[] = { {255,210,0},{0,200,255},{255,90,140},{80,255,170},{255,150,50},{190,90,255} };
    sf::Color c = pal[id % 6]; //Each car gets one of 6 colours based on its ID number.
    return c;
}
//  drawHUD
void drawHUD(sf::RenderWindow& win, sf::Font& font, bool fl,  //info panel
    const Simulator& sim, float speed)
{
    sf::RectangleShape panel({ 250.f,195.f }); panel.setFillColor({ 6,8,22,220 });
    panel.setOutlineColor({ 70,110,255,200 }); panel.setOutlineThickness(2.f); panel.setPosition({ 10.f,10.f }); win.draw(panel);
    sf::RectangleShape accent({ 250.f,4.f }); accent.setFillColor({ 70,130,255,200 }); accent.setPosition({ 10.f,10.f }); win.draw(accent);
    if (!fl) return;

    sf::Text title(font, "TRAFFIC MONITOR", 13); title.setFillColor({ 130,180,255 }); title.setPosition({ 18,18 }); win.draw(title);
    sf::RectangleShape div({ 230.f,1.f }); div.setFillColor({ 50,70,150,160 }); div.setPosition({ 18.f,36.f }); win.draw(div);

    int moving = 0, waiting = 0, arrived = 0, atRed = 0;
    for (auto& v : sim.vehicles) {
        if (v.status == MOVING) moving++;
        else if (v.status == WAITING && v.currentNode != v.destination) waiting++;
        else if (v.status == ARRIVED) arrived++;
        if (v.status == WAITING && v.currentNode != v.destination && sim.signals.count(v.currentNode) && v.hasPath()) atRed++;
    }
    double sc = 0; for (auto& r : sim.graph.roads) sc += r.congestion;
    double avgC = sim.graph.roads.empty() ? 0 : sc / sim.graph.roads.size();
    string cs = to_string(avgC); if (cs.size() > 5)cs = cs.substr(0, 5);

    auto row = [&](const string& lbl, const string& val, float y, sf::Color vc = { 130,220,130,255 }) {
        sf::Text l(font, lbl, 12); l.setFillColor({ 160,175,210,255 }); l.setPosition({ 18,y }); win.draw(l);
        sf::Text vt(font, val, 12); vt.setFillColor(vc);
        sf::FloatRect vb = vt.getLocalBounds(); vt.setPosition({ 248 - vb.size.x,y }); win.draw(vt);
        };
    auto dot = [&](sf::Color c, float x, float y) { sf::CircleShape d(4.f); d.setFillColor(c); d.setPosition({ x,y + 3.f }); win.draw(d); };

    float y = 42;
    row("Step", to_string(sim.currentStep), y); y += 18;
    dot({ 100,180,255 }, 18, y); row("  Moving", to_string(moving), y, { 100,220,255,255 }); y += 18;
    dot({ 200,200,80 }, 18, y); row("  Waiting", to_string(waiting), y, { 220,220,100,255 }); y += 18;
    dot({ 255,60,60 }, 18, y); row("  At Red", to_string(atRed), y, { 255,90,90,255 }); y += 18;
    dot({ 80,220,120 }, 18, y); row("  Arrived", to_string(arrived), y, { 80,220,120,255 }); y += 18;
    row("Generated", to_string(sim.totalGenerated), y, { 200,200,200,255 }); y += 18;
    row("Avg Cong", cs, y, { 255,180,60,255 }); y += 18;

    sf::RectangleShape cbar({ 210.f,5.f }); cbar.setFillColor({ 30,30,50 }); cbar.setPosition({ 18.f,y }); win.draw(cbar);
    float fill = std::min(1.f, (float)avgC) * 210.f;
    sf::RectangleShape cfill({ fill,5.f });
    cfill.setFillColor({ (uint8_t)std::min(255.f,510.f * (float)avgC),(uint8_t)std::min(255.f,510.f * (1.f - (float)avgC)),35u,220u });
    cfill.setPosition({ 18.f,y }); win.draw(cfill); y += 14.f;

    bool fast = speed < 0.5f;
    sf::RectangleShape pill({ 80.f,16.f });
    pill.setFillColor(fast ? sf::Color(60, 15, 15, 220) : sf::Color(15, 40, 15, 220));
    pill.setOutlineColor(fast ? sf::Color(255, 80, 80) : sf::Color(60, 200, 80)); pill.setOutlineThickness(1.f);
    pill.setPosition({ 18.f,y }); win.draw(pill);
    sf::Text spd(font, fast ? "FAST" : "NORMAL", 11);
    spd.setFillColor(fast ? sf::Color(255, 90, 90) : sf::Color(80, 220, 100)); spd.setPosition({ 24,y + 1 }); win.draw(spd);
    sf::Text hint(font, "[SPC] speed  [ESC] quit", 10);
    hint.setFillColor({ 70,90,150,200 }); hint.setPosition({ 108,y + 2 }); win.draw(hint);
}

//  drawLegend
void drawLegend(sf::RenderWindow& win, sf::Font& font, bool fl)
{
    sf::RectangleShape p({ 195.f,160.f }); p.setFillColor({ 6,8,22,220 });
    p.setOutlineColor({ 70,110,255,200 }); p.setOutlineThickness(2.f);
    p.setPosition({ (float)WIDTH - 210.f,10.f }); win.draw(p);
    sf::RectangleShape accent({ 195.f,4.f }); accent.setFillColor({ 70,130,255,200 });
    accent.setPosition({ (float)WIDTH - 210.f,10.f }); win.draw(accent);
    if (!fl) return;
    sf::Text title(font, "LEGEND", 13); title.setFillColor({ 130,180,255 });
    title.setPosition({ (float)WIDTH - 202.f,18.f }); win.draw(title);
    sf::RectangleShape div({ 175.f,1.f }); div.setFillColor({ 50,70,150,160 });
    div.setPosition({ (float)WIDTH - 202.f,36.f }); win.draw(div);

    struct Row { sf::Color c; string s; bool isRect; };
    Row rows[] = {
        {{255,210,0},"Moving vehicle",false},{{100,100,50},"Waiting / red",false},
        {{28,28,35},"Road",true},{{40,220,70},"Road: clear",true},
        {{255,60,60},"Road: congested",true},{{75,18,18},"Road: blocked",true},
        {{255,40,40},"Signal: RED",false},{{35,225,65},"Signal: GREEN",false}
    };
    for (int i = 0;i < 8;i++) {
        float ry = 42 + (float)i * 14;
        if (rows[i].isRect) {
            sf::RectangleShape sq({ 10.f,8.f }); sq.setFillColor(rows[i].c);
            sq.setOutlineColor({ 80,90,110,120 }); sq.setOutlineThickness(0.5f);
            sq.setPosition({ (float)WIDTH - 203.f,ry + 1.f }); win.draw(sq);
        }
        else {
            sf::CircleShape d(5.f); d.setFillColor(rows[i].c);
            d.setPosition({ (float)WIDTH - 203.f,ry }); win.draw(d);
        }
        sf::Text t(font, rows[i].s, 11); t.setFillColor({ 185,205,255,230 });
        t.setPosition({ (float)WIDTH - 188.f,ry - 1.f }); win.draw(t);
    }
}
//  MAIN
int main()
{
    Simulator sim;
    sim.buildCityGraph(); sim.setupSignals(); sim.scheduleEvents();
    for (auto& [nid, sig] : sim.signals) sigVis[nid] = { -1,sig.currentGreenRoad,0 };

    sf::RenderWindow window(sf::VideoMode({ (unsigned)WIDTH,(unsigned)HEIGHT }), "Traffic Flow Optimization Simulation");
    window.setFramerateLimit(100);

    sf::Font font; bool fontLoaded = false;
    for (auto fp : {
        "C:\\Windows\\Fonts\\arial.ttf","C:\\Windows\\Fonts\\calibri.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/System/Library/Fonts/Helvetica.ttc"
        }) if (font.openFromFile(fp)) { fontLoaded = true;break; }

    sf::Clock clock; float accum = 0, stepSpeed = STEP_INTERVAL;
    //one tick and do all these
    auto doStep = [&]() {
        map<int, int> oldGreen;
        for (auto& [nid, sig] : sim.signals) oldGreen[nid] = sig.currentGreenRoad;
        sim.currentStep++;
        sim.processEvents(); sim.generateVehicles();
        auto departed = sim.moveVehicles();
        sim.updateRoadStates(departed); sim.updateSignals();
        sim.releaseFromQueues(); sim.rerouteWaitingVehicles(); sim.dispatchWaitingVehicles();

        for (auto& [nid, sig] : sim.signals) {
            int ng = sig.currentGreenRoad, og = oldGreen.count(nid) ? oldGreen[nid] : -1;
            if (ng != og) { sigVis[nid].prevGreenRoad = og; sigVis[nid].yellowTimer = YELLOW_DURATION; }
            sigVis[nid].currGreenRoad = ng;
        }
        for (auto& v : sim.vehicles) {
            if (!vRender.count(v.id)) {
                VehicleRender vr;
                vr.pos = vr.lastTarget = nodePos.count(v.source) ? nodePos[v.source] : nodePos.begin()->second;
                vr.active = true; vRender[v.id] = vr;
            }
            if (v.status == WAITING)
                vRender[v.id].stopLinePos = computeStopLine(v.currentNode, v.currentRoad, sim.graph);
        }
        set<int> live; for (auto& v : sim.vehicles) live.insert(v.id);
        for (auto it = vRender.begin();it != vRender.end();)
            it = live.count(it->first) ? ++it : vRender.erase(it);
        };

    while (window.isOpen()) {
        float dt = std::min(clock.restart().asSeconds(), 0.12f);
        while (const auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) window.close();
            if (const auto* kp = ev->getIf<sf::Event::KeyPressed>()) {
                if (kp->code == sf::Keyboard::Key::Space) stepSpeed = (stepSpeed > 0.5f) ? STEP_FAST : STEP_INTERVAL;
                if (kp->code == sf::Keyboard::Key::Escape) window.close();
            }
        }
        if ((accum += dt) >= stepSpeed) { accum = 0; doStep(); }
        for (auto& [nid, sv] : sigVis) sv.yellowTimer = std::max(0.f, sv.yellowTimer - dt);

        for (auto& v : sim.vehicles) {
            if (!vRender.count(v.id)) continue;
            VehicleRender& vr = vRender[v.id];
            if (v.status == ARRIVED) { vr.active = false; continue; }
            vr.active = true;
            if (v.status == WAITING) {
                vr.stopLinePos = computeStopLine(v.currentNode, v.currentRoad, sim.graph);
                if (v.hasPath()) {
                    int nx = v.getNextNode();
                    if (nx >= 0 && nodePos.count(v.currentNode) && nodePos.count(nx))
                        vr.angle = dirAngle(nodePos[v.currentNode], nodePos[nx]);
                }
                bool green = isGreenForVehicle(v, sim.signals, sim.graph);
                sf::Vector2f tgt = green ? (nodePos.count(v.currentNode) ? nodePos[v.currentNode] : vr.lastTarget) : vr.stopLinePos;
                vr.pos += (tgt - vr.pos) * lerpF(dt, green ? 8.f : 10.f);
                vr.colourBlend += (green ? 0.f - vr.colourBlend : 1.f - vr.colourBlend) * lerpF(dt, green ? 6.f : 5.f);
            }
            else {
                vr.colourBlend += (0.f - vr.colourBlend) * lerpF(dt, 12.f);
                int rid = v.currentRoad;
                if (rid >= 0 && rid < (int)sim.graph.roads.size()) {
                    const Road& r = sim.graph.roads[rid];
                    if (nodePos.count(r.source) && nodePos.count(r.destination)) {
                        sf::Vector2f A = nodePos[r.source], B = nodePos[r.destination];
                        float prog = v.entryTravelTime > 0 ? 1 - (float)(v.remainingTravelTime / v.entryTravelTime) : 0;
                        prog = std::max(0.f, std::min(1.f, prog));
                        sf::Vector2f dir = vnorm(B - A), perp(-dir.y, dir.x);
                        sf::Vector2f tgt = A + (B - A) * prog + perp * 6.f;
                        vr.angle = dirAngle(A, B); vr.lastTarget = tgt;
                        vr.pos += (tgt - vr.pos) * lerpF(dt, 9.f);
                    }
                }
            }
        }

        window.clear({ 12,12,20 });
        for (int gx = 0;gx < WIDTH;gx += 60) { sf::Vertex l[2] = { {{(float)gx,0},{20,25,40,35}},{{(float)gx,(float)HEIGHT},{20,25,40,35}} }; window.draw(l, 2, sf::PrimitiveType::Lines); }
        for (int gy = 0;gy < HEIGHT;gy += 60) { sf::Vertex l[2] = { {{0,(float)gy},{20,25,40,35}},{{(float)WIDTH,(float)gy},{20,25,40,35}} }; window.draw(l, 2, sf::PrimitiveType::Lines); }

        for (auto& r : sim.graph.roads) {
            if (!nodePos.count(r.source) || !nodePos.count(r.destination)) continue;
            float cong = r.capacity > 0 ? std::min(1.f, (float)r.currentFlow / (float)r.capacity) : 0;
            drawRoad(window, nodePos[r.source], nodePos[r.destination], cong, r.capacity == 0);
        }
        for (auto& [nid, node] : sim.graph.nodes) {
            if (!nodePos.count(nid)) continue;
            string lbl = NODE_LABELS.count(nid) ? NODE_LABELS.at(nid) : to_string(nid);
            drawNode(window, nodePos[nid], lbl, font, fontLoaded);
        }
        for (auto& [nid, sv] : sigVis)
            if (nodePos.count(nid)) drawNodeSignals(window, nid, sim.graph, sim.signals, sv);

        for (int pass = 0;pass < 2;pass++)
            for (auto& v : sim.vehicles) {
                if (v.status == ARRIVED || !vRender.count(v.id)) continue;
                const VehicleRender& vr = vRender[v.id];
                if (!vr.active) continue;
                bool mov = (v.status == MOVING);
                if (pass == 0 && mov || pass == 1 && !mov) continue;
                drawCar(window, vr.pos, vr.angle, vehColor(v.id, v.status, vr.colourBlend), v.status == WAITING);
            }

        drawHUD(window, font, fontLoaded, sim, stepSpeed);
        drawLegend(window, font, fontLoaded);

        if (fontLoaded) {
            sf::RectangleShape bar({ (float)WIDTH,22.f }); bar.setFillColor({ 6,8,22,200 });
            bar.setPosition({ 0.f,(float)HEIGHT - 22.f }); window.draw(bar);
            sf::Text bt(font, "TRAFFIC FLOW OPTIMIZER  |  Pakistan Road Network Simulation", 11);
            bt.setFillColor({ 80,120,220,180 }); bt.setPosition({ 10.f,(float)HEIGHT - 17.f }); window.draw(bt);
            sf::Text bk(font, "[SPACE] toggle speed    [ESC] quit", 11);
            bk.setFillColor({ 60,90,160,160 });
            sf::FloatRect br = bk.getLocalBounds();
            bk.setPosition({ (float)WIDTH - br.size.x - 12.f,(float)HEIGHT - 17.f }); window.draw(bk);
        }
        window.display();
    }
    return 0;
}