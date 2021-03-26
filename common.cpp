//standard library
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <complex>
#include <mutex>



#ifndef _common_
#define _common_

#ifndef NDEBUG
const bool debug = true;
#else
const bool debug = false;
#endif

using namespace std;

typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef std::complex<double> double_c;
const double_c I(0, 1);
const double pi = 3.1415926535897932384626433832795;

//Constants
const double PROGRAM_VERSION = 7.0;
const int NUMBER_OF_TRANSFORMATIONS = 7 + 1;
const int MAXIMUM_TILE_SIZE = 50; //tiles in renderSilverRect smaller than this do not get subdivided.
const int NEW_TILE_THREAD_MIN_PIXELS = 8; //For tiles with a width or height in PIXELS smaller than this no new threads are created, which has two reasons: 1. thread overhead; 2. See the explanation of stop_creating_threads in the function Render::renderSilverRect.
int BI_CHOICE; //easter egg

//I wonder if it's good to have global mutexes while using multiple instances of FractalCanvas. It makes no sense that one fractalcanvas can't resize while another one is rendering, for example, although multiple renders at once is questionable.
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
const int PROCEDURE_BURNING_SHIP = 5;
const int PROCEDURE_M3 = 6;
const int PROCEDURE_M4 = 7;
const int PROCEDURE_M5 = 8;
const int PROCEDURE_TRIPLE_MATCHMAKER = 11;
const int PROCEDURE_CHECKERS = 12;
const int PROCEDURE_HIGH_POWER = 13;
const int PROCEDURE_TEST_CONTROL = 15;
const int PROCEDURE_BI = 16;

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
const Formula BI = { PROCEDURE_BI, true, 2, false, 4, "Business Intelligence" };

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
	case PROCEDURE_BI:
		return BI;
	}
	//Not found:
	Formula f; f.identifier = -1;
	return f;
}


inline uint rgb(uchar r, uchar g, uchar b) {
	return ((ulong)(((uchar)(b)|((ushort)((uchar)(g))<<8))|(((ulong)(uchar)(r))<<16)));
}

inline uchar getRValue(uint rgb) {
	return ((uchar)(((unsigned long long)((rgb)>>16)) & 0xff));
}

inline uchar getGValue(uint rgb) {
	return ((uchar)(((unsigned long long)(((ushort)(rgb)) >> 8)) & 0xff));
}

inline uchar getBValue(uint rgb) {
	return ((uchar)(((unsigned long long)(rgb)) & 0xff));
}

inline UINT rgbColorAverage(UINT u1, UINT u2, double ratio) {
	assert(ratio >= 0);
	assert(ratio <= 1);
	return rgb(
		(uchar)((getRValue(u1))*(1 - ratio) + (getRValue(u2))*ratio) | 1,
		(uchar)((getGValue(u1))*(1 - ratio) + (getGValue(u2))*ratio) | 1,
		(uchar)((getBValue(u1))*(1 - ratio) + (getBValue(u2))*ratio) | 1
	);
}


/*
This class is used in FractalCanvas to avoid depending on the windows api to resize the bitmap. Instead, it depends on BitmapManager. Because any derivation of BitmapManager can be used, that makes FractalCanvas compatible with any bitmap that has a pointer to 4-byte RGBA-values (represented by a uint here). This class intended only as an abstraction. No instance of BitmapManager should be used, only of its derivations.
*/
class BitmapManager {
public:
	virtual uint* realloc(int newScreenWidth, int newScreenHeight) { assert(false); return (uint*)0; }
	virtual void draw() { assert(false); }
};

#endif