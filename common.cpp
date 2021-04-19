#ifndef COMMON_H
#define COMMON_H

//standard library
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <complex>
#include <mutex>
#include <cstdint>

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

typedef uint32_t ARGB;

typedef std::complex<double> double_c;
constexpr double_c I(0, 1);
constexpr double pi = 3.1415926535897932384626433832795;

//Constants
constexpr double PROGRAM_VERSION = 8.1;
constexpr uint NUMBER_OF_TRANSFORMATIONS = 7 + 1;
constexpr uint MAXIMUM_TILE_SIZE = 50; //tiles in renderSilverRect smaller than this do not get subdivided.
constexpr uint NEW_TILE_THREAD_MIN_PIXELS = 8; //For tiles with a width or height in PIXELS smaller than this no new threads are created, which has two reasons: 1. thread overhead; 2. See the explanation of stop_creating_threads in the function Render::renderSilverRect.
int BI_CHOICE; //easter egg

//I wonder if it's good to have global mutexes while using multiple instances of FractalCanvas. It makes no sense that one fractalcanvas can't resize while another one is rendering, for example, although multiple renders at once is questionable.
mutex threadCountChange;
mutex renders;
mutex drawingBitmap;
mutex renderingBitmap;

namespace ProcedureClass {
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
	constexpr_str name_;
	bool hasJuliaVersion;
	bool hasAvxVersion;
	int procedureClass;

	string name() {
		return string(name_.str);
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
	 5. To make it available through menu options:
	   5.1 Create a new value for it in the enum in the MenuOption namespace.
	   5.2 Use the value in adding the menu in the AddMenus function.
	   5.3 Handle usage of the menu option in the case WM_COMMAND in MainWndProc.
*/
//                                      id   guessable inflection-  name                    hasJulia-  hasAvx-    procedureClass
//                                                      Power                                 Version    Version
constexpr Procedure NOT_FOUND =         {-1,  false,   -1,          "Procedure not found",  false,     false,     ProcedureClass::Other };
constexpr Procedure M2 =                { 4,  true,     2,          "Mandelbrot power 2",   true,      true,      ProcedureClass::Mandelbrot };
constexpr Procedure BURNING_SHIP =      { 5,  false,    2,          "Burning ship",         true,      false,     ProcedureClass::Other };
constexpr Procedure M3 =                { 6,  true,     3,          "Mandelbrot power 3",   true,      false,     ProcedureClass::Mandelbrot };
constexpr Procedure M4 =                { 7,  true,     4,          "Mandelbrot power 4",   true,      false,     ProcedureClass::Mandelbrot };
constexpr Procedure M5 =                { 8,  true,     5,          "Mandelbrot power 5",   true,      false,     ProcedureClass::Mandelbrot };
constexpr Procedure TRIPLE_MATCHMAKER = { 11, true,     2,          "Triple Matchmaker",    true,      false,     ProcedureClass::Other };
constexpr Procedure CHECKERS =          { 12, true,     2,          "Checkers",             false,     false,     ProcedureClass::Other };
constexpr Procedure HIGH_POWER =        { 13, true,     33554432,   "High power Mandelbrot",true,      false,     ProcedureClass::Mandelbrot };
constexpr Procedure RECURSIVE_FRACTAL = { 15, true,     2,          "Recursive Fractal",    false,     false,     ProcedureClass::Other };
constexpr Procedure BI =                { 16, true,     2,          "Business Intelligence",false,     false,     ProcedureClass::Other };
constexpr Procedure PURE_MORPHINGS =    { 17, true,     2,          "Pure Julia morphings", false,     false,     ProcedureClass::Other };
constexpr Procedure M512 =              { 18, true,     512,        "Mandelbrot power 512", true,      false,     ProcedureClass::Mandelbrot };

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
		case BI.id:                return BI;
		case PURE_MORPHINGS.id:    return PURE_MORPHINGS;
		case M512.id:              return M512;
	}
	//not found
	return NOT_FOUND;
}


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
	assert(ratio >= 0);
	assert(ratio <= 1);
	return rgb(
		(uint8)((getRValue(c1))*(1 - ratio) + (getRValue(c2))*ratio) | 1,
		(uint8)((getGValue(c1))*(1 - ratio) + (getGValue(c2))*ratio) | 1,
		(uint8)((getBValue(c1))*(1 - ratio) + (getBValue(c2))*ratio) | 1
	);
}


/*
This class is used in FractalCanvas to avoid depending on the windows api to resize the bitmap. Instead, it depends on BitmapManager. Because any derivation of BitmapManager can be used, that makes FractalCanvas compatible with any bitmap that has a pointer to 4-byte RGBA-values (represented by a uint here). This class intended only as an abstraction. No instance of BitmapManager should be used, only of its derivations.
*/
class BitmapManager {
public:
	virtual ARGB* realloc(int newScreenWidth, int newScreenHeight) { assert(false); return (ARGB*)0; }
	virtual void draw() { assert(false); }
};

struct box {
	double xfrom;
	double xto;
	double yfrom;
	double yto;
};

#endif