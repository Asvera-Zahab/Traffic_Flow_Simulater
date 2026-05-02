#pragma once
#include <iostream>
#include <vector>
using namespace std;

//Representation of an intersection on a road Network.
//Each node has outgoing, ingoing road and signal state of incoming road.

class Node {
public:
	int id; //Unique id of intersetion
	string name; //name of intersection
	vector<int> incomingRoads; //ids of incoming road
	vector<int> outgoingRoads; //ids of outgoing road
	int greenRoadIndex; //which road has green signal
	int signalTimer; //Step green signal has been active

	Node() { id = -1;name = ""; greenRoadIndex = -1; signalTimer = 0;}
	Node(int nodeId, string nodeName = "") { id = nodeId; name = nodeName; greenRoadIndex = -1; signalTimer = 0; }
	void addIncomingRoad(int roadId) { incomingRoads.push_back(roadId); }
	void addOutgoingRoad(int roadId) { outgoingRoads.push_back(roadId); }

	void display() const {
		cout << "  Node " << id;
		cout << " (" << name << ")";
		cout << " | Incoming roads: " << incomingRoads.size() << " | Outgoing roads: " << outgoingRoads.size() << endl;
	}
};
