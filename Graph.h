#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include <map>
#include "Node.h"
#include "Road.h"
#include "TrafficFormula.h"

using namespace std;
// Graph.h - Road network as a directed graph G = (V, E)
// Section 4.1: Road Network Model
// Dijkstra's shortest path (Section 4.7)

const double INF = 1e18; // Infinity for Dijkstra

class Graph {
public:
    map<int, Node> nodes;          // nodeId Node object
    vector<Road> roads;            // All road objects
    map<int, vector<int>> adjList; // nodeId list of road indices going OUT

    // Add an intersection (vertex) to the graph
    void addVertex(int nodeId, string name = "") {
        if (nodes.find(nodeId) == nodes.end()) {
            nodes[nodeId] = Node(nodeId, name);
            adjList[nodeId] = vector<int>();
        }
    }

    // Add a directed road (edge) between two nodes
    int addEdge(int src, int dst, double length, double maxSpeed, int capacity, double discharge = 3.0) {
        // Make sure both nodes exist
        addVertex(src);
        addVertex(dst);

        int roadId = (int)roads.size();
        Road r(roadId, src, dst, length, maxSpeed, capacity, discharge);
        roads.push_back(r);

        // Update adjacency list
        adjList[src].push_back(roadId);

        // Track road in nodes
        nodes[src].addOutgoingRoad(roadId);
        nodes[dst].addIncomingRoad(roadId);

        return roadId;
    }

    // Find road index between src and dst (-1 if not found)
    int findRoadIndex(int src, int dst) const {
        auto it = adjList.find(src);
        if (it == adjList.end()) return -1;
        for (int rid : it->second) {
            if (roads[rid].destination == dst) return rid;
        }
        return -1;
    }

    // Get all neighbor node IDs of a given node
    vector<int> getNeighbors(int nodeId) const {
        vector<int> neighbors;
        auto it = adjList.find(nodeId);
        if (it == adjList.end()) return neighbors;
        for (int rid : it->second) {
            neighbors.push_back(roads[rid].destination);
        }
        return neighbors;
    }

    // Get available capacity on road between src->dst
    int getAvailableCapacity(int src, int dst) const {
        int rid = findRoadIndex(src, dst);
        if (rid < 0) return 0;
        int avail = roads[rid].capacity - roads[rid].currentFlow;
        if (avail > 0) { return avail; }
        else { return 0; }
    }

    // Display full graph structure
    void displayGraph() const {
        cout << "\n=== ROAD NETWORK [BY GRAPH]===" << endl;
        cout << "Nodes: " << nodes.size() << " | Roads: " << roads.size() << endl;
        for (auto& kv : nodes) {
            kv.second.display();
        }
        cout << "\nRoad Details:" << endl;
        for (auto& r : roads) {
            r.display();
        }
    }

    // Display current traffic state (for each step summary)
    void displayTrafficState() const {
        for (auto& r : roads) {
            cout << " [BY GRAPH] Road " << r.source << "->" << r.destination
                << " | Flow: " << r.currentFlow << "/" << r.capacity
                << " | Queue: " << r.queueCount
                << " | Congestion: " << r.congestion
                << " | TT: " << r.travelTime << endl;
        }
    }

    // Section 4.7: Dijkstra's Shortest Path Algorithm
    // cost(eij) = wij(t) (current travel time)
    vector<int> shortestPathDijkstra(int startNode, int endNode) const {
        priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> pq;

        map<int, double> dist;
        map<int, int> prev;

        // Initialize all distances to infinity
        for (auto& kv : nodes) {
            dist[kv.first] = INF;
            prev[kv.first] = -1;
        }

        dist[startNode] = 0.0;
        pq.push({ 0.0, startNode });

        while (!pq.empty()) {
            auto [cost, u] = pq.top();
            pq.pop();

            // Skip if we already found a better path
            if (cost > dist[u]) continue;
            if (u == endNode) break;

            auto it = adjList.find(u);
            if (it == adjList.end()) continue;

            for (int rid : it->second) {
                const Road& r = roads[rid];
                int v = r.destination;
                double newDist = dist[u] + r.travelTime;

                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    prev[v] = u;
                    pq.push({ newDist, v });
                }
            }
        }

        // Reconstruct path
        vector<int> path;
        if (dist[endNode] == INF) return path;

        int cur = endNode;
        while (cur != -1) {
            path.push_back(cur);
            cur = prev[cur];
        }

        for (int i = 0, j = (int)path.size() - 1; i < j; i++, j--)
            swap(path[i], path[j]);

        return path;
    }
};