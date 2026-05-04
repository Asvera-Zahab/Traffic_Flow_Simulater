#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

using namespace std;

class TrafficFormula {
public:
   // Section 4.4:  Travel Time Model  wij_free = lij / vij_max
   // length: road length (km), maxSpeed: max speed (km/h)
   // Returns travel time in simulation steps 
	static double freeTravelTime(double length, double maxSpeed) {
		if (maxSpeed <= 0) return 999.0; //INVALID
		double timeInHours = length / maxSpeed;
		double steps = timeInHours * 60.0; // convert to minutes :: steps
		if (steps < 1.0) steps = 1.0;
		return steps;
	}

	// Section 4.4:  Travel Time Model  
	// wij(t) = wij_free * (1 + alpha * (fij/cij)^beta)
	// alpha=0.15, beta=4
	static double congestedTravelTime(double freeTT, int flow, int capacity, double alpha = 0.15, double beta = 4.0) {
		if (capacity <= 0) return freeTT;
		double ratio = (double)flow / (double)capacity;
		double tt = freeTT * (1.0 + alpha * pow(ratio, beta));
		if (tt < 1.0) tt = 1.0;
		return tt;
	}

	// Section 4.3: Congestion Model
	// rho = fij(t) / cij
	// rho = 0: free road, rho = 1: congested
	static double congestionLevel(int flow, int capacity) {
		if (capacity <= 0) return 0.0;
		return (double)flow / (double)capacity;
	}

	// Section 4.2: Traffic Flow Model  Update
    // fij(t+1) = fij(t) + aij(t) - xij(t)
    // currentFlow: fij(t), arrivals: aij(t), departures: xij(t)
	static int updatedFlow(int currentFlow, int arrivals, int departures) {
		int updated = currentFlow + arrivals - departures;
		if (updated < 0) updated = 0;
		return updated;
	}

	// Section 4.2: Traffic Flow Model   Queue Update
    // Qij(t+1) = Qij(t) + xij(t) - dij(t)
    // currentQueue: Qij(t), incoming: xij(t), released: dij(t)
	static int updatedQueue(int currentQueue, int incoming, int released) {
		int updated = currentQueue + incoming - released;
		if (updated < 0) updated = 0;
		return updated;
	}

	// Section 4.2: Traffic Flow Model  Queue Release
	// dij(t) = gij(t) * min(Qij(t), muij, cjk - fjk(t))
	// signalGreen: 1=green, 0=red
	// queue: Qij(t), dischargeRate: muij
	// nextAvailCap: cjk - fjk(t) (space on next road)
	static int queueRelease(int signalGreen, int queueCount, int dischargeRate, int nextAvailableCapacity) {
		if (signalGreen == 0) return 0;  //If red light no cars move
		int release = min(queueCount, min((int)dischargeRate, nextAvailableCapacity));
		if (release < 0) release = 0;
		return release;
	}

	// Section 4.10: Average Travel Time
	// (1/N) * sum(Tsd)
	static double averageTravelTime(vector<int>& travelTimes) {
		if (travelTimes.empty()) return 0.0;
		double sum = 0.0;
		for (int t : travelTimes) sum += t;
		return sum / (double)travelTimes.size();
	}

	// Section 4.10: Total Delay metric
    // sum(Tsd - Tsd_free)
    // travelTimes: actual times, freeTimes: ideal free flow times
	//d>0 traffic caused delay (slower than ideal)
	//d=0 perfect flow
	static double totalDelay(vector<int>& travelTimes, vector<double>& freeTimes) {
		double delay = 0.0;

		// Use OF smaller size to avoid out-of-bounds access
		int n = min(travelTimes.size(), freeTimes.size());

		for (int i = 0; i < n; i++) {
			double d = travelTimes[i] - freeTimes[i];
			if (d > 0) {
				delay += d;
			}
		}

		return delay;
	}

	// Section 4.10: Average Congestion Level
	// (1/|E|) * sum(fij / cij)
	//E is size
	static double averageCongestion(vector<double>& congestions) {
		if (congestions.empty()) return 0.0;
		double sum = 0.0;
		for (double c : congestions) sum += c;
		return sum / congestions.size();
	}

	// Section 4.9: Objective Function (for display)
    //min sum_t sum_ij [ alpha*Qij(t) + beta*(fij/cij)^2 ]
	//balances Queue lengths and congestion level
	static double objectiveValue(int queueLen, int flow, int capacity,
		double alpha = 0.15, double beta = 4.0) {
		double cong;
		if (capacity > 0) cong = (double)flow / capacity;
		else cong = 0.0;
		return alpha * queueLen + beta * cong * cong;
	}

};
