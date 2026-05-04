#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "Road.h"
#include "Vehicle.h"

using namespace std;
//Handles all file input/output operations
//.txt and .dat files

//road data 
struct RoadRecord {
    int id;
    int source;
    int destination;
    double length;
    double maxSpeed;
    int capacity;
    double dischargeRate;
    int currentFlow;
    int queueCount;
    double congestion;
    double travelTime;
};

//vehicle
struct VehicleRecord {
    int id;
    int source;
    int destination;
    int currentNode;
    int status;    // 0=WAITING, 1=MOVING, 2=ARRIVED
    int stepEntered;
    int stepArrived;
    double remainingTravelTime;
};

class FileManager {
public:
    //road data
    static void saveRoadData(vector<Road>& roads, string filename = "roads.dat") {
        ofstream file(filename, ios::binary | ios::out);
        if (!file.is_open()) {
            cout << "[FileManager] ERROR: Cannot open " << filename << endl;
            return;
        }
        int count = (int)roads.size();
        file.write((char*)&count, sizeof(int));  
        for (Road& r : roads) {
            RoadRecord rec;
            rec.id = r.id;
            rec.source = r.source;
            rec.destination = r.destination;
            rec.length = r.length;
            rec.maxSpeed = r.maxSpeed;
            rec.capacity = r.capacity;
            rec.dischargeRate = r.dischargeRate;
            rec.currentFlow = r.currentFlow;
            rec.queueCount = r.queueCount;
            rec.congestion = r.congestion;
            rec.travelTime = r.travelTime;
            file.write((char*)&rec, sizeof(RoadRecord));
        }
        file.close();
        cout << "[FileManager] Saved " << count << " roads to " << filename << endl;
    }
    static bool loadRoadData(vector<Road>& roads, string filename = "roads.dat") {
        ifstream file(filename, ios::binary | ios::in);
        if (!file.is_open()) {
            cout << "[FileManager] No existing " << filename << " found. Starting fresh." << endl;
            return false;
        }

        int count = 0;
        file.read((char*)&count, sizeof(int));

        roads.clear();
        for (int i = 0; i < count; i++) {
            RoadRecord rec;
            file.read((char*)&rec, sizeof(RoadRecord));
            Road r(rec.id, rec.source, rec.destination, rec.length, rec.maxSpeed, rec.capacity, rec.dischargeRate);
            r.currentFlow = rec.currentFlow;
            r.queueCount = rec.queueCount;
            r.congestion = rec.congestion;
            r.travelTime = rec.travelTime;
            roads.push_back(r);
        }
        file.close();
        cout << "[FileManager] Loaded " << count << " roads from " << filename << endl;
        return true;
    }

    //vehicle data
    static void saveVehicleData(vector<Vehicle>& vehicles, string filename = "vehicles.dat") {
        ofstream file(filename, ios::binary | ios::out);
        if (!file.is_open()) {
            cout << "[FileManager] ERROR: Cannot open " << filename << endl;
            return;
        }

        int count = (int)vehicles.size();
        file.write((char*)&count, sizeof(int));

        for (Vehicle& v : vehicles) {
            VehicleRecord rec;
            rec.id = v.id;
            rec.source = v.source;
            rec.destination = v.destination;
            rec.currentNode = v.currentNode;
            rec.status = (int)v.status;
            rec.stepEntered = v.stepEntered;
            rec.stepArrived = v.stepArrived;
            rec.remainingTravelTime = v.remainingTravelTime;
            file.write((char*)&rec, sizeof(VehicleRecord));
        }
        file.close();
        cout << "[FileManager] Saved " << count << " vehicles to " << filename << endl;
    }
    static bool loadVehicleData(vector<Vehicle>& vehicles, string filename = "vehicles.dat") {
        ifstream file(filename, ios::binary | ios::in);
        if (!file.is_open()) {
            cout << "[FileManager] No existing " << filename << " found. Starting fresh." << endl;
            return false;
        }

        int count = 0;
        file.read((char*)&count, sizeof(int));

        vehicles.clear();
        for (int i = 0; i < count; i++) {
            VehicleRecord rec;
            file.read((char*)&rec, sizeof(VehicleRecord));
            Vehicle v(rec.id, rec.source, rec.destination, rec.stepEntered);
            v.currentNode = rec.currentNode;
            v.status = rec.status;
            v.stepArrived = rec.stepArrived;
            v.remainingTravelTime = rec.remainingTravelTime;
            vehicles.push_back(v);
        }
        file.close();
        cout << "[FileManager] Loaded " << count << " vehicles from " << filename << endl;
        return true;
    }

    //simulation state
    static void saveTrafficState(int step, vector<Road>& roads, string filename = "traffic_state.dat") {
        ofstream file(filename, ios::binary | ios::out);
        if (!file.is_open()) return;
        file.write((char*)&step, sizeof(int));
        int count = (int)roads.size();
        file.write((char*)&count, sizeof(int));
        for (Road& r : roads) {
            file.write((char*)&r.currentFlow, sizeof(int));
            file.write((char*)&r.queueCount, sizeof(int));
            file.write((char*)&r.congestion, sizeof(double));
            file.write((char*)&r.travelTime, sizeof(double));
        }
        file.close();
    }

    //Written in file
    static void exportReport(int totalSteps, int completed, int waiting,double avgTravelTime, double totalDelay,
        double throughput, double avgCongestion,int mostCongestedRoad, int mostBusyNode,string filename = "report.txt") {
        ofstream file(filename, ios::out);
        if (!file.is_open()) {
            cout << "[FileManager] ERROR: Cannot open " << filename << endl;
            return;
        }
        file << "[FILE MANAGER]" << endl;
        file << "------------------------------------" << endl;
        file << "  TRAFFIC SIMULATION REPORT  " << endl;
        file << "------------------------------------" << endl;
        file << "Total Simulation Steps : " << totalSteps << endl;
        file << "Vehicles Completed     : " << completed << endl;
        file << "Vehicles Still Waiting : " << waiting << endl;
        file << "-----------------------------------" << endl;
        file << "Average Travel Time    : " << avgTravelTime << " steps" << endl;
        file << "Total Delay            : " << totalDelay << " steps" << endl;
        file << "Throughput             : " << throughput << " vehicles/step" << endl;
        file << "Average Congestion     : " << avgCongestion << endl;
        file << "Most Congested Road ID : " << mostCongestedRoad << endl;
        file << "Most Busy Node ID      : " << mostBusyNode << endl;
        file << "------------------------------------" << endl;
        file.close();
        cout << "[FileManager] Report saved to " << filename << endl;
    }

    static void appendTrafficLog(int step, int moving, int waiting, int completed,double avgCongestion, double avgTT,string filename = "traffic_log.txt") {
        //new data after old data else it overwrites
        ofstream file(filename, ios::out | ios::app);
        if (!file.is_open()) return;
        file << "STEP [BY FILE MANAGER]" << step << endl;
        file << "  Vehicles Moving   : " << moving << endl;
        file << "  Vehicles Waiting  : " << waiting << endl;
        file << "  Completed         : " << completed << endl;
        file << "  Avg Congestion    : " << avgCongestion << endl;
        file << "  Avg Travel Time   : " << avgTT << endl;
        file << "-----------------------------" << endl;
        file.close();
    }

    static void saveRoadsTxt(int step, vector<Road>& roads, string filename = "roads.txt") {
        ofstream file(filename, ios::out | ios::app);
        if (!file.is_open()) return;
        file << "STEP [BY FILE MANAGER]" << step << endl;
        for (Road& r : roads) {
            file << "Road " << r.source << "->" << r.destination
                << " Flow: " << r.currentFlow
                << " Queue: " << r.queueCount
                << " Congestion: " << r.congestion
                << " TravelTime: " << r.travelTime << endl;
        }
        file << endl;
        file.close();
    }

    static void saveVehiclesTxt(vector<Vehicle>& vehicles, string filename = "vehicles.txt") {
        ofstream file(filename, ios::out);
        if (!file.is_open()) return;
        file << "---VEHICLE COMPLETION FILE [BY FILE MANAGER] ---" << endl;
        for (Vehicle& v : vehicles) {
            file << "Vehicle " << v.id<< " | " << v.source << "->" << v.destination
                << " | Status: " << v.getStatusString();
            if (v.stepArrived >= 0)
                file << " | TravelTime: " << v.getTravelTime() << " steps";
            file << endl;
        }
        file.close();
        cout << "[FileManager] Vehicle log saved to " << filename << endl;
    }

    // Clear a log file at start of simulation
    static void clearLogFile(string filename) {
        ofstream file(filename, ios::out | ios::trunc);
        file.close();
    }
};

