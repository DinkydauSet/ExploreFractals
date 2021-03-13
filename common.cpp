//Visual Studio requires this for no clear reason
#include "stdafx.h"

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <complex>
#include <mutex>


#ifndef _common_
#define _common_

#ifdef NDEBUG
const bool debug = false;
#else
const bool debug = true;
#endif

using namespace std;

HWND hWndMain; //main window

typedef unsigned int uint;
typedef unsigned char uchar;

typedef std::complex<double> double_c;
const double_c I(0, 1);


//Constants
const double PROGRAM_VERSION = 6.1;
const int NUMBER_OF_TRANSFORMATIONS = 7 + 1;
int NUMBER_OF_COLORS = 4; //must be power of 2 to use "AND" to calculate the color array index


//I wonder if it's good to have global mutexes while using multiple instances of FractalCanvas. It makes no sense that one fractalcanvas can't resize while another one is rendering, for example,  although multiple renders at once is questionable.
mutex threadCountChange;
mutex renders;
mutex drawingBitmap;
mutex renderingBitmap;


//Formulas
struct Formula {
	int identifier;
	bool isGuessable;
	int inflectionPower;
	bool isEscapeTime; //This means the procedure is iterating a Mandelbrot type formula (z->f(z, c))
	double escapeRadius;
	string name;
};

//Formula identifiers (also menu options)
const int PROCEDURE_M2 = 4;
const int PROCEDURE_M3 = 6;
const int PROCEDURE_M4 = 7;
const int PROCEDURE_M5 = 8;
const int PROCEDURE_BURNING_SHIP = 5;
const int PROCEDURE_CHECKERS = 12;
const int PROCEDURE_TEST_CONTROL = 15;
const int PROCEDURE_HIGH_POWER = 13;
const int PROCEDURE_TRIPLE_MATCHMAKER = 11;

//identifier; isGuessable; inflectionPower, likeMandelbrot, escapeRadius, name
const Formula M2 = { PROCEDURE_M2, true, 2, true, 4, "Mandelbrot power 2" };
const Formula M3 = { PROCEDURE_M3, true, 3, true, 2, "Mandelbrot power 3" };
const Formula M4 = { PROCEDURE_M4, true, 4, true, pow(2, 2 / 3.), "Mandelbrot power 4" }; //Escape radius for Mandelbrot power n: pow(2, 2/(n-1))
const Formula M5 = { PROCEDURE_M5, true, 5, true, pow(2, 2 / 4.), "Mandelbrot power 5" };
const Formula BURNING_SHIP = { PROCEDURE_BURNING_SHIP, false, 2, true, 4, "Burning ship" };
const Formula CHECKERS = { PROCEDURE_CHECKERS, true, 2, false, 4, "Checkers" };
const Formula TRIPLE_MATCHMAKER = { PROCEDURE_TRIPLE_MATCHMAKER, true, 2, false, 550, "Triple Matchmaker" };
const Formula HIGH_POWER = { PROCEDURE_HIGH_POWER, true, 2, true, 4, "High power Mandelbrot" };
const Formula TEST_CONTROL = { PROCEDURE_TEST_CONTROL, true, 2, false, 4, "Test" };

Formula getFormulaObject(int identifier) {
	switch (identifier) {
	case PROCEDURE_M2:
		return M2;
	case PROCEDURE_M3:
		return M3;
	case PROCEDURE_M4:
		return M4;
	case PROCEDURE_M5:
		return M5;
	case PROCEDURE_BURNING_SHIP:
		return BURNING_SHIP;
	case PROCEDURE_CHECKERS:
		return CHECKERS;
	case PROCEDURE_TRIPLE_MATCHMAKER:
		return TRIPLE_MATCHMAKER;
	case PROCEDURE_HIGH_POWER:
		return HIGH_POWER;
	case PROCEDURE_TEST_CONTROL:
		return TEST_CONTROL;
	}
	//Not found:
	Formula f; f.identifier = -1;
	return f;
}

#endif