#pragma once
#include <iostream>
#include <string>
#include <ctime>
#include <vector>
using namespace std;
//Helper functions random generation

class Utility {
public:
	static void initRandom() { srand((unsigned int)time(0)); }
	//Random number in range
	static int randomInt(int low, int high) {
		if (high < low) return low;
		int range = high - low + 1;   // how many numbers
		int r = rand() % range;       // number from 0 to range-1
		return low + r;               // shift to desired range
	}

	// Print a divider line
	static void printDivider(char c = '-', int len = 50) {
		for (int i = 0; i < len; i++) cout << c;
		cout << endl;
	}

	// Print a section header
	static void printHeader(string title) {
		printDivider('=');
		cout << "  " << title << endl;
		printDivider('=');
	}

	// Print a step header
	static void printStepHeader(int step) {
		cout << endl;
		printDivider('-');
		cout << "  SIMULATION STEP " << step << endl;
		printDivider('-');
	}

	
	static string formatDouble(double val) {
		return to_string(val).substr(0, to_string(val).find('.') + 3);
	}
};

