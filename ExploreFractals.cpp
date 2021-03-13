//Visual Studio requires this for no clear reason
#include "stdafx.h"

//WinApi
#include <Windowsx.h>
#include <windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <ocidl.h>
#include <olectl.h>
#include <atlbase.h>
#include <shellapi.h>

//C++ standard library
#include <iomanip>		//for setfill and setw
#include <chrono>
#include <thread>
//#include <random>
#include <fstream>
#include <regex>
#include <codecvt>		//to convert utf16 to utf8

//Intrinsics, for using avx instructions
#include <intrin.h>
#include <immintrin.h>


//this program
#include "common.cpp"
#include "FractalParameters.cpp"
#include "FractalCanvas.cpp"
#include "Render.cpp"

#include "windows_util.cpp"



/*
#include <gmp.h>
#include <gmpxx.h>
*/

//Link to libraries
//#pragma comment(lib,"comctl32.lib") //is this important? for InitCommonControls();
//#pragma comment(lib,"C:/msys64/mingw64/lib/libgmp.dll.a")

//Macro
#define CUSTOM_REFRESH 8000 //custom message
//macro to easily time pieces of code
#define timerstart { auto start = chrono::high_resolution_clock::now();
#define timerend(name) \
	auto end = chrono::high_resolution_clock::now(); \
	chrono::duration<double> elapsed = end - start; \
	cout << #name << " took " << elapsed.count() << " seconds" << endl; }




/*
std::mt19937_64 generator;
uniform_real_distribution<double> distribution(0.0, 1.0);

double random() {
	return distribution(generator);
}
*/


//Configurable constants. The values are somewhat arbitrary.
const int MAXIMUM_TILE_SIZE = 50; //tiles in renderSilverRect smaller than this do not get subdivided
//determined at runtime:
const int NEW_TILE_THREAD_MIN_PIXELS = 3; //minimum required number of pixels of both the width and the height of a tile in renderSilverRect



unsigned int NUMBER_OF_THREADS;
bool using_avx = false;


bool firstPaint = true;

//variables related to command line options
bool render_animation = false;
bool render_image = false;
bool interactive = true;
string write_directory = "";
string parameterfile = "default.efp";

HINSTANCE hInst; //application instance
HWND statusBar; //status bar of the main window
HWND hOptions; //options window
HWND hJSON; //JSON window

void setWindowSize(int,int);


static TCHAR szWindowClass[] = _T("win32app"); // The main window class name.  
static TCHAR szTitle[] = _T("ExploreFractals"); // The string that appears in the application's title bar.

static TCHAR TITLE_OPTIONS[] = _T("Options");
static TCHAR CLASS_OPTIONS[] = _T("Options Window");

static TCHAR TITLE_JSON[] = _T("JSON");
static TCHAR CLASS_JSON[] = _T("JSON Window");

int createOptionsWindow() {
	hOptions = CreateWindow(CLASS_OPTIONS, TITLE_OPTIONS, WS_OVERLAPPEDWINDOW, 0, 0, 610, 340, NULL, NULL, hInst, NULL);
	if (!hOptions) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to CreateWindow failed for hOptions!"),
			_T("Message"),
			NULL);
		return 1;
	}
	ShowWindow(hOptions, SW_SHOW);
	return 0;
}

int createJsonWindow() {
	hJSON = CreateWindow(CLASS_JSON, TITLE_JSON, WS_OVERLAPPEDWINDOW, 0, 0, 420, 680, NULL, NULL, hInst, NULL);
	if (!hJSON) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to CreateWindow failed for hJson!"),
			_T("Message"),
			NULL);
		return 1;
	}
	ShowWindow(hJSON, SW_SHOW);
	return 0;
}





//Menu options
const int QUIT = 1;
const int RESET = 2;
const int TOGGLE_JULIA = 3;
const int VIEW_GUESSED_PIXELS = 9;
const int WINDOW_OPTIONS = 10;
const int CHANGE_TRANSFORMATION = 14;
const int SAVE_IMAGE = 16;
const int SAVE_PARAMETERS = 17;
const int LOAD_PARAMETERS = 18;
const int CANCEL_RENDER = 19;
const int WINDOW_JSON = 20;
const int SAVE_BOTH = 21;

void AddMenus(HWND hwnd) {
	HMENU hMenubar = CreateMenu();
	HMENU hMenuOther = CreateMenu();
	HMENU hMenuFile = CreateMenu();

	//hMenuOther, item type, message (number), button text
	AppendMenuA(hMenuFile, MF_STRING, SAVE_IMAGE, "Save image");
	AppendMenuA(hMenuFile, MF_STRING, SAVE_PARAMETERS, "&Save parameters");
	AppendMenuA(hMenuFile, MF_STRING, SAVE_BOTH, "&Save both");
	AppendMenuA(hMenuFile, MF_STRING, LOAD_PARAMETERS, "&Load parameters");

	AppendMenuA(hMenuOther, MF_STRING, CANCEL_RENDER, "&Cancel Render");
	AppendMenuA(hMenuOther, MF_STRING, CHANGE_TRANSFORMATION, "Change &transformation");
	AppendMenuA(hMenuOther, MF_STRING, VIEW_GUESSED_PIXELS, "&View guessed pixels");
	AppendMenuA(hMenuOther, MF_SEPARATOR, 0, NULL);
	AppendMenuA(hMenuOther, MF_STRING, PROCEDURE_BURNING_SHIP, "&Burning Ship");
	AppendMenuA(hMenuOther, MF_STRING, PROCEDURE_M4, "Mandelbrot power 4&");
	AppendMenuA(hMenuOther, MF_STRING, PROCEDURE_M5, "Mandelbrot power 5&");
	AppendMenuA(hMenuOther, MF_STRING, PROCEDURE_TRIPLE_MATCHMAKER, "Triple Matchmaker&");
	AppendMenuA(hMenuOther, MF_STRING, PROCEDURE_TEST_CONTROL, "Test formula 2 (control)&");
	AppendMenuA(hMenuOther, MF_STRING, PROCEDURE_HIGH_POWER, "High Power Mandelbrot&");
	AppendMenuA(hMenuOther, MF_SEPARATOR, 0, NULL);
	AppendMenuA(hMenuOther, MF_STRING, QUIT, "&Quit");

	AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuFile, "&File");
	AppendMenuA(hMenubar, MF_STRING, WINDOW_OPTIONS, "&Options");
	AppendMenuA(hMenubar, MF_STRING, WINDOW_JSON, "&JSON");
	AppendMenuA(hMenubar, MF_STRING, RESET, "&Reset");
	AppendMenuA(hMenubar, MF_STRING, TOGGLE_JULIA, "&Toggle Julia");
	AppendMenuA(hMenubar, MF_STRING, PROCEDURE_M2, "&Mandelbrot");
	AppendMenuA(hMenubar, MF_STRING, PROCEDURE_M3, "Mandelbrot power 3");
	AppendMenuA(hMenubar, MF_STRING, PROCEDURE_CHECKERS, "&Checkers");
	AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuOther, "Mor&e");

	SetMenu(hwnd, hMenubar);
}







//constants for Triple Matchmaker:
const double sqrt3 = sqrt(3);
const double a = 2.2;
const double b = 1.4;
const double d = 1.1;


template <int formula_identifier>
double_c escapeTimeFormula(double_c z, double_c c) { return 0; };  //default implementation

template <>
double_c escapeTimeFormula<PROCEDURE_M3>(double_c z, double_c c) {
	return pow(z, 3) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_M4>(double_c z, double_c c) {
	return pow(z, 4) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_M5>(double_c z, double_c c) {
	return pow(z, 5) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_BURNING_SHIP>(double_c z, double_c c) {
	return pow((abs(real(z)) + abs(imag(z))*I), 2) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_TRIPLE_MATCHMAKER>(double_c z, double_c c) {
	return (z + a / sqrt3) / (b*(pow(z, 3) - sqrt3 * a*pow(z, 2) + c * z + a * c / sqrt3)) + d;
}

template <>
double_c escapeTimeFormula<PROCEDURE_HIGH_POWER>(double_c z, double_c c) {
	return pow(z, 33554432) + c;
}




bool readParameters(FractalParameters& S, string fileName) {
	bool success = false;
	cout << "Reading parameters" << endl;
	{
		ifstream infile;
		infile.open(fileName);
		if (infile.is_open()) {
			stringstream strStream;
			strStream << infile.rdbuf(); //now the JSON-string can be accessed by strStream.str()
			if (!S.fromJson(strStream.str())) {
				infile.close();
				MessageBox(NULL,
					_T("The parameter file doesn't have the expected format. If this is a binary file (from program version 4 and earlier), try to open and resave it with ExploreFractals 5."),
					_T("Error"),
					NULL);
				success = false;
			}
			//else: fromJson already parsed the json and changed the parameters in the program
			success = true;
		}
		else {
			cout << "opening infile failed" << endl;
			success = false;
		}
		infile.close();
	}
	return success;
}

bool writeParameters(FractalParameters& S, string fileName) {
	bool succes = true;
	cout << "Writing parameters" << endl;
	ofstream outfile(fileName);
	if (outfile.is_open()) {
		outfile << S.toJson(); //converts parameter struct to JSON
		outfile.close();
	}
	else {
		cout << "opening outfile failed" << endl;
		succes = false;
	}
	return succes;
}

FractalParameters defaultParameters;


/*
inline UINT rgbColorAverage(UINT u1, UINT u2, double ratio) {
	assert(ratio >= 0);
	assert(ratio <= 1);
	return RGB(
		(unsigned char)((GetRValue(u1))*(1 - ratio) + (GetRValue(u2))*ratio),
		(unsigned char)((GetGValue(u1))*(1 - ratio) + (GetGValue(u2))*ratio),
		(unsigned char)((GetBValue(u1))*(1 - ratio) + (GetBValue(u2))*ratio)
	);
}
*/

/*
inline UINT rgbColorAverage(vector<UINT>& colors) {
	UINT sumR, sumG, sumB;
	int size = colors.size();

	for (UINT color : colors) {
		sumR += GetRValue(color);
		sumG += GetGValue(color);
		sumB += GetBValue(color);
	}
	return RGB(
		(unsigned char)(sumR / size),
		(unsigned char)(sumG / size),
		(unsigned char)(sumB / size)
	);
}
*/

const bool CALCULATED = false;
const bool GUESSED = true;


FractalCanvas canvas;

double_c post_transformation(double_c c) {
	switch (canvas.S.post_transformation_type) {
		case 0:
			return c;
		case 1: {
			double_c z = 0;
			for (int i = 0; i < 5; i++) {
				z = z * z + c;
			}
			return z;
		}
		case 2:
			return cos(c);
		case 3:
			return c + 2. + 2.*I;
		case 4:
			return sqrt(c);
		case 5:
			return sqrt(sqrt(c));
		case 6:
			return log(c);
		case 7:
			return pow(c, canvas.S.partialInflectionPower) + canvas.S.partialInflectionCoord;
	}
	return 0;
}

double_c pre_transformation(double_c c) {
	//mostly copied from post_transformation, could be better
	switch(canvas.S.pre_transformation_type) {
		case 0:
			return c;
		case 1: {
			double_c z = 0;
			for (int i = 0; i < 5; i++) {
				z = z * z + c;
			}
			return z;
		}
		case 2:
			return cos(c);
		case 3:
			return c + 2. + 2.*I;
		case 4:
			return sqrt(c);
		case 5:
			return sqrt(sqrt(c));
		case 6:
			return log(c);
		case 7:
			return pow(c, canvas.S.partialInflectionPower) + canvas.S.partialInflectionCoord;
	}
	return 0;
}


/*
This class "Render"is literally a datastructure, but I think of it as a task. It models a task performed on a FractalCanvas.

Render holds a copy of the FractalCanvas that created it to have access to the information stored in the FractalCanvas. Most importantly access to the pointers is required to change the iteration- and pixeldata ...which means care is required so that a Render doesn't use the pointers of the FractalCanvas during a resize etc.

The idea behind the template is that the compiler will optimize everything away that is not used, given specific values for the parameters. For example, in isSameHorizontalLine it says "if (guessing)". If guessing is false, it says "if (false)", so the whole function definition can be recuced to "return false", which will lead to optimizations wherever the function is used as well, because the return value is known at compile time. Also for example in calcPoint there are many ifs 
*/
template <int formula_identifier, bool guessing, bool use_avx, bool julia>
class Render {
public:
	//a copy of some widely used members of canvas.S
	int width;
	int height;
	int screenWidth;
	int screenHeight;
	int oversampling;
	//other 
	FractalCanvas& canvas2;
	int renderID;

	int usedThreads; //counts the total number of threads created
	int threadCount;
	Formula formula;
	double_c juliaSeed;
	int maxIters;
	double escapeRadius;
	vector<double_c> inflectionCoords;
	int inflectionCount;
	int inflectionPower;

	chrono::time_point<chrono::steady_clock> startTime;
	chrono::time_point<chrono::steady_clock> endTime;
	bool isFinished;
	int guessedPixelCount;
	int calculatedPixelCount;
	int pixelGroupings;
	long long computedIterations;

	Render(FractalCanvas& canvasContext, int renderID)
	: canvas2(canvasContext)
	{
		if(debug) cout << "creating render " << renderID << endl;
		this->renderID = renderID;
		
		threadCount = 0;
		usedThreads = 0;
		pixelGroupings = 0;
		guessedPixelCount = 0;
		calculatedPixelCount = 0;
		computedIterations = 0;
		isFinished = false;

		formula = canvas2.S.get_formula();
		juliaSeed = canvas2.S.juliaSeed;
		width = canvas2.S.get_width();
		height = canvas2.S.get_height();
		screenWidth = canvas2.S.get_screenWidth();
		screenHeight = canvas2.S.get_screenHeight();
		oversampling = canvas2.S.get_oversampling();
		maxIters = canvas2.S.get_maxIters();
		escapeRadius = canvas2.S.get_formula().escapeRadius;
		inflectionCoords = canvas2.S.get_inflectionCoords(); //This makes a copy of the whole vector.
		inflectionCount = canvas2.S.get_inflectionCount();
		inflectionPower = canvas2.S.get_formula().inflectionPower;
	}

	~Render(void) {
		if(debug) cout << "Render " << renderID << " is being deleted" << endl;
	}

	void addToThreadcount(int n) {
		lock_guard<mutex> guard(threadCountChange);
		threadCount += n;
		usedThreads++;
	}

	double_c inflections_m2(double_c c) {
		//More efficient inflection application just for Mandelbrot power 2

		double_c thisInflectionCoord;
		double cr;
		double ci;
		double zr;
		double zi;
		double zrsqr;
		double zisqr;
		zr = real(c);
		zi = imag(c);
		for (int i = inflectionCount - 1; i >= 0; i--) {
			thisInflectionCoord = inflectionCoords[i];
			cr = real(thisInflectionCoord);
			ci = imag(thisInflectionCoord);

			zrsqr = zr * zr;
			zisqr = zi * zi;
			zi = zr * zi;
			zi += zi;
			zi += ci;
			zr = zrsqr - zisqr + cr;
		}
		return zr + zi * I;
	}

	double_c inflections(double_c c) {
		for (int i = inflectionCount - 1; i >= 0; i--) {
			c = pow(c, inflectionPower) + inflectionCoords[i];
		}
		return c;
	}

	inline void setPixel(int i, int j, int iterationCount, bool GUESSED)
	{
		assert(i < canvas.S.get_width() && j < canvas.S.get_height());

		bool isInMinibrot = iterationCount == canvas.S.get_maxIters();
		int itersIndex = canvas.itersIndex_of_itersXY(i, j);

		canvas.iters[itersIndex] = {
			iterationCount,
			GUESSED,
			isInMinibrot
		};
	}

	int calcPoint(int i, int j) {
		int iterationCount = 0;

		if (formula_identifier == PROCEDURE_M2) {
			double_c c = post_transformation(inflections_m2(pre_transformation(canvas2.S.map(i, j))));

			double cr;
			double ci;
			double zr;
			double zi;
			double zrsqr;
			double zisqr;
			if (julia) {
				cr = real(juliaSeed);
				ci = imag(juliaSeed);
				zr = real(c);
				zi = imag(c);
				zrsqr = zr * zr;
				zisqr = zi * zi;
			}
			else {
				double zx;
				double zy;
				zx = real(c);
				zy = imag(c);
				double zx2 = zx;
				double zy2 = zy;

				zx -= 0.25; zy *= zy;
				double q = zx * zx + zy;
				if (4 * q*(q + zx) < zy) {
					setPixel(i, j, maxIters, CALCULATED);
					return maxIters;
				}
				zx = zx2;
				zy = zy2;
				zx++;
				if (zx * zx + zy * zy < 0.0625) {
					setPixel(i, j, maxIters, CALCULATED);
					return maxIters;
				}

				cr = zx2;
				ci = zy2;
				zr = 0;
				zi = 0;
				zrsqr = 0;
				zisqr = 0;
			}
			while (zrsqr + zisqr <= 4.0 && iterationCount < maxIters) {
				zi = zr * zi;
				zi += zi;
				zi += ci;
				zr = zrsqr - zisqr + cr;
				zrsqr = zr * zr;
				zisqr = zi * zi;
				iterationCount++;
			}
		}
		if (
			formula_identifier == PROCEDURE_M3
			|| formula_identifier == PROCEDURE_M4
			|| formula_identifier == PROCEDURE_M5
			|| formula_identifier == PROCEDURE_HIGH_POWER
			|| formula_identifier == PROCEDURE_BURNING_SHIP
		) {
			//use a general escape time fractal loop
			double_c c;
			double_c z;
			if (julia) {
				c = juliaSeed;
				z = post_transformation(inflections(pre_transformation(canvas2.S.map(i, j))));
			}
			else {
				c = post_transformation(inflections(pre_transformation(canvas2.S.map(i, j))));
				z = 0;
			}
			while (real(z)*real(z) + imag(z)*imag(z) < escapeRadius && iterationCount < maxIters) {
				z = escapeTimeFormula<formula_identifier>(z, c);
				iterationCount++;
			}
		}
		if (formula_identifier == PROCEDURE_CHECKERS) {
			double_c c = post_transformation(inflections_m2(pre_transformation(canvas2.S.map(i, j))));
			//create checkerboard tiles
			double resolution = 3.1415926535897932384626433832795; //tile size, pi goes well with the natural log transformation
			bool vertical = (int)(imag(c) / resolution) % 2 == 0;
			bool horizontal = (int)(real(c) / resolution) % 2 == 0;
			bool result = horizontal ^ vertical;
			if ((imag(c) < 0) ^ (real(c) < 0)) result = !result;

			//create julia detail simulation
			double transRe = real(c) - (int)(real(c) / resolution) * resolution; //coords translated to the first quadrant's first tile for simplicity
			double transIm = imag(c) - (int)(imag(c) / resolution) * resolution;
			if (transRe < 0) transRe += resolution;
			if (transIm < 0) transIm += resolution;

			bool underInc = transIm < transRe; //coordinate is under the increasing line from (0,0) to (resolution, resolution)
			bool underDec = transIm < resolution - transRe; //under decreasing line from (0, resolution) to (resolution, 0)

			double_c ref;
			if (underInc) {
				if (underDec) ref = 0.5*resolution;
				else ref = resolution + 0.5*resolution*I;
			}
			else {
				if (underDec) ref = 0.5*resolution*I;
				else ref = 0.5*resolution + resolution * I;
			}

			double transRefDist = sqrt(pow(transRe - real(ref), 2) + pow(transIm - imag(ref), 2));
			double distLog = log(transRefDist);
			int resFactors = (int)(distLog / resolution - 0.5);

			if (resFactors % 2 != 0) result = !result;

			if (result) iterationCount = 501; //these choices are arbitrary and just to distinguish tiles
			else iterationCount = 54;

		}
		if (formula_identifier == PROCEDURE_TEST_CONTROL) {
			int thisMaxIters = 100;
			double_c c = post_transformation(inflections(pre_transformation(canvas2.S.map(i, j))));
			double cr;
			double ci;
			double zr;
			double zi;
			double zrsqr;
			double zisqr;
			cr = real(c);
			ci = imag(c);
			zr = 0;
			zi = 0;
			zrsqr = 0;
			zisqr = 0;
			while (zrsqr + zisqr <= 4.0 && iterationCount < thisMaxIters) {
				zi = zr * zi;
				zi += zi;
				zi += ci;
				zr = zrsqr - zisqr + cr;
				zrsqr = zr * zr;
				zisqr = zi * zi;
				iterationCount++;
			}
		}
		if (formula_identifier == PROCEDURE_TRIPLE_MATCHMAKER) {
			double_c c;
			double_c z;
			double summ = 0;
			if (julia) {
				c = juliaSeed;
				z = post_transformation(inflections(pre_transformation(canvas2.S.map(i, j))));
			}
			else {
				c = post_transformation(inflections(pre_transformation(canvas2.S.map(i, j))));
				z = 0;
			}
			for (int k = 2; k < maxIters; k++) {
				z = escapeTimeFormula<PROCEDURE_TRIPLE_MATCHMAKER>(z, c);
				summ += abs(z);
			}
			iterationCount = (int)(summ);
		}

		setPixel(i, j, iterationCount, CALCULATED);

		return iterationCount;
	}

	#define setPixelAndThisIter(i, y, iterationCount, mode) \
	setPixel(i, y, iterationCount, mode); \
	if (isSame) \
		if (iterationCount != thisIter) { \
			if (thisIter == -1) \
				thisIter = iterationCount; \
			else \
				isSame = false; \
		}

	bool calcPixelVector(vector<int>& pixelVect, int fromPixel, int toPixel) {
		if (renderID != canvas2.lastRenderID) { //gaat fout want lastRenderId wordt alleen geupdate in het globale canvas. canvas2 is een kopie
			if(debug) cout << "Render " << renderID << " cancelled; terminating thread" << endl;
			return true; //Returning true but it doesn't really matter what the result is when the render is cancelled.
		}

		int pixelCount = toPixel - fromPixel;
		bool isSame = true;
		int thisIter = -1;

		if (!use_avx || formula.identifier != PROCEDURE_M2 || pixelCount < 4) {
			int thisI = pixelVect[fromPixel + fromPixel];
			int thisJ = pixelVect[fromPixel + fromPixel + 1];
			int thisIter = calcPoint(thisI, thisJ);

			for (int k = fromPixel + 1; k < toPixel; k++) {
				assert(thisI < width); assert(thisJ < height);

				thisI = pixelVect[k + k];
				thisJ = pixelVect[k + k + 1];
				if (calcPoint(thisI, thisJ) != thisIter)
					isSame = false;
			}
		}
		else { //AVX only for Mandelbrot power 2
			//AVX is used. Length 4 arrays and vectors are constructed to iterate 4 pixels at once. That means 4 i-values, 4 j-values, 4 c-values etc.
			int i[4] = {
				pixelVect[fromPixel + fromPixel]
				,pixelVect[fromPixel + fromPixel + 2]
				,pixelVect[fromPixel + fromPixel + 4]
				,pixelVect[fromPixel + fromPixel + 6]
			};
			int j[4]{
				pixelVect[fromPixel + fromPixel + 1]
				,pixelVect[fromPixel + fromPixel + 3]
				,pixelVect[fromPixel + fromPixel + 5]
				,pixelVect[fromPixel + fromPixel + 7]
			};
			double_c c[4] = {
				post_transformation(inflections_m2(pre_transformation(canvas2.S.map(i[0], j[0]))))
				,post_transformation(inflections_m2(pre_transformation(canvas2.S.map(i[1], j[1]))))
				,post_transformation(inflections_m2(pre_transformation(canvas2.S.map(i[2], j[2]))))
				,post_transformation(inflections_m2(pre_transformation(canvas2.S.map(i[3], j[3]))))
			};
			int iterationCounts[4] = { 0, 0, 0, 0 };
			int nextPixel = fromPixel + 4;

			__m256d cr, ci, zr, zi, zrsqr, zisqr; //256-bit vectors of 4 doubles each
			//access to doubles individually through array (p for pointer) interpretation
			double* crp = (double*)&cr;
			double* cip = (double*)&ci;
			double* zrp = (double*)&zr;
			double* zip = (double*)&zi;
			double* zrsqrp = (double*)&zrsqr;
			double* zisqrp = (double*)&zisqr;
			//double versions of the vector variables, used to iterate pixels individually:
			double crd, cid, zrd, zid, zrsqrd, zisqrd;

			__m256d bailout_v = _mm256_set1_pd(4.0);
			__m256d zisqr_plus_zrsqr_v = _mm256_setzero_pd();
			__m256d pixel_has_escaped_v;
			bool* pixel_has_escaped_p = (bool*)&pixel_has_escaped_v;
			__m256d all_true = _mm256_cmp_pd(_mm256_setzero_pd(), _mm256_setzero_pd(), _CMP_EQ_OS);

			double julia_r;
			double julia_i;

			//note that the order here is different from the arrays i, j and c. Apparently _mm256_set_pd fills the vector in opposite order?
			if (julia) {
				julia_r = real(juliaSeed);
				julia_i = imag(juliaSeed);

				cr = _mm256_set1_pd(julia_r);
				ci = _mm256_set1_pd(julia_i);
				zr = _mm256_set_pd(real(c[3]), real(c[2]), real(c[1]), real(c[0]));
				zi = _mm256_set_pd(imag(c[3]), imag(c[2]), imag(c[1]), imag(c[0]));
				zrsqr = _mm256_mul_pd(zr, zr);
				zisqr = _mm256_mul_pd(zi, zi);
			}
			else {
				cr = _mm256_set_pd(real(c[3]), real(c[2]), real(c[1]), real(c[0]));
				ci = _mm256_set_pd(imag(c[3]), imag(c[2]), imag(c[1]), imag(c[0]));
				zr = _mm256_setzero_pd();
				zi = _mm256_setzero_pd();
				zrsqr = _mm256_setzero_pd();
				zisqr = _mm256_setzero_pd();
			}

			bool use_avx_iteration = true;

			while (use_avx_iteration) {
				zi = _mm256_mul_pd(zr, zi);
				zi = _mm256_add_pd(zi, zi);
				zi = _mm256_add_pd(zi, ci);
				zr = _mm256_add_pd(_mm256_sub_pd(zrsqr, zisqr), cr);
				zrsqr = _mm256_mul_pd(zr, zr);
				zisqr = _mm256_mul_pd(zi, zi);
				zisqr_plus_zrsqr_v = _mm256_add_pd(zrsqr, zisqr);

				for (int m = 0; m < 4; m++) {
					iterationCounts[m]++;
				}

				//pixel_has_escaped_v = _mm256_cmp_pd(bailout_v, zisqr_plus_zrsqr_v, _CMP_LE_OQ);
				pixel_has_escaped_v = _mm256_xor_pd(
					_mm256_cmp_pd(zisqr_plus_zrsqr_v, bailout_v, _CMP_LE_OQ)
					, all_true //xor with this is required to interpret comparison with NaN as true. When there's a NaN I consider that pixel escaped.
				);

				if (
					pixel_has_escaped_p[0]
					|| pixel_has_escaped_p[8] //Indices are multiplied by 8 because the bool values in pixel_has_escaped_p are stored as doubles which are 8 bytes long:
					|| pixel_has_escaped_p[16]
					|| pixel_has_escaped_p[24]
					|| iterationCounts[0] >= maxIters
					|| iterationCounts[1] >= maxIters
					|| iterationCounts[2] >= maxIters
					|| iterationCounts[3] >= maxIters
					) {
					//when here, one of the pixels has escaped or exceeded the max number of iterations

					for (int k = 0; k < 4; k++) { //for each pixel in the AVX vector
						if (pixel_has_escaped_p[8 * k] || iterationCounts[k] >= maxIters) { //this pixel is done
							if (nextPixel < toPixel) {
								//There is work left to do. The finished pixel needs to be replaced by a new one.

								setPixelAndThisIter(i[k], j[k], iterationCounts[k], CALCULATED); //it is done so we set it
								iterationCounts[k] = -1; //to mark pixel k in the vectors as done/invalid

								bool pixelIsValid = false;

								while (!pixelIsValid && nextPixel < toPixel) {
									//take new pixel:
									i[k] = pixelVect[nextPixel + nextPixel];
									j[k] = pixelVect[nextPixel + nextPixel + 1];
									c[k] = post_transformation(inflections_m2(pre_transformation(canvas2.S.map(i[k], j[k]))));
									nextPixel++;

									//verify that this pixel is valid:
									if (julia) {
										pixelIsValid = true;

										iterationCounts[k] = 0;
										crp[k] = julia_r;
										cip[k] = julia_i;
										zrp[k] = real(c[k]);
										zip[k] = imag(c[k]);
										zrsqrp[k] = zrp[k] * zrp[k];
										zisqrp[k] = zip[k] * zip[k];
									}
									else {
										double zx = real(c[k]);
										double zy = imag(c[k]);
										double zx_backup = zx;
										double zy_backup = zy;

										zx++;
										if (zx * zx + zy * zy < 0.0625) {
											setPixelAndThisIter(i[k], j[k], maxIters, CALCULATED);
										}
										else {
											zx = zx_backup;
											zy = zy_backup;
											zx -= 0.25;
											zy *= zy;
											double q = zx * zx + zy;
											if (4 * q*(q + zx) < zy) {
												setPixelAndThisIter(i[k], j[k], maxIters, CALCULATED);
											}
											else {
												pixelIsValid = true;

												iterationCounts[k] = 0;
												crp[k] = zx_backup;
												cip[k] = zy_backup;
												zrp[k] = 0;
												zip[k] = 0;
												zrsqrp[k] = 0;
												zisqrp[k] = 0;
											}
										}

									}
								}
								if (!pixelIsValid) {
									//searching for a valid new pixel resulted in reaching the end of the vector. There are not enough pixels to fill the AVX vector.
									use_avx_iteration = false;
								}
							}
							else {
								use_avx_iteration = false;
							}
						}
					}
				}
			}

			//Finish iterating the remaining 3 or fewer pixels without AVX:
			for (int k = 0; k < 4; k++) {
				int iterationCount = iterationCounts[k];
				if (iterationCount != -1) {
					crd = crp[k];
					cid = cip[k];
					zrd = zrp[k];
					zid = zip[k];
					zrsqrd = zrsqrp[k];
					zisqrd = zisqrp[k];
					while (zrsqrd + zisqrd <= 4.0 && iterationCount < maxIters) {
						zid = zrd * zid;
						zid += zid;
						zid += cid;
						zrd = zrsqrd - zisqrd + crd;
						zrsqrd = zrd * zrd;
						zisqrd = zid * zid;
						iterationCount++;
					}
					setPixelAndThisIter(i[k], j[k], iterationCount, CALCULATED);
				}
			}
		}
		calculatedPixelCount += pixelCount;
		return isSame && formula.isGuessable;
	}

	bool isSameHorizontalLine(int iFrom, int iTo, int height) {
		assert(iTo >= iFrom);
		if (guessing) {
			bool same = true;
			int thisIter = canvas2.getIterationcount(iFrom, height);
			for (int i = iFrom + 1; i < iTo; i++) {
				if (thisIter != canvas2.getIterationcount(i, height)) {
					same = false;
					break;
				}
			}
			return same;
		}
		else {
			return false;
		}
	}

	bool isSameVerticalLine(int jFrom, int jTo, int width) {
		assert(jTo >= jFrom);
		if (guessing) {
			bool same = true;
			int thisIter = canvas2.getIterationcount(width, jFrom);
			for (int j = jFrom + 1; j < jTo; j++) {
				if (thisIter != canvas2.getIterationcount(width, j)) {
					same = false;
					break;
				}
			}
			return same;
		}
		else {
			return false;
		}
	}

	bool calcHorizontalLine(int iFrom, int iTo, int height) {
		assert(iTo >= iFrom); //faalde
		int size = iTo - iFrom;
		vector<int> hLine(size * 2);
		int thisIndex = 0;
		for (int i = iFrom; i < iTo; i++) {
			hLine[thisIndex++] = i;
			hLine[thisIndex++] = height;
		}
		return calcPixelVector(hLine, 0, thisIndex / 2);
	}

	bool calcVerticalLine(int jFrom, int jTo, int width) {
		assert(jTo >= jFrom);
		int size = jTo - jFrom;
		vector<int> vLine(size * 2);
		int thisIndex = 0;
		for (int j = jFrom; j < jTo; j++) {
			vLine[thisIndex++] = width;
			vLine[thisIndex++] = j;
		}
		return calcPixelVector(vLine, 0, thisIndex / 2);
	}

	/*
		Calculate the tile from imin to imax (x-coordinates) and from jmin to jmax (y-coordinates). These boundary values belong to the tile. The assumption is that the boundary has already been computed. sameBottom means that the bottom row of points in the tile all have the same iterationcount. iterBottom is that iterationcount. The same goes for sameLeft, sameRight etc.

		The bool inNewThread is only to keep track of the current number of threads.

		bitmap_render_responsibility means that this tile should compute and set the colors of its pixels to the bitmap after it is finished. The function can pass the responsiblity on to subtiles through its recursive calls. It's not entirely clear how long the passing should continue for the best performance. When oversampling is used, it's important not to continue for too long because a tile can be smaller than a pixel. (Each pixel is a raster of oversampling×oversampling calculated points.) There would be double work done if every tile that has overlap with some pixel calculates its color. See the explanation of stop_creating_threads.
	*/
	void renderSilverRect(bool inNewThread, bool bitmap_render_responsibility, int imin, int imax, int jmin, int jmax, bool sameTop, int iterTop, bool sameBottom, int iterBottom, bool sameLeft, int iterLeft, bool sameRight, int iterRight)
	{
		if (renderID != canvas2.lastRenderID) {
			if(debug) cout << "Render " << renderID << " cancelled; terminating thread" << endl;
			return;
		}

		int size = (imax - imin - 1)*(jmax - jmin - 1); //the size of the part that still has to be calculated

		/*
			This is to prevent creating new threads when the tile becomes so small that it's only a few pixels wide or high. (That number is NEW_TILE_THREAD_MIN_PIXELS.)
			
			The problem with threads at that scale has to do with oversampling. Oversampling means there are multiple calculated points per pixel. If two tiles overlap with one pixel, and both tiles are being calculated by different threads, it can happen that a pixel color value is changed by 2 threads at the same time, leading to an incorrect result. By not creating new threads, the subdivision into smaller tiles can continue safely.

			 Increasing NEW_TILE_THREAD_MIN_PIXELS can improve performance by allowing for more optimal tile subdivision, but it also causes color values to be calculated in larger groups together making bitmap updates less frequent, and not allowing new threads too early means less usage of all CPU cores.

			Note also that this is only necessary to show progress in the program. Without visible progress, calculating the color values could be postponed until after all the iteration counts have been calculated, but that's not user-friendly.
		*/
		int stop_creating_threads = (imax - imin) / oversampling < 8 || (jmax - jmin) / oversampling < 8;

		bool pass_on_bitmap_render_responsibility = bitmap_render_responsibility && !stop_creating_threads;

		if (guessing) {
			if (sameRight && sameLeft && sameTop && sameBottom && iterRight == iterTop && iterTop == iterLeft && iterLeft == iterBottom && iterRight != 1 && iterRight != 0) {
				//The complete boundary of the tile has the same iterationCount. Fill with that same value:
				for (int i = imin + 1; i < imax; i++) {
					for (int j = jmin + 1; j < jmax; j++) {
						setPixel(i, j, iterLeft, GUESSED);
					}
				}
				guessedPixelCount += (imax - imin - 1)*(jmax - jmin - 1);
				pass_on_bitmap_render_responsibility = false;
				goto returnLabel;
			}
		}

		if (size < MAXIMUM_TILE_SIZE) {
			//The tile is now very small. Stop the recursion and iterate all pixels.
			vector<int> toIterate(size * 2);
			int thisIndex = 0;
			for (int i = imin + 1; i < imax; i++) {
				for (int j = jmin + 1; j < jmax; j++) {
					toIterate[thisIndex++] = i;
					toIterate[thisIndex++] = j;
				}
			}
			calcPixelVector(toIterate, 0, thisIndex / 2);
			pass_on_bitmap_render_responsibility = false;
			goto returnLabel;
		}

		//The tile gets split up:
		if (imax - imin < jmax - jmin) {
			//The tile is taller than it's wide. Split the tile with a horizontal line. The y-coordinate is:
			int j = jmin + (jmax - jmin) / 2;
			if (!stop_creating_threads) {
				j = j - (j%oversampling); //round to whole pixels
			}

			//compute new line
			bool sameNewLine = calcHorizontalLine(imin + 1, imax, j);
			int iterNewLine = canvas2.getIterationcount(imin + 1, j);

			//check right and left for equality
			bool sameRightTop = true;
			bool sameLeftTop = true;
			bool sameLeftBottom = true;
			bool sameRightBottom = true;
			int iterRightTop = canvas2.getIterationcount(imax, jmin);
			int iterRightBottom = canvas2.getIterationcount(imax, j);
			int iterLeftTop = canvas2.getIterationcount(imin, jmin);
			int iterLeftBottom = canvas2.getIterationcount(imin, j);

			if (!sameRight) {
				sameRightTop = isSameVerticalLine(jmin, j, imax);
				sameRightBottom = isSameVerticalLine(j, jmax, imax);
			}
			if (!sameLeft) {
				sameLeftTop = isSameVerticalLine(jmin, j, imin);
				sameLeftBottom = isSameVerticalLine(j, jmax, imin);
			}

			if (renderID == canvas.lastRenderID) {
				if (threadCount < NUMBER_OF_THREADS && !stop_creating_threads) {
					addToThreadcount(1);
					thread t(&Render::renderSilverRect, this, true, pass_on_bitmap_render_responsibility, imin, imax, jmin, j, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
					renderSilverRect(inNewThread, pass_on_bitmap_render_responsibility, imin, imax, j, jmax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
					t.join();
					inNewThread = false; //t is now the new thread
				}
				else {
					if (height - jmax < 0.5*height) {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, jmin, j, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, j, jmax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
					}
					else {
						//same in different order. The intention is that the center of the screen receives priority.
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, j, jmax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, jmin, j, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
					}
				}
			}
		}
		else {
			//The tile is wider than it's tall. Split the tile with a vertical line. The x-coordinate is:
			int i = imin + (imax - imin) / 2;
			if (!stop_creating_threads) {
				i = i - (i%oversampling); //round to whole pixels
			}

			//Compute new line
			bool sameNewLine = calcVerticalLine(jmin + 1, jmax, i);
			int iterNewLine = canvas2.getIterationcount(i, jmin + 1);

			//Check Top and Bottom for equality
			bool sameRightTop = true;
			bool sameLeftTop = true;
			bool sameLeftBottom = true;
			bool sameRightBottom = true;
			int iterRightTop = canvas2.getIterationcount(i, jmin);
			int iterLeftTop = canvas2.getIterationcount(imin, jmin);
			int iterRightBottom = canvas2.getIterationcount(i, jmax);
			int iterLeftBottom = canvas2.getIterationcount(imin, jmax);

			if (!sameTop) {
				sameLeftTop = isSameHorizontalLine(imin, i, jmin);
				sameRightTop = isSameHorizontalLine(i, imax, jmin);
			}
			if (!sameBottom) {
				sameLeftBottom = isSameHorizontalLine(imin, i, jmax);
				sameRightBottom = isSameHorizontalLine(i, imax, jmax);
			}
			if (renderID == canvas.lastRenderID) {
				if (threadCount < NUMBER_OF_THREADS && !stop_creating_threads) {
					addToThreadcount(1);
					thread t(&Render::renderSilverRect, this, true, pass_on_bitmap_render_responsibility, imin, i, jmin, jmax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
					renderSilverRect(inNewThread, pass_on_bitmap_render_responsibility, i, imax, jmin, jmax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
					t.join();
					inNewThread = false; //t is now the new thread
				}
				else {
					if (width - imax < 0.5*width) {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, i, jmin, jmax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, i, imax, jmin, jmax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
					}
					else {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, i, imax, jmin, jmax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, i, jmin, jmax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
					}
				}
			}
		}

	returnLabel:
		if (bitmap_render_responsibility && !pass_on_bitmap_render_responsibility) {
			
			int xborderCorrection = (imax == width - 1 ? 0 : 1);
			int yborderCorrection = (jmax == height - 1 ? 0 : 1);

			int xfrom = imin / oversampling;
			int xto = (imax - xborderCorrection) / oversampling + 1;
			int yfrom = jmin / oversampling;  
			int yto = (jmax - yborderCorrection) / oversampling + 1;
			assert(xfrom <= xto);
			assert(yfrom <= yto);
			
			//if(debug) cout << "taking bitmap render responsibility for (xfrom, xto, yfrom, yto): (" << xfrom << ", " << xto << ", " << yfrom << ", " << yto << endl;
			canvas.renderBitmapRect(false, xfrom, xto, yfrom, yto, canvas.lastBitmapRenderID);
		}
		
		pixelGroupings += 2;
		if (inNewThread) {
			addToThreadcount(-1);
		}
	}

	void renderSilverFull() {
		//This calculates a raster of pixels first and then launches threads for each tile in the raster.
		//"Silver" refers to the Mariani-Silver algorithm.
		//Use the option "View guessed pixels" in the program while in a sparse area to see how it works.
		int imin = 0;
		int imax = width - 1;
		int jmin = 0;
		int jmax = height - 1;

		int tiles = (int)(sqrt(NUMBER_OF_THREADS)); //the number of tiles in both horizontal and vertical direction, so in total there are (tiles * tiles)

		int widthStep = width / tiles;
		int heightStep = height / tiles;

		vector<bool> isSameList(2 * tiles*(tiles + 1));
		vector<int> heights(tiles + 1);
		vector<int> widths(tiles + 1);

		for (int k = 0; k < tiles; k++) {
			heights[k] = k * heightStep;
			widths[k] = k * widthStep;
		}
		heights[tiles] = jmax;
		widths[tiles] = imax;

		//This vector is all pixels in the raster in (x-coord, y-coord) format: x-coords are at odd indices (0, 3, ....), y-coords at even indices
		vector<int> rasterCoordinates((tiles + 1)*(height + width) * 2);
		int rasterCoordinateIndex = 0;

		int thisHeight;
		int thisWidth;
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
				thisHeight = heights[lineNumV];
				thisWidth = widths[lineNumH];
				for (int k = widths[lineNumH] + 1; k < widths[lineNumH + 1]; k++) { // a '+ 1' here for initial k, because otherwise the next for-loop would calculate the same pixel again (this only saves calculating typically 4 - 16 pixels per render)
					rasterCoordinates[rasterCoordinateIndex++] = k;
					rasterCoordinates[rasterCoordinateIndex++] = thisHeight;
				}
				for (int k = heights[lineNumV]; k < heights[lineNumV + 1]; k++) {
					rasterCoordinates[rasterCoordinateIndex++] = thisWidth;
					rasterCoordinates[rasterCoordinateIndex++] = k;
				}
			}
		}
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int k = widths[lineNumH]; k < widths[lineNumH + 1]; k++) {
				rasterCoordinates[rasterCoordinateIndex++] = k;
				rasterCoordinates[rasterCoordinateIndex++] = jmax;
			}
		}
		for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
			for (int k = heights[lineNumV]; k < heights[lineNumV + 1]; k++) {
				rasterCoordinates[rasterCoordinateIndex++] = imax;
				rasterCoordinates[rasterCoordinateIndex++] = k;
			}
		}
		rasterCoordinates[rasterCoordinateIndex++] = imax;
		rasterCoordinates[rasterCoordinateIndex++] = jmax;


		//Calculate the array of raster pixels multithreaded:
		int usingThreadCount = tiles * tiles;
		int pixelCount = rasterCoordinateIndex / 2;
		int thrVectPieceSize = pixelCount / usingThreadCount;
		int createdRasterThreads = 0;
		vector<thread> threadsRaster(usingThreadCount);

		for (int k = 0; k < usingThreadCount - 1; k++) {
			threadsRaster[createdRasterThreads++] = thread(&Render::calcPixelVector, this, ref(rasterCoordinates), k * thrVectPieceSize, (k + 1)*thrVectPieceSize);
		}
		threadsRaster[createdRasterThreads++] = thread(&Render::calcPixelVector, this, ref(rasterCoordinates), (usingThreadCount - 1) * thrVectPieceSize, pixelCount);

		cout << "Calculating initial raster with " << createdRasterThreads << " threads" << endl;
		for (int k = 0; k < createdRasterThreads; k++)
			threadsRaster[k].join();


		//Check which rectangles in the raster can be guessed:
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
				isSameList[(lineNumH * tiles + lineNumV) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], heights[lineNumV]);
				isSameList[(lineNumH * tiles + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], widths[lineNumH]);
			}
		}
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			isSameList[(lineNumH * tiles + tiles) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], jmax);
		}
		for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
			isSameList[(tiles * tiles + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], imax);
		}

		//Launch threads to fully compute each rectangle
		vector<thread> threadsTiles(usingThreadCount);
		int createdTileThreads = 0;
		threadCount = usingThreadCount;

		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
				int thisImin = widths[lineNumH];
				int thisImax = widths[lineNumH + 1];
				int thisJmin = heights[lineNumV];
				int thisJmax = heights[lineNumV + 1];

				int iterTop = canvas2.getIterationcount(thisImax, thisJmin);
				int iterBottom = canvas2.getIterationcount(thisImax, thisJmax);
				int iterLeft = canvas2.getIterationcount(thisImin, thisJmax);
				int iterRight = canvas2.getIterationcount(thisImax, thisJmax);

				bool sameTop = isSameList[(lineNumH * tiles + lineNumV) * 2];
				bool sameBottom = isSameList[(lineNumH * tiles + (lineNumV + 1)) * 2];
				bool sameLeft = isSameList[(lineNumH * tiles + lineNumV) * 2 + 1];
				bool sameRight = isSameList[((lineNumH + 1) * tiles + lineNumV) * 2 + 1];

				threadsTiles[createdTileThreads++] = thread(&Render::renderSilverRect, this, true, true, thisImin, thisImax, thisJmin, thisJmax, sameTop, iterTop, sameBottom, iterBottom, sameLeft, iterLeft, sameRight, iterRight);
			}
		}

		cout << "Calculating tiles with " << createdTileThreads << " threads" << endl;
		for (int k = 0; k < createdTileThreads; k++) {
			threadsTiles[k].join();
		}
		cout << "Calculating tiles finished." << endl;
		return;
	}

	double getElapsedTime() {
		chrono::duration<double> elapsed;
		if (!isFinished) {
			auto now = chrono::high_resolution_clock::now();
			elapsed = now - startTime;
		}
		else {
			elapsed = endTime - startTime;
		}
		return elapsed.count();
	}

	void execute() {
		assert(width > 0);
		assert(height > 0);
		cout << "in execute " << renderID << endl;

		lock_guard<mutex> guard(renders);

		if (renderID != canvas.lastRenderID)
			return;

		startTime = chrono::high_resolution_clock::now();
		renderSilverFull();
		endTime = chrono::high_resolution_clock::now();
		isFinished = true;

		for (int i = 0; i < width; i++) {
			for (int j = 0; j < height; j++) {
				computedIterations += canvas2.iters[i*height + j].iterationCount;
			}
		}
	}
};

template <int formula_identifier, bool guessing, bool use_avx, bool julia>
void refreshDuringRender(Render<formula_identifier, guessing, use_avx, julia>& R, FractalCanvas& canvasContext, int renderID) {
	this_thread::sleep_for(chrono::milliseconds(70));
	if(debug) cout << "refreshDuringRender " << renderID << " awakened" << endl;
	int screenWidth = canvasContext.S.get_screenWidth();
	int screenHeight = canvasContext.S.get_screenHeight();

	while (canvasContext.lastRenderID == renderID && canvas.activeRenders != 0) {
		if(debug) cout << "refreshDuringRender " << renderID << " waiting for lock drawingBitmap" << endl;
		{
			lock_guard<mutex> guard(drawingBitmap);
			if(debug) cout << "refreshDuringRender " << renderID << " has lock drawingBitmap" << endl;
			//if (canvasContext.lastRenderID == renderID) { //to prevent drawing after the render is already finished, because a drawing is already made immediately after the render finishes
			if (canvasContext.lastRenderID == renderID) {
				HDC hdc = GetDC(hWndMain);
				DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY, screenWidth, screenHeight);
				ReleaseDC(hWndMain, hdc);
				double percentage = (double)(R.guessedPixelCount + R.calculatedPixelCount) / (R.width * R.height) * 100;
				stringstream progressString; progressString << fixed << setprecision(2) << percentage << "%";
				string elapsedString = to_string(R.getElapsedTime()) + " s";
				SendMessageA(statusBar, SB_SETTEXTA, 0, (LPARAM)(progressString.str().c_str()));
				SendMessageA(statusBar, SB_SETTEXTA, 1, (LPARAM)(elapsedString.c_str()));
				firstPaint = false;
			}
			else {
				if(debug) cout << "refreshDuringRender " << renderID << " doesn't draw because the render was cancelled" << endl;
			}
		}
		if(debug) cout << "refreshDuringRender " << renderID << " released lock drawingBitmap" << endl;
		this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if(debug) cout << "refreshDuringRender thread " << renderID << " ended" << endl;
}

template <int formula_identifier, bool guessing, bool use_avx, bool julia>
void FractalCanvas::createNewRenderTemplated(bool headless) {
	int renderID;
	{
		lock_guard<mutex> guard(renders);
		renderID = ++lastRenderID;
		activeRenders++;
		for (int i=0; i < S.get_screenWidth() * S.get_screenHeight(); i++) {
			ptPixelsMetadata[i] = 0;
		}
	}

	Render<formula_identifier, guessing, use_avx, julia> R(*this, renderID);

	int width = S.get_width();
	int height = S.get_height();
	{
		//Printing information before the render
		double_c center = S.get_center();
		double_c juliaSeed = S.juliaSeed;
		cout << "-----New Render-----" << endl;
		cout << "renderID: " << renderID << endl;
		cout << "Formula: " << S.get_formula().name << endl;
		cout << "width: " << width << endl;
		cout << "height: " << height << endl;
		cout << "center: " << real(center) << " + " << imag(center) << " * I" << endl;
		cout << "xrange: " << S.get_x_range() << endl;
		cout << "yrange: " << S.get_y_range() << endl;
		cout << "zoom: " << S.getZoomLevel() << endl;
		cout << "Julia: ";
		if (S.get_julia()) cout << "Yes, with seed " << real(juliaSeed) << " + " << imag(juliaSeed) << " * I" << endl;
		else cout << "no" << endl;
	}

	if (headless) {
		R.execute(); //after this the render is done
		{
			lock_guard<mutex> guard(renders);
			activeRenders--;
		}
	}
	else {
		thread refreshThread(refreshDuringRender<formula_identifier, guessing, use_avx, julia>, ref(R), ref(*this), renderID);
		R.execute(); //after this the render is done
		{
			lock_guard<mutex> guard(renders);
			activeRenders--;
		}
		{
			lock_guard<mutex> guard(drawingBitmap);	
			if (lastRenderID == renderID) { //which means: if the render was not cancelled	
				HDC hdc = GetDC(hWndMain);
				DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY, S.get_screenWidth(), S.get_screenHeight());
				ReleaseDC(hWndMain, hdc);
				string elapsedString = to_string(R.getElapsedTime()) + " s";
				SendMessageA(statusBar, SB_SETTEXTA, 0, (LPARAM)("100%"));
				SendMessageA(statusBar, SB_SETTEXTA, 1, (LPARAM)(elapsedString.c_str()));
				firstPaint = false;
			}
		}
		refreshThread.join();
	}

	{
		//Printing information after the render
		int pixelGroupings = R.pixelGroupings;
		int guessedPixels = R.guessedPixelCount;

		string elapsedString = to_string(R.getElapsedTime()) + " s";
		cout << "Elapsed time: " << elapsedString << endl;
		cout << "computed iterations: " << R.computedIterations << endl;
		cout << "iterations per second: " << ((long long)(R.computedIterations / R.getElapsedTime()) / 1000000.0) << " M" << endl;
		cout << "used threads: " << R.usedThreads / 2 << endl; // divide by 2 because each thread is counted when created and when destroyed by addToThreadcount
		cout << "guessedPixelCount: " << R.guessedPixelCount << " / " << width * height << " = " << (double)R.guessedPixelCount / (width*height) << endl;
		cout << "calculatedPixelCount" << R.calculatedPixelCount << " / " << width * height << " = " << (double)R.calculatedPixelCount / (width*height) << endl;
		cout << "pixelGroupings: " << R.pixelGroupings << endl;
	}
	return;
}

//this is the most used form of a case: the procedure has no AVX implementation, can use guessing and has a julia version
#define procedureRenderCase(formula_identifier) \
	case formula_identifier: { \
		if (S.get_julia()) { \
			createNewRenderTemplated<formula_identifier, true, false, true>(headless); \
		} \
		else { \
			createNewRenderTemplated<formula_identifier, true, false, false>(headless); \
		} \
		break; \
	}

/* createNewRenderTemplated has template:
	template <int formula_identifier, bool guessing, bool use_avx, bool julia>
*/
void FractalCanvas::createNewRender(bool headless) {
	if(debug) {
		int formulaID = S.get_formula_identifier();
		assert(formulaID == S.get_formula().identifier);
		cout << "creating new render with formula: " << formulaID << " (" << S.get_formula().name << ")" << endl;
	}
	switch (S.get_formula_identifier()) {
		case PROCEDURE_M2: {
			if (using_avx) {
				if (S.get_julia()) {
					createNewRenderTemplated<PROCEDURE_M2, true, true, true>(headless);
				}
				else {
					createNewRenderTemplated<PROCEDURE_M2, true, true, false>(headless);
				}
			}
			else {
				if (S.get_julia()) {
					createNewRenderTemplated<PROCEDURE_M2, true, false, true>(headless);
				}
				else {
					createNewRenderTemplated<PROCEDURE_M2, true, false, false>(headless);
				}
			}
			break;
		}
		case PROCEDURE_BURNING_SHIP : {
			if (S.get_julia()) {
				createNewRenderTemplated<PROCEDURE_BURNING_SHIP, false, false, true>(headless);
			}
			else {
				createNewRenderTemplated<PROCEDURE_BURNING_SHIP, false, false, false>(headless);
			}
			break;
		}
		case PROCEDURE_TEST_CONTROL: {
			createNewRenderTemplated<PROCEDURE_TEST_CONTROL, true, false, false>(headless);
			break;
		}
		case PROCEDURE_CHECKERS : {
			createNewRenderTemplated<PROCEDURE_CHECKERS, true, false, false>(headless);
			break;
		}
		procedureRenderCase(PROCEDURE_M3)
		procedureRenderCase(PROCEDURE_M4)
		procedureRenderCase(PROCEDURE_M5)
		procedureRenderCase(PROCEDURE_HIGH_POWER)
		procedureRenderCase(PROCEDURE_TRIPLE_MATCHMAKER)
	}
}

void refreshBitmap(bool viewGuessedPixels) {
	canvas.createNewBitmapRender(false, viewGuessedPixels); //uses drawingBitmap mutex
}

void refreshBitmapThread(bool viewGuessedPixels) {
	if(debug) cout << "refresh bitmap" << endl;
	thread t(refreshBitmap, viewGuessedPixels);
	t.detach();
}

void setWindowSize(int windowWidth, int windowHeight) {
	RECT mainWndRect;
	mainWndRect.left = mainWndRect.top = 0;
	mainWndRect.right = windowWidth;
	mainWndRect.bottom = windowHeight;
	AdjustWindowRect(&mainWndRect, WS_OVERLAPPEDWINDOW, TRUE);
	RECT statusBarRect;
	GetWindowRect(statusBar, &statusBarRect);
	SetWindowPos(hWndMain, HWND_TOP, 0, 0,
		mainWndRect.right - mainWndRect.left,
		(mainWndRect.bottom - mainWndRect.top) + (statusBarRect.bottom - statusBarRect.top),
		SWP_NOMOVE
	);

	InvalidateRect(hWndMain, NULL, TRUE);
}

/*
inline double_c map(int i, int j) {
	//Maps a pixel at location (i, j) in the window to a complex number.
	return S.get_topleftCorner() + i * S.get_pixelWidth() - j * S.get_pixelHeight()*I;
}
*/

inline double_c inflections(double_c c) {
	for (int i = canvas.S.get_inflectionCount() - 1; i >= 0; i--) {
		c = pow(c, canvas.S.get_formula().inflectionPower) + canvas.S.get_inflectionCoords()[i];
	}
	return c;
}

void recalculate() {
	cout << "recalculate" << endl;
	canvas.cancelRender();
	thread t(&FractalCanvas::createNewRender, &canvas, false);
	t.detach();
	PostMessageA(hOptions, CUSTOM_REFRESH, 0, 0);
}

void saveImage(string filename) {
	int screenWidth = canvas.S.get_screenWidth();
	int screenHeight = canvas.S.get_screenHeight();
	int width = canvas.S.get_width();
	int height = canvas.S.get_height();

	PICTDESC pictdesc = {};
	pictdesc.cbSizeofstruct = sizeof(pictdesc);
	pictdesc.picType = PICTYPE_BITMAP;
	pictdesc.bmp.hbitmap = screenBMP;

	CComPtr<IPicture> picture;
	OleCreatePictureIndirect(&pictdesc, __uuidof(IPicture), FALSE, (LPVOID*)&picture);
	//save to file
	CComPtr<IPictureDisp> disp;
	picture->QueryInterface(&disp);
	OleSavePictureFile(disp, CComBSTR(filename.c_str()));
}

string getDate() {
	std::time_t t = std::time(0);   // get time now
	std::tm* now = std::localtime(&t);
	std::stringstream s;
	s << (now->tm_year + 1900) << '-'
		<< (now->tm_mon < 10 ? "0" : "") << (now->tm_mon + 1) << '-'
		<< (now->tm_mday < 10 ? "0" : "") << now->tm_mday << " "
		<< (now->tm_hour < 10 ? "0" : "") << now->tm_hour << ";"
		<< (now->tm_min < 10 ? "0" : "") << now->tm_min;
	return s.str();
}

int fps = 60;
double secondsPerInflection = 3;
double secondsPerZoom = 0.6666666666666;

void animation() {
	FractalParameters& S = canvas.S;

	string path = write_directory;
	int framesPerInflection = (int)(fps * secondsPerInflection);
	int framesPerZoom = (int)(fps * secondsPerZoom);

	cout << "rendering animation with" << endl;
	cout << fps << " fps" << endl;
	cout << framesPerInflection << " frames per inflection" << endl;
	cout << framesPerZoom << " frames per zoom" << endl;

	auto makeFrame = [&path](int frame, bool recalculate) {
		if (recalculate)
			canvas.createNewRender(true); //the whole render takes place in this thread so after this the render is done

		std::stringstream num;
		num << std::setfill('0') << std::setw(6); //numbering 000001, 000002, 000003, ...
		num << frame;

		string filename = "frame" + num.str() + ".bmp";
		cout << "saving image " << filename << endl;
		saveImage(path + filename);
	};


	S.pre_transformation_type = 7; //broken inflection power transformation
	vector<double_c> inflections = S.get_inflectionCoords();
	int inflectionCount = S.get_inflectionCount();
	while(S.removeInflection(0,0)); //reset to 0 inflections
	S.setCenterAndZoom(0, 0); //start the animation unzoomed and centered
	S.partialInflectionCoord = 0;

	double inflectionPowerStepsize = 1.0 / (framesPerInflection - 1);
	int frame = 1;

	makeFrame(frame++, true);

	for (int inflection=0; inflection < inflectionCount; inflection++) {

		S.partialInflectionPower = 1;
		S.partialInflectionCoord = 0;		

		double_c thisInflectionCoord = inflections[inflection];
		double_c currentCenter = S.get_center();
		double_c diff = thisInflectionCoord - currentCenter;

		//gradually move the center towards the next inflection location
		if (thisInflectionCoord != 0.0 + 0.0*I) {
			for (int i=1; i<=framesPerInflection; i++) {

				S.setCenterAndZoom(
					currentCenter + diff * ((1.0 / framesPerInflection) * i)
					, S.getZoomLevel()
				);

				makeFrame(frame++, true);
			}
		}

		double currentZoom = S.getZoomLevel();
		double targetZoom = S.get_inflectionZoomLevel() * (1 / pow(2, S.get_inflectionCount()));
		double zoomDiff = targetZoom - currentZoom;
		double zoomStepsize = 1.0 / framesPerZoom;

		//and zoom to the inflection zoom level
		if (zoomStepsize > 0.001) {
			for (int i=1; i <= framesPerZoom * zoomDiff; i++) {

				S.setCenterAndZoom(
					S.get_center()
					, currentZoom + zoomStepsize * i
				);

				makeFrame(frame++, true);
			}
		}

		S.partialInflectionCoord = thisInflectionCoord;

		//gradually apply the next inflection by letting the inflection power go from 1 to 2.
		for (int i=0; i<framesPerInflection; i++) {
			S.setCenterAndZoom(0, (
				S.get_inflectionZoomLevel()
				* (1 / pow(2, S.get_inflectionCount()))
				* (1 / S.partialInflectionPower)
			));
			//How to handle different powers? Something like (partialPower - 1) / (inflectionPower - 1). When inflectionPower is 2 that division has no effect.

			makeFrame(frame++, true);

			S.partialInflectionPower += inflectionPowerStepsize;
		}
		S.addInflection(S.partialInflectionCoord);
	}

	//repeat the last frame
	for (int i=0; i<framesPerInflection; i++)
		makeFrame(frame++, false);

	S.pre_transformation_type = 0;
}

//identifiers start at 300:
const int mainWndStatusBar = 300;

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	
	case WM_CREATE: {
		hWndMain = hWnd;
		AddMenus(hWnd);
		statusBar = CreateWindowExW(0, STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)mainWndStatusBar, hInst, NULL);
		int widths[4] = { 80, 180, 380, -1 };
		SendMessage(statusBar, SB_SETPARTS, 4, (LPARAM)&widths);

		setWindowSize(canvas.S.get_screenWidth(), canvas.S.get_screenHeight());

		//canvas.S.setGradientOffset(defaultParameters.get_gradientOffset() + 0.00001);
		//canvas.changeParameters(defaultParameters);
		if ( ! render_animation)
			recalculate();

		break;
	}
	
	case WM_SIZE: {
		SendMessage(statusBar, message, wParam, lParam);
		break;
	}
	case WM_COMMAND: {
		//menu selection
		bool fractalTypeChange = canvas.S.changeFormula(LOWORD(wParam));
		if (fractalTypeChange) {
			canvas.S.setCenterAndZoom(0, 0);
			recalculate();
			break;
		}

		switch (LOWORD(wParam)) {//other menu buttons
			case RESET: {
				canvas.S.reset();
				recalculate();
				break;
			}
			case TOGGLE_JULIA: {
				canvas.S.toggleJulia();
				recalculate();
				break;
			}
			case CHANGE_TRANSFORMATION: {
				canvas.S.changeTransformation();
				canvas.S.setCenterAndZoom(0, 0);
				recalculate();
				break;
			}
			case WINDOW_OPTIONS: {
				//SendMessage(hOptions, WM_DESTROY, NULL, NULL); //destroy currently active options window? doesn't work
				createOptionsWindow();
				break;
			}
			case WINDOW_JSON: {
				createJsonWindow();
				break;
			}
			case VIEW_GUESSED_PIXELS: {
				refreshBitmapThread(true);
				break;
			}
			case SAVE_IMAGE: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, FALSE, "Save bitmap", "Bitmap\0*.bmp\0\0", filename)) {
					saveImage(filename);
				}
				break;
			}
			case SAVE_PARAMETERS: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, FALSE, "Save parameters", "Parameters\0*.efp\0\0", filename)) {
					writeParameters(canvas.S, filename);
				}
				break;
			}
			case SAVE_BOTH: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, FALSE, "Save parameters", "Parameters\0*.efp\0\0", filename)) {
					writeParameters(canvas.S, filename);
					saveImage(filename + ".bmp");
				}
				break;
			}
			case LOAD_PARAMETERS: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, TRUE, "Load parameters", "Parameters\0*.efp\0\0", filename)) {
					FractalParameters newS;
					newS.initialize();
					bool success = readParameters(newS, filename);
					assert(success);
					if (success) {
						success = canvas.changeParameters(newS);
						assert(success);
						if (success) {
							setWindowSize(newS.get_screenWidth(), newS.get_screenHeight());
							recalculate();
						}
					}
				}
				break;
			}
			case CANCEL_RENDER: {
				canvas.cancelRender();
				break;
			}
			case QUIT: {
				PostQuitMessage(0);
				break;
			}
		}
		break;
	}
	case WM_PAINT: {
		if (firstPaint) break;
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOWTEXT + 1));
		DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY, canvas.S.get_screenWidth(), canvas.S.get_screenHeight());
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_MOUSEMOVE: {
		int oversampling = canvas.S.get_oversampling();
		int width = canvas.S.get_width();
		int height = canvas.S.get_height();
		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		int xPos = point.x * oversampling;
		int yPos = point.y * oversampling;
		if (xPos < 0 || xPos > width || yPos < 0 || yPos > height) {
			return 0;
		}

		IterData thisPixel = canvas.getIterData(xPos, yPos);
		double_c thisComplexNumber = post_transformation(inflections(pre_transformation(canvas.S.map(xPos, yPos))));
		string complexNumber = to_string(real(thisComplexNumber)) + " + " + to_string(imag(thisComplexNumber)) + "* I";
		string iterationData = "iters: " + to_string(thisPixel.iterationCount);
		SendMessageA(statusBar, SB_SETTEXTA, 2, (LPARAM)(complexNumber.c_str()));
		SendMessageA(statusBar, SB_SETTEXTA, 3, (LPARAM)(iterationData.c_str()));

		break;
	}
	case WM_MOUSEWHEEL: {
		//zoom action
		int oversampling = canvas.S.get_oversampling();
		int screenWidth = canvas.S.get_screenWidth();
		int screenHeight = canvas.S.get_screenHeight();
		int width = canvas.S.get_width();
		int height = canvas.S.get_height();
		double_c center = canvas.S.get_center();

		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ScreenToClient(hWnd, &point);
		//int xPos = point.x * oversampling;
		//int yPos = point.y * oversampling;
		int xPos = point.x;
		int yPos = point.y;
		if (xPos < 0 || xPos > screenWidth || yPos < 0 || yPos > screenHeight) {
			return 0;
		}
		canvas.cancelRender();

		//set new parameters
		//center = (map(xPos, yPos)+(map(xPos, yPos)+center)/2)/2; //working formula, maybe can be simplified
		//the way this works is applying the transformation for a zoom size of 2, 2 times.
		//for the other direction: 2*(2*center-map(xPos, yPos))-map(xPos, yPos) is two times the inverse transformation
		//center = map(xPos, yPos); //old method
		/*
		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (zDelta > 0) {
			S.setCenterAndZoom(
				0.75*(S.map(xPos, yPos)) + 0.25*center,
				S.getZoomLevel() + 2
			);
		}
		else {
			S.setCenterAndZoom(
				4.*center - 3.*(S.map(xPos, yPos)),
				S.getZoomLevel() - 2
			);
		}
		*/

		//new version of the above, for any zoom size, currently only uses the zoom size when zooming in
		double zooms = 2;
		double magnificationStep = pow(2, -zooms);
		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (zDelta > 0) {
			canvas.S.setCenterAndZoom(
				(1-magnificationStep)*(canvas.S.map(xPos * oversampling, yPos * oversampling)) + magnificationStep*center,
				canvas.S.getZoomLevel() + zooms
			);
		}
		else {
			canvas.S.setCenterAndZoom(
				4.*center - 3.*(canvas.S.map(xPos * oversampling, yPos * oversampling)),
				canvas.S.getZoomLevel() - 2
			);
		}

		//generate preview bitmap
		BITMAP Bitmap;
		HDC screenHDC = CreateCompatibleDC(NULL);
		GetObject(screenBMP, sizeof(BITMAP), (LPSTR)&Bitmap);
		SelectObject(screenHDC, screenBMP);

		if (zDelta > 0) {
			StretchBlt(screenHDC, 0, 0, screenWidth, screenHeight, screenHDC, xPos - xPos / 4, yPos - yPos / 4, screenWidth / 4, screenHeight / 4, SRCCOPY); //stretch the screen bitmap
		}
		else {
			SetStretchBltMode(screenHDC, HALFTONE);
			StretchBlt(screenHDC, xPos - xPos / 4, yPos - yPos / 4, screenWidth / 4, screenHeight / 4, screenHDC, 0, 0, screenWidth, screenHeight, SRCCOPY);
		}
		DeleteDC(screenHDC);

		recalculate();
		break;
	}
	case WM_LBUTTONUP: {
		//create inflection
		int oversampling = canvas.S.get_oversampling();
		int xPos = GET_X_LPARAM(lParam) * oversampling;
		int yPos = GET_Y_LPARAM(lParam) * oversampling;
		bool added = canvas.S.addInflection(xPos, yPos);

		if (added) {
			canvas.S.printInflections();
			recalculate();
		}
		break;
	}
	case WM_RBUTTONUP: {
		//remove inflection
		int oversampling = canvas.S.get_oversampling();
		int xPos = GET_X_LPARAM(lParam) * oversampling;
		int yPos = GET_Y_LPARAM(lParam) * oversampling;
		bool removed = canvas.S.removeInflection(xPos, yPos);

		if (removed) {
			canvas.S.printInflections();
			recalculate();
		}
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_ERASEBKGND: {
		return true;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return 0;
}

HWND optionsElements[30];
HWND& optionsElt(int index) {
	return optionsElements[index - 100];
}
//identifiers start at 100:
const int optionWidth = 100;
const int optionHeight = 101;
const int optionOk = 102;
const int optionApply = 103;
const int optionCancel = 104;
const int optionHeightText = 105;
const int optionWidthText = 106;
const int optionGradientSpeed = 107;
const int optionGradientSpeedText = 108;
const int optionGradientSpeedTrackbarCoarse = 109;
const int optionGradientSpeedTrackbarFine = 110;
const int optionGradientSpeedTextCoarse = 111;
const int optionGradientSpeedTextFine = 112;
const int optionZoomLevelText = 113;
const int optionZoomLevel = 114;
const int optionMaxItersText = 115;
const int optionMaxIters = 116;
const int optionsOversamplingText = 117;
const int optionsRadioAA_1 = 118;
const int optionsRadioAA_2 = 119;
const int optionsRadioAA_3 = 120;
const int optionsRadioAA_4 = 121;
const int optionsRadioAA_6 = 122;
const int optionsRadioAA_8 = 123;
const int optionsRadioAA_12 = 124;
const int optionsRadioAA_16 = 125;
const int optionGradientOffsetText = 126;
const int optionGradientOffsetTrackbar = 127;
const int optionSetInflectionZoom = 128;

int getOverSampleIdentifier() {
	switch (canvas.S.get_oversampling()) {
		case 1:		return optionsRadioAA_1;
		case 2:		return optionsRadioAA_2;
		case 3:		return optionsRadioAA_3;
		case 4:		return optionsRadioAA_4;
		case 6:		return optionsRadioAA_6;
		case 8:		return optionsRadioAA_8;
		case 12:		return optionsRadioAA_12;
		case 16:		return optionsRadioAA_16;
	}
	return 0;
}

LRESULT CALLBACK OptionsProc(HWND hOptions, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message)
	{
	case WM_CREATE: {
		//Create all items: buttons, text fields...
		optionsElt(optionWidthText) = CreateWindowExA(0, "STATIC", "Width:", WS_VISIBLE | WS_CHILD, 0, 0, 100, 30, hOptions, (HMENU)optionWidthText, hInst, NULL);
		optionsElt(optionWidth) = CreateWindowExA(0, "EDIT", "", WS_VISIBLE | WS_CHILD, 60, 0, 100, 30, hOptions, (HMENU)optionWidth, hInst, NULL);
		optionsElt(optionHeightText) = CreateWindowExA(0, "STATIC", "Height:", WS_VISIBLE | WS_CHILD, 170, 0, 100, 30, hOptions, (HMENU)optionHeightText, hInst, NULL);
		optionsElt(optionHeight) = CreateWindowExA(0, "EDIT", "", WS_VISIBLE | WS_CHILD, 240, 0, 100, 30, hOptions, (HMENU)optionHeight, hInst, NULL);

		optionsElt(optionsOversamplingText) = CreateWindowExA(0, "STATIC", "Oversampling:", WS_VISIBLE | WS_CHILD, 480, 10, 120, 30, hOptions, (HMENU)optionsOversamplingText, hInst, NULL);
		optionsElt(optionsRadioAA_1) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "1x1", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, 480, 30, 80, 20, hOptions, (HMENU)optionsRadioAA_1, hInst, NULL);
		optionsElt(optionsRadioAA_2) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "2x2", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 480, 60, 80, 20, hOptions, (HMENU)optionsRadioAA_2, hInst, NULL);
		optionsElt(optionsRadioAA_3) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "3x3", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 480, 90, 80, 20, hOptions, (HMENU)optionsRadioAA_3, hInst, NULL);
		optionsElt(optionsRadioAA_4) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "4x4", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 480, 120, 80, 20, hOptions, (HMENU)optionsRadioAA_4, hInst, NULL);
		optionsElt(optionsRadioAA_6) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "6x6", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 480, 150, 80, 20, hOptions, (HMENU)optionsRadioAA_6, hInst, NULL);
		optionsElt(optionsRadioAA_8) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "8x8", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 480, 180, 80, 20, hOptions, (HMENU)optionsRadioAA_8, hInst, NULL);
		optionsElt(optionsRadioAA_12) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "12x12", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 480, 210, 80, 20, hOptions, (HMENU)optionsRadioAA_12, hInst, NULL);
		optionsElt(optionsRadioAA_16) = CreateWindowExA(WS_EX_WINDOWEDGE, "BUTTON", "16x16", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 480, 240, 80, 20, hOptions, (HMENU)optionsRadioAA_16, hInst, NULL);

		optionsElt(optionGradientSpeedText) = CreateWindowExA(0, "STATIC", "Gradient speed:", WS_VISIBLE | WS_CHILD, 0, 40, 130, 30, hOptions, (HMENU)optionGradientSpeedText, hInst, NULL);
		optionsElt(optionGradientSpeed) = CreateWindowExA(0, "EDIT", "", WS_VISIBLE | WS_CHILD, 130, 40, 150, 30, hOptions, (HMENU)optionGradientSpeed, hInst, NULL);

		optionsElt(optionGradientSpeedTextCoarse) = CreateWindowExA(0, "STATIC", "Coarse", WS_VISIBLE | WS_CHILD, 0, 85, 60, 30, hOptions, (HMENU)optionGradientSpeedTextCoarse, hInst, NULL);
		optionsElt(optionGradientSpeedTrackbarCoarse) = CreateTrackbar(hOptions, 0, 100, 60, 85, 400, 30, optionGradientSpeedTrackbarCoarse, hInst);
		optionsElt(optionGradientSpeedTextFine) = CreateWindowExA(0, "STATIC", "Fine", WS_VISIBLE | WS_CHILD, 0, 115, 110, 30, hOptions, (HMENU)optionGradientSpeedTextFine, hInst, NULL);
		optionsElt(optionGradientSpeedTrackbarFine) = CreateTrackbar(hOptions, 0, 100, 60, 115, 400, 30, optionGradientSpeedTrackbarFine, hInst);
		optionsElt(optionGradientOffsetText) = CreateWindowExA(0, "STATIC", "Offset", WS_VISIBLE | WS_CHILD, 0, 145, 130, 30, hOptions, (HMENU)optionGradientOffsetText, hInst, NULL);
		optionsElt(optionGradientOffsetTrackbar) = CreateTrackbar(hOptions, 0, 100, 60, 145, 400, 30, optionGradientOffsetTrackbar, hInst);
		double gradientSpeed = canvas.S.get_gradientSpeed();
		SendMessage(optionsElt(optionGradientSpeedTrackbarCoarse), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)((int)gradientSpeed));
		SendMessage(optionsElt(optionGradientSpeedTrackbarFine), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(100 * (gradientSpeed - (int)gradientSpeed)));
		SendMessage(optionsElt(optionGradientOffsetTrackbar), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(100 * canvas.S.get_gradientOffset()));

		optionsElt(optionZoomLevelText) = CreateWindowExA(0, "STATIC", "Zoom:", WS_VISIBLE | WS_CHILD, 0, 200, 60, 30, hOptions, (HMENU)optionZoomLevelText, hInst, NULL);
		optionsElt(optionZoomLevel) = CreateWindowExA(0, "EDIT", "", WS_VISIBLE | WS_CHILD, 60, 200, 150, 30, hOptions, (HMENU)optionZoomLevel, hInst, NULL);
		optionsElt(optionSetInflectionZoom) = CreateWindowExA(0, "BUTTON", "Set as inflection zoom", WS_CHILD | WS_VISIBLE, 250, 200, 180, 30, hOptions, (HMENU)optionSetInflectionZoom, hInst, NULL);

		optionsElt(optionMaxItersText) = CreateWindowExA(0, "STATIC", "Max iterations:", WS_VISIBLE | WS_CHILD, 0, 235, 130, 30, hOptions, (HMENU)optionMaxItersText, hInst, NULL);
		optionsElt(optionMaxIters) = CreateWindowExA(0, "EDIT", "", WS_VISIBLE | WS_CHILD, 130, 235, 100, 30, hOptions, (HMENU)optionMaxIters, hInst, NULL);

		optionsElt(optionOk) = CreateWindowExA(0, "BUTTON", "Ok", WS_CHILD | WS_VISIBLE, 0, 270, 60, 30, hOptions, (HMENU)optionOk, hInst, NULL);
		optionsElt(optionApply) = CreateWindowExA(0, "BUTTON", "Apply", WS_CHILD | WS_VISIBLE, 80, 270, 60, 30, hOptions, (HMENU)optionApply, hInst, NULL);
		optionsElt(optionCancel) = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE, 160, 270, 60, 30, hOptions, (HMENU)optionCancel, hInst, NULL);

		ShowWindow(hOptions, SW_SHOW);
		//break intentionally left out
	}
	case CUSTOM_REFRESH: {
		//set newOversampling
		assert(canvas.S.get_oversampling() == canvas.S.get_height() / canvas.S.get_screenHeight());

		SendMessage(optionsElt(getOverSampleIdentifier()), BM_SETCHECK, BST_CHECKED, 0);

		//set gradient speed
		SetDlgItemTextA(hOptions, optionGradientSpeed, to_string(canvas.S.get_gradientSpeed()).c_str());

		//set width, height
		SetDlgItemTextA(hOptions, optionHeight, to_string(canvas.S.get_screenHeight()).c_str());
		SetDlgItemTextA(hOptions, optionWidth, to_string(canvas.S.get_screenWidth()).c_str());

		//Set zoom level
		SetDlgItemTextA(hOptions, optionZoomLevel, to_string(canvas.S.getZoomLevel()).c_str());

		//Set max iters
		SetDlgItemTextA(hOptions, optionMaxIters, to_string(canvas.S.get_maxIters()).c_str());
		break;
	}
	case WM_HSCROLL: {
		SendMessage(hOptions, CUSTOM_REFRESH, 0, 0);
		bool refreshBitmapRequired = false;
		if (lParam == (LPARAM)optionsElt(optionGradientSpeedTrackbarCoarse) || lParam == (LPARAM)optionsElt(optionGradientSpeedTrackbarFine)) {
			int posCoarse = SendMessage(optionsElt(optionGradientSpeedTrackbarCoarse), TBM_GETPOS, 0, 0);
			int posFine = SendMessage(optionsElt(optionGradientSpeedTrackbarFine), TBM_GETPOS, 0, 0);
			double newGradientSpeed = posCoarse + 0.01*(double)posFine;
			refreshBitmapRequired = canvas.S.setGradientSpeed(newGradientSpeed);
		}
		else if (lParam == (LPARAM)optionsElt(optionGradientOffsetTrackbar)) {
			double newGradientOffset = 0.01 * SendMessage(optionsElt(optionGradientOffsetTrackbar), TBM_GETPOS, 0, 0);
			cout << "new gradientOffset: " << newGradientOffset << endl;
			refreshBitmapRequired = canvas.S.setGradientOffset(newGradientOffset);

		}
		if (refreshBitmapRequired)
			refreshBitmapThread(false);
		break;
	}
	case WM_COMMAND: {
		if (wParam == optionSetInflectionZoom) {
			canvas.S.setInflectionZoomLevel();
		}
		if (wParam == optionApply || wParam == optionOk) {
			bool recalcNeeded = false;

			//change iteration count:
			char aMaxIters[16]; GetDlgItemTextA(hOptions, optionMaxIters, aMaxIters, sizeof(aMaxIters));
			int newMaxIters = atoi(aMaxIters);
			recalcNeeded |= canvas.S.setMaxIters(newMaxIters);

			//change gradient speed:
			char aGradientSpeed[16]; GetDlgItemTextA(hOptions, optionGradientSpeed, aGradientSpeed, sizeof(aGradientSpeed));
			double newGradientSpeed = strtod(aGradientSpeed, NULL);
			bool refreshNeeded = canvas.S.setGradientSpeed(newGradientSpeed);

			//change zoom level:
			char aZoomLevel[16]; GetDlgItemTextA(hOptions, optionZoomLevel, aZoomLevel, sizeof(aZoomLevel));
			double newZoomLevel = strtod(aZoomLevel, NULL);
			recalcNeeded |= canvas.S.setCenterAndZoom(canvas.S.get_center(), newZoomLevel);

			//resize:
			char aWidth[16]; GetDlgItemTextA(hOptions, optionWidth, aWidth, sizeof(aWidth));
			char aHeight[16]; GetDlgItemTextA(hOptions, optionHeight, aHeight, sizeof(aHeight));
			int newScreenWidth = atoi(aWidth);
			int newScreenHeight = atoi(aHeight);

			//change newOversampling
			int newOversampling = canvas.S.get_oversampling();
			if (IsDlgButtonChecked(hOptions, optionsRadioAA_1) == BST_CHECKED) newOversampling = 1;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_2) == BST_CHECKED) newOversampling = 2;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_3) == BST_CHECKED) newOversampling = 3;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_4) == BST_CHECKED) newOversampling = 4;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_6) == BST_CHECKED) newOversampling = 6;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_8) == BST_CHECKED) newOversampling = 8;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_12) == BST_CHECKED) newOversampling = 12;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_16) == BST_CHECKED) newOversampling = 16;

			bool success;
			recalcNeeded |= canvas.resize(newOversampling, newScreenWidth, newScreenHeight, success);
			if (success)
				setWindowSize(newScreenWidth, newScreenHeight);

			if (recalcNeeded) {
				recalculate();
			}
			else if (refreshNeeded) {
				refreshBitmapThread(false);
			}
		}
		if (wParam == optionOk || wParam == optionCancel) {
			DestroyWindow(hOptions);
		}
		break;
	}
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hOptions, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW));
		EndPaint(hOptions, &ps);
		break;
	}
	case WM_DESTROY: {
		break;
	}
	case WM_ERASEBKGND: {
		return true;
	}
	default:
		return DefWindowProc(hOptions, message, wParam, lParam);
		break;
	}
	return 0;
}

HWND JsonElements[30];
HWND& JsonElt(int index) {
	return JsonElements[index - 200];
}
//identifiers start at 200:
const int JsonOk = 200;
const int JsonApply = 201;
const int JsonCancel = 202;
const int JsonRefresh = 204;
const int JsonText = 205;

LRESULT CALLBACK JsonProc(HWND hJson, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message)
	{
	case WM_CREATE: {
		//Create all items: buttons, text fields...
		JsonElt(JsonText) = CreateWindowExA(0, "EDIT", "",
			WS_CHILD | WS_VISIBLE | ES_LEFT | ES_WANTRETURN | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_MULTILINE | WS_VSCROLL | WS_HSCROLL,
			0, 0, 400, 600, hJson, (HMENU)JsonText, GetModuleHandle(NULL), NULL
		);

		JsonElt(JsonOk) = CreateWindowExA(0, "BUTTON", "Ok", WS_CHILD | WS_VISIBLE, 0, 610, 60, 30, hJson, (HMENU)JsonOk, hInst, NULL);
		JsonElt(JsonApply) = CreateWindowExA(0, "BUTTON", "Apply", WS_CHILD | WS_VISIBLE, 80, 610, 60, 30, hJson, (HMENU)JsonApply, hInst, NULL);
		JsonElt(JsonCancel) = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE, 160, 610, 60, 30, hJson, (HMENU)JsonCancel, hInst, NULL);
		JsonElt(JsonRefresh) = CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE, 240, 610, 70, 30, hJson, (HMENU)JsonRefresh, hInst, NULL);

		ShowWindow(hJson, SW_SHOW);
		//break intentionally left out
	}
	case CUSTOM_REFRESH: {
		string textFieldContent = regex_replace(canvas.S.toJson(), regex("\n"), "\r\n"); //this is necessary because the text field doesn't display newlines otherwise
		SetDlgItemTextA(hJson, JsonText, textFieldContent.c_str());
		break;
	}
	case WM_COMMAND: {
		if (wParam == JsonRefresh) {
			SendMessage(hJson, CUSTOM_REFRESH, 0, 0);
		}
		if (wParam == JsonApply || wParam == JsonOk) {
			//allocating 1 MB of memory for the JSON string typed by the user, which should usually be enough. This should be improved when possible.
			int jsonCharsLimit = 1000000;
			char* aJson = (char*)calloc(jsonCharsLimit, sizeof(char));
			GetDlgItemTextA(hJson, JsonText, aJson, jsonCharsLimit);
			string json(aJson);
			if ( ! canvas.S.fromJson(json)) cout << "The JSON that you entered could not be parsed.";
			recalculate();
			free(aJson);
		}
		if (wParam == JsonOk || wParam == JsonCancel) {
			DestroyWindow(hJson);
		}
		break;
	}
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hJson, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW));
		EndPaint(hJson, &ps);
		break;
	}
	case WM_DESTROY: {
		break;
	}
	case WM_ERASEBKGND: {
		return true;
	}
	default:
		return DefWindowProc(hJson, message, wParam, lParam);
		break;
	}
	return 0;
}


//utf16 to utf8 converter from https://stackoverflow.com/a/35103224/10336025
std::string utf16_to_utf8(std::u16string utf16_string)
{
    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> convert;
    auto p = reinterpret_cast<const int16_t *>(utf16_string.data());
    return convert.to_bytes(p, p + utf16_string.size());
}

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR	 lpCmdLine,
	_In_ int	   nCmdShow
)
{
	setvbuf(stdout, NULL, _IOLBF, 1024);
	cout << setprecision(21);	//precision when printing doubles
	
	if(debug) cout << "debug version" << endl;
	//difficulty because windows uses UTF16
	int argc;
	char16_t** argv = (char16_t**)CommandLineToArgvW(GetCommandLineW(), &argc);
	vector<string> commands(argc);
	for (int i=0; i<argc; i++) {
		u16string u16 = u16string(argv[i]);
		string normal = utf16_to_utf8(u16);
		commands[i] = normal;
	}
	
	bool override_interactive = false;
	int override_width = -1;
	int override_height = -1;
	int override_oversampling = -1;

	for (int i=0; i<commands.size(); i++) {
		string c = commands[i];

		if (c == "-i") {
			override_interactive = true;
			interactive = true;
		}
		else if (c == "--animation") {
			render_animation = true;
			if (!override_interactive) {
				interactive = false;
			}
		}
		else if (c == "--fps") {
			if (i+1 < argc) {
				fps = stoi(commands[i+1]);
			}
		}
		else if (c == "--spi") {
			if (i+1 < argc) {
				secondsPerInflection = stod(commands[i+1]);
			}
		}
		else if (c == "--spz") {
			if (i+1 < argc) {
				secondsPerZoom = stod(commands[i+1]);
			}
		}
		else if (c == "--image") {
			render_image = true;
			if (!override_interactive) {
				interactive = false;
			}
		}
		else if (c == "--width") {
			if (i+1 < argc) {
				override_width = stoi(commands[i+1]);
			}
		}
		else if (c == "--height") {
			if (i+1 < argc) {
				override_height = stoi(commands[i+1]);
			}
		}
		else if (c == "--oversampling") {
			if (i+1 < argc) {
				override_oversampling = stoi(commands[i+1]);
			}
		}
		else if (c == "-o") {
			if (i+1 < argc) {
				string s = commands[i+1];
				s = regex_replace(s, regex("\\\\"), "/");	 //to allow both C:\ and C:/
				if (s.back() != '/')
					s += '/';		//the rest of the program expects a directory to end with /
				write_directory = s;
			}
		}
		else if (c == "-p") {
			if (i+1 < argc) {
				parameterfile = commands[i+1];
			}
		}
		else if (c == "--help" || c == "-h") {
			cout << (R"(
all available parameters:
    -p name.efp     use the file name.efp as initial parameters. default: default.efp
	--width         override the width parameter
	--height        override the height parameter
	--oversampling  override the oversampling parameter
    --image         render the initial parameter file to an image
    --animation     render an animation of the initial parameters
    --fps number    the number of frames per second (integer)
    --spi number    the number of seconds per inflection (floating point)
    --spz number    the number of seconds per zoom (floating point)
    -o directory    use the directory as output directory for animation frames (example: C:\folder)
    -i              do not close the program after rendering an image or animation to continue interactive use
    --help or -h    show this text

examples:
    ExploreFractals -p file.efp --animation --fps 60 --spi 3 --spz 0.6666 -o C:\folder -i
	ExploreFractals -p name.efp --width 1920 --height 1080 --oversampling 2
)"			) << endl;
			return 0;
		}
	}

	cout << "animation: " << (render_animation ? "yes" : "no") << endl;
	cout << "initial parameters: " << parameterfile << endl;
	cout << "override width, height, oversampling: "
		<< (override_width != -1 ? "yes" : "no") << " "
		<< (override_height != -1 ? "yes" : "no") << " "
		<< (override_oversampling != -1 ? "yes" : "no") << endl;
	cout << "output directory: " << write_directory << endl;
	cout << "interactive use: " << (interactive ? "yes" : "no") << endl;
	

	NUMBER_OF_THREADS = thread::hardware_concurrency() + 4;
	printf("number of threads: %d\n", NUMBER_OF_THREADS);
	if (NUMBER_OF_THREADS < 1) {
		printf("Couldn't detect the number of cores (default: 12)\n");
		NUMBER_OF_THREADS = 12;
	}

	//Detect AVX support, from: https://stackoverflow.com/questions/6121792/how-to-check-if-a-cpu-supports-the-sse3-instruction-set
	int cpuInfo[4];
	__cpuid(cpuInfo, 1);
	bool osUsesXSAVE_XRSTORE = cpuInfo[2] & (1 << 27) || false;
	bool cpuAVXSuport = cpuInfo[2] & (1 << 28) || false;
	if (osUsesXSAVE_XRSTORE && cpuAVXSuport) {
		unsigned long long xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
		using_avx = (xcrFeatureMask & 0x6) == 0x6;
	}
	cout << "using AVX: " << (using_avx ? "Yes" : "No") << endl;


	defaultParameters.initialize();
	readParameters(defaultParameters, parameterfile);
	{
		int& o = override_oversampling, w = override_width, h = override_height;
		if (o != -1 || w != -1 || h != -1)
			defaultParameters.resize(
				(o != -1 ? o : defaultParameters.get_oversampling())
				,(w != -1 ? w : defaultParameters.get_screenWidth())
				,(h != -1 ? h : defaultParameters.get_screenHeight())
			);
	}

	//defaultParameters.setGradientOffset(defaultParameters.get_gradientOffset() + 0.00001); //doesn't fix the problem
	canvas = *new FractalCanvas(defaultParameters, NUMBER_OF_THREADS);
	{
		double offset = canvas.S.get_gradientOffset();
		canvas.S.setGradientOffset(0);
		canvas.S.setGradientOffset(offset); //workaround for a problem. There is a problem with setting the global variable NUMBER_OF_COLORS in the constructor of FractalCanvas. The parameters in the FractalParameters instance related to the gradient need to be recalculated.
	}

	if (render_image) {
		cout << "rendering image" << endl;
		canvas.createNewRender(true);
		saveImage(write_directory + parameterfile + ".bmp");
	}
	if (render_animation) {
		cout << "rendering animation" << endl;
		animation();
	}
	if (!interactive) {
		return 0;
	}
	

	HWND hWnd;
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	//wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION); /* Load a standard icon */
	wcex.hIcon = LoadIcon(NULL, MAKEINTRESOURCE(1100)); /* Load a standard icon */
	//wcex.hIcon = (HICON)LoadImageA(hInst, "R.ico", IMAGE_ICON, 152, 152, LR_LOADFROMFILE);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION); /* use the name "A" to use the project icon */
	if (!RegisterClassEx(&wcex)) {
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed!"),
			_T("Message"),
			NULL);
		return 1;
	}
	timerstart
	hWnd = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		20, 70,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	timerend(createhWnd) //this takes 0.43 seconds //not anymore now and I have no idea why
	if (!hWnd) {
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Message"),
			NULL);
		return 1;
	}
	hWndMain = hWnd;
	//Register options window
	WNDCLASSEX optionsEx;
	optionsEx.cbSize = sizeof(WNDCLASSEX);
	optionsEx.style = CS_HREDRAW | CS_VREDRAW;
	optionsEx.lpfnWndProc = OptionsProc;
	optionsEx.cbClsExtra = 0;
	optionsEx.cbWndExtra = 0;
	optionsEx.hInstance = hInstance;
	optionsEx.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Load a standard icon
	optionsEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	optionsEx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	optionsEx.lpszMenuName = NULL;
	optionsEx.lpszClassName = CLASS_OPTIONS;
	optionsEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION); // use the name "A" to use the project icon
	if (!RegisterClassEx(&optionsEx)) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed for optionsEx!"),
			_T("Message"),
			NULL);
		return 1;
	}
	//end register options window
	
	//Register JSON window
	WNDCLASSEX JsonEx;
	JsonEx.cbSize = sizeof(WNDCLASSEX);
	JsonEx.style = CS_HREDRAW | CS_VREDRAW;
	JsonEx.lpfnWndProc = JsonProc;
	JsonEx.cbClsExtra = 0;
	JsonEx.cbWndExtra = 0;
	JsonEx.hInstance = hInstance;
	JsonEx.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Load a standard icon
	JsonEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	JsonEx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	JsonEx.lpszMenuName = NULL;
	JsonEx.lpszClassName = CLASS_JSON;
	JsonEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION); // use the name "A" to use the project icon
	if (!RegisterClassEx(&JsonEx)) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed for JsonEx!"),
			_T("Message"),
			NULL);
		return 1;
	}
	//end register JSON window

	timerstart
	hInst = hInstance;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	timerend(showWindows)
	

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
