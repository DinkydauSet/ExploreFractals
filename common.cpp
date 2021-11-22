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

string to_string(double_c c, std::streamsize precision = 5, bool fixed_ = true) {
	stringstream ss;
	if (fixed_) ss << fixed;
	ss << setprecision(precision) << real(c) << " + " << imag(c) << "i";
	return ss.str();
}

bool isfinite(double_c c) {
	return isfinite(real(c)) && isfinite(imag(c));
}


//Constants

constexpr double PROGRAM_VERSION = 9.2;
constexpr uint NUMBER_OF_TRANSFORMATIONS = 7 + 1;
constexpr uint MAXIMUM_TILE_SIZE = 50; //tiles in renderSilverRect smaller than this do not get subdivided.
constexpr uint NEW_TILE_THREAD_MIN_PIXELS = 8; //For tiles with a width or height in PIXELS smaller than this no new threads are created, which has two reasons: 1. thread overhead; 2. See the explanation of stop_creating_threads in the function Render::renderSilverRect.
constexpr double pi = 3.1415926535897932384626433832795;


//Global variables
uint NUMBER_OF_THREADS;
bool using_avx = false;

mutex threadCountChange;
mutex drawingBitmap;


//Procedures definition

namespace ProcedureKind {
	enum {
		Mandelbrot
		,Other
	};
}

struct Procedure {
	/*
		from https://stackoverflow.com/a/37876799
		This struct is a string-like type that can be declared as a constexpr. This is needed to be able to declare a Formula, which contains a name, as constexpr. I want to do that because procedure properties are constant values known at compile time.
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
	}
	//not found
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

typedef uint32_t ARGB;

inline ARGB rgb(uint8 r, uint8 g, uint8 b) {
	return ((uint)(((uint8)(b)|((uint16)((uint8)(g))<<8))|(((uint)(uint8)(r))<<16)));
}

inline uint8 getRValue(ARGB argb) {
	return ((uint8)(((uint64)((argb)>>16)) & 0xff));
}

inline uint8 getGValue(ARGB argb) {
	return ((uint8)(((uint64)(((uint16)(argb)) >> 8)) & 0xff));
}

inline uint8 getBValue(ARGB argb) {
	return ((uint8)(((uint64)(argb)) & 0xff));
}

inline ARGB rgbColorAverage(ARGB c1, ARGB c2, double ratio) {
	//todo: I had to disable these asserts because they can fail when the parameters are changed during a render, which is fine. However, I'm not sure if it's good design to allow that. Most importantly I want speed. The program should not hang for a long time waiting for renders to cancel.
	//assert(ratio >= 0);
	//assert(ratio <= 1);
	return rgb(
		(uint8)((getRValue(c1))*(1 - ratio) + (getRValue(c2))*ratio) | 1,
		(uint8)((getGValue(c1))*(1 - ratio) + (getGValue(c2))*ratio) | 1,
		(uint8)((getBValue(c1))*(1 - ratio) + (getBValue(c2))*ratio) | 1
	);
}


/*
This class is used in FractalCanvas to avoid depending on the windows api to resize the bitmap. Instead, it depends on BitmapManager. Because any derivation of BitmapManager can be used, that makes FractalCanvas compatible with any bitmap that has a pointer to 4-byte RGBA-values (represented by a uint here).
*/
class BitmapManager {
public:
	virtual ARGB* realloc(uint newScreenWidth, uint newScreenHeight) = 0;
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
	//virtual void drawBitmap(void* canvas) = 0;
	virtual void renderStarted(shared_ptr<RenderInterface> render) = 0;
	virtual void renderFinished(shared_ptr<RenderInterface> render) = 0;
	virtual void bitmapRenderStarted(void* canvas, uint bitmapRenderID) = 0;
	virtual void bitmapRenderFinished(void* canvas, uint bitmapRenderID) = 0;
	virtual void parametersChanged(void* canvas, int source_id) = 0;
	virtual void canvasSizeChanged(void* canvas) = 0;
	virtual void canvasResizeFailed(void* canvas, ResizeResult result) = 0;
};

#endif