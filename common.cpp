/*
    ExploreFractals, a tool for testing the effect of Mandelbrot set Julia morphings
    Copyright (C) 2021  DinkydauSet

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef COMMON_H
#define COMMON_H

//standard library
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cassert>
#include <complex>
#include <mutex>
#include <cstdint>
#include <chrono>
#include <thread>
#include <cmath>

#ifndef NDEBUG
constexpr bool debug = true;
#else
constexpr bool debug = false;
#endif

using namespace std;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef unsigned int uint;

// double_c type definition

typedef std::complex<double> double_c;
constexpr double_c I(0, 1);

bool isfinite(double_c c) {
	return isfinite(real(c)) && isfinite(imag(c));
}

string to_string(double_c c, std::streamsize precision = 5, bool fixed_ = true) {
	stringstream ss;
	if (fixed_) ss << fixed;
	ss << setprecision(precision) << real(c) << " + " << imag(c) << "i";
	return ss.str();
}


//Constants

constexpr double PROGRAM_VERSION = 10.1;
constexpr uint NUMBER_OF_TRANSFORMATIONS = 7 + 1;
//todo: remove if the old mariani silver algortihm is not used anymore
constexpr uint MAXIMUM_TILE_SIZE = 50; //tiles in renderSilverRect smaller than this do not get subdivided.
constexpr uint NEW_TILE_THREAD_MIN_PIXELS = 8; //For tiles with a width or height in PIXELS smaller than this no new threads are created, which has two reasons: 1. thread overhead; 2. See the explanation of stop_creating_threads in the function Render::renderSilverRect.
constexpr uint WORK_STORAGE_SIZE = 256; // how much work (points to calculate) worker threads receive from the work distribution function (used in the Render class)
constexpr double pi = 3.1415926535897932384626433832795;


//Global variables
uint NUMBER_OF_THREADS;
bool using_avx = false;

mutex threadCountChange;
mutex drawingBitmap;


//Procedures definition

namespace ProcedureKind { enum {
	Mandelbrot
	,Other
};}

struct Procedure {
	/*
		from https://stackoverflow.com/a/37876799
		This struct is a string-like type that can be declared as a constexpr. This is needed to be able to declare a Formula, which contains a name, as constexpr. I want to do that because procedure properties are constant values known at compile time and I use them as template parameters.
		C++20 will allow string to be declared as constexpr, so this trick can be removed when C++20 has good compiler support.
	*/
	struct constexpr_str {
		char const* str;
		std::size_t size;

		// can only construct from a char[] literal
		template <std::size_t N>
		constexpr constexpr_str(char const (&s)[N])
			: str(s)
			, size(N - 1) // not count the trailing nul
		{}
	};

	int id;
	bool guessable;
	int inflectionPower;
	constexpr_str name_dont_use; //use the name() function to get the name
	bool hasJuliaVersion;
	bool hasAvxVersion;
	int kind;

	string name() {
		return string(name_dont_use.str);
	}
};

/*
	All procedures / formulas in the program
	The id numbers should never be changed to keep backwards compatibility with parameter files.

	The steps to add a new procedure:
	 1. Add a constexpr Procedure to the list below.
	 2. Add it to getProcedureObject.
	 3. Add a case to the switch in FractalCanvas::createNewRender.
	 4. Define the calculations that should be done when this procedure is used in Render::calcPoint. An AVX implementation should be placed in Render::calcPointVector. Note: hasJuliaVersion and hasAvxVersion should correspond with the real situation. An AVX implementation is not used if hasAvxVersion is false even if there is one. The same goes for julia versions.
	 5. To make it available through menu options: add the ID to the const vector<int> procedureIDs in GUI.cpp
*/
//                                      id   guessable inflection-  name                    hasJulia-  hasAvx-    procedureClass
//                                                      Power                                 Version    Version
constexpr Procedure NOT_FOUND =         {-1,  false,   -1,          "Procedure not found",  false,     false,     ProcedureKind::Other };
constexpr Procedure M2 =                { 4,  true,     2,          "Mandelbrot power 2",   true,      true,      ProcedureKind::Mandelbrot };
constexpr Procedure BURNING_SHIP =      { 5,  false,    2,          "Burning ship",         true,      false,     ProcedureKind::Other };
constexpr Procedure M3 =                { 6,  true,     3,          "Mandelbrot power 3",   true,      false,     ProcedureKind::Mandelbrot };
constexpr Procedure M4 =                { 7,  true,     4,          "Mandelbrot power 4",   true,      false,     ProcedureKind::Mandelbrot };
constexpr Procedure M5 =                { 8,  true,     5,          "Mandelbrot power 5",   true,      false,     ProcedureKind::Mandelbrot };
constexpr Procedure TRIPLE_MATCHMAKER = { 11, true,     2,          "Triple Matchmaker",    true,      false,     ProcedureKind::Other };
constexpr Procedure CHECKERS =          { 12, true,     2,          "Checkers",             false,     false,     ProcedureKind::Other };
constexpr Procedure HIGH_POWER =        { 13, true,     33554432,   "High power Mandelbrot",true,      false,     ProcedureKind::Mandelbrot };
constexpr Procedure RECURSIVE_FRACTAL = { 15, true,     2,          "Recursive Fractal",    false,     false,     ProcedureKind::Other };
constexpr Procedure PURE_MORPHINGS =    { 17, true,     2,          "Pure Julia morphings", false,     false,     ProcedureKind::Other };
constexpr Procedure M512 =              { 18, true,     512,        "Mandelbrot power 512", true,      false,     ProcedureKind::Mandelbrot };
constexpr Procedure DEBUG_TEST          { 19, false,		2,			"debug test",			false,     false,	  ProcedureKind::Other };

constexpr Procedure getProcedureObject(int id) {
	switch (id) {
		case M2.id:                return M2;
		case M3.id:                return M3;
		case M4.id:                return M4;
		case M5.id:                return M5;
		case BURNING_SHIP.id:      return BURNING_SHIP;
		case CHECKERS.id:          return CHECKERS;
		case TRIPLE_MATCHMAKER.id: return TRIPLE_MATCHMAKER;
		case HIGH_POWER.id:        return HIGH_POWER;
		case RECURSIVE_FRACTAL.id: return RECURSIVE_FRACTAL;
		case PURE_MORPHINGS.id:    return PURE_MORPHINGS;
		case M512.id:              return M512;
		case DEBUG_TEST.id:        return DEBUG_TEST; //todo: remove
	}
	return NOT_FOUND;
}


// these are the transformations used in FractalParameters::post_transformation and ~pre_transformation
string transformation_name(int transformation_id)
{
	switch(transformation_id) {
		case 0: return "None";
		case 1: return "5 Mandelbrot iterations";
		case 2: return "Cosine";
		case 4: return "Square root";
		case 5: return "4th power root";
		case 6: return "Logarithm";
	}
	assert(false); return "";
};


// ARGB color type definition
//The order in the struct is inverted because an ARGB value in a windows API bitmap is interpreted as a little endian integer.
struct ARGB {
	uint8 B;
	uint8 G;
	uint8 R;
	uint8 A;
};

inline constexpr ARGB rgb(uint8 r, uint8 g, uint8 b) {
	return {b, g, r, 255};
}

/*
This class is used in FractalCanvas to avoid depending on the windows api to resize the bitmap. Instead, it depends on BitmapManager. Because any derivation of BitmapManager can be used, that makes FractalCanvas compatible with any bitmap that has a pointer to 4-byte RGBA-values (represented by a uint here).
*/
class BitmapManager {
public:
	virtual ARGB* realloc(uint width, uint height) = 0;
	virtual ~BitmapManager() {}
};

class RenderInterface {
public:
	struct ProgressInfo {
		uint64 guessedPixelCount;
		uint64 calculatedPixelCount;
		double elapsedTime;
		bool ended;
	};
	virtual uint getWidth() = 0;
	virtual uint getHeight() = 0;
	virtual ProgressInfo getProgress() = 0;
	virtual void* canvasPtr() = 0;
	virtual uint getId() = 0;

	virtual ~RenderInterface() {}
};

enum class ResizeResultType {
	Success,
	OutOfRangeError,
	MemoryError
};

struct ResizeResult {
	bool success;
	bool changed;
	ResizeResultType resultType;
};

/*
	a (programming) interface for the graphical interface
	Non-GUI classes can request action by the GUI through this interface.

	"Started" means just that: a render has started.
	"Finished" means that the render was 100% completed. Cancelled renders don't ever finish.

	The int "source_id" is an identifier for the cause / source of the event which the GUI can use to handle it.
*/
class GUIInterface {
public:
	virtual void renderStarted(shared_ptr<RenderInterface> render) = 0;
	virtual void renderFinished(shared_ptr<RenderInterface> render) = 0;
	virtual void bitmapRenderStarted(void* canvas, uint bitmapRenderID) = 0;
	virtual void bitmapRenderFinished(void* canvas, uint bitmapRenderID) = 0;
	virtual void parametersChanged(void* canvas, int source_id) = 0;
	virtual void canvasSizeChanged(void* canvas) = 0;
	virtual void canvasResizeFailed(void* canvas, ResizeResult result) = 0;

	virtual ~GUIInterface() {}
};

//a location in a FractalCanvas
struct point {
	uint x;
	uint y;
};

//macro to easily time pieces of code
#define timerstart { auto start = chrono::high_resolution_clock::now();
#define timerend(name) \
	auto end = chrono::high_resolution_clock::now(); \
	chrono::duration<double> elapsed = end - start; \
	cout << #name << " took " << elapsed.count() << " seconds" << endl; }

#endif