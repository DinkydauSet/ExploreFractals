
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
#include <iomanip>		//for setfill and setw, to make framenumbers with leading zeros such as frame000001.png
#include <chrono>
#include <thread>
//#include <random>
#include <fstream>
#include <regex>
#include <codecvt>		//to convert utf16 to utf8

//Intrinsics, for using avx instructions
#include <intrin.h>
#include <immintrin.h>

//lodepng
#include "lodepng/lodepng.cpp"

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

//I used this to prevent moire by adding some noise to the coordinates by it's too slow:
/*
std::mt19937_64 generator;
uniform_real_distribution<double> distribution(0.0, 1.0);

double random() {
	return distribution(generator);
}
*/

FractalParameters defaultParameters;
FractalCanvas canvas;
Win32BitmapManager bitmapManager;

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
HWND hWndMain; //main window
HWND statusBar; //status bar of the main window
HWND hOptions; //options window
HWND hJSON; //JSON window

void setWindowSize(int,int);

static TCHAR TITLE_OPTIONS[] = _T("Options");
static TCHAR CLASS_OPTIONS[] = _T("Options Window");

static TCHAR TITLE_JSON[] = _T("JSON");
static TCHAR CLASS_JSON[] = _T("JSON Window");

HWND createOptionsWindow(HINSTANCE hInst) {
	HWND hOptions = CreateWindow(CLASS_OPTIONS, TITLE_OPTIONS, WS_OVERLAPPEDWINDOW, 0, 0, 610, 340, NULL, NULL, hInst, NULL);
	if (!hOptions) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to CreateWindow failed for hOptions!"),
			_T("Message"),
			NULL);
	}
	else {
		ShowWindow(hOptions, SW_SHOW);
	}
	return hOptions;
}

HWND createJsonWindow(HINSTANCE hInst) {
	HWND hJSON = CreateWindow(CLASS_JSON, TITLE_JSON, WS_OVERLAPPEDWINDOW, 0, 0, 420, 680, NULL, NULL, hInst, NULL);
	if (!hJSON) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to CreateWindow failed for hJson!"),
			_T("Message"),
			NULL);
	}
	else {
		ShowWindow(hJSON, SW_SHOW);
	}
	return hJSON;
}


namespace MenuOption {
	enum {
		QUIT
		,RESET
		,TOGGLE_JULIA
		,VIEW_GUESSED_PIXELS
		,VIEW_REGULAR_COLORS
		,WINDOW_OPTIONS
		,HIGH_POWER
		,CHANGE_TRANSFORMATION
		,SAVE_IMAGE
		,SAVE_PARAMETERS
		,LOAD_PARAMETERS
		,CANCEL_RENDER
		,WINDOW_JSON
		,SAVE_BOTH
		,ROTATION_LEFT
		,ROTATION_RIGHT
		//Options to change the fractal type:
		,BURNING_SHIP
		,M2
		,M3
		,M4
		,M5
		,CHECKERS
		,TRIPLE_MATCHMAKER
		,RECURSIVE_FRACTAL
		,PURE_MORPHINGS
		//BI options
		,BI_SHOW
		,DEPARTMENT_SALES
		,DEPARTMENT_IT
		,DEPARTMENT_MT
		,DEPARTMENT_RND
	};
}

void AddMenus(HWND hwnd, bool include_BI) {
	HMENU hMenubar = CreateMenu();
	HMENU hMenuOther = CreateMenu();
	HMENU hMenuFile = CreateMenu();

	//hMenuOther, item type, message (number), button text
	AppendMenuA(hMenuFile, MF_STRING, MenuOption::SAVE_IMAGE, "Save &image");
	AppendMenuA(hMenuFile, MF_STRING, MenuOption::SAVE_PARAMETERS, "Save &parameters");
	AppendMenuA(hMenuFile, MF_STRING, MenuOption::SAVE_BOTH, "&Save both");
	AppendMenuA(hMenuFile, MF_STRING, MenuOption::LOAD_PARAMETERS, "&Load parameters");

	AppendMenuA(hMenuOther, MF_STRING, MenuOption::CANCEL_RENDER, "&Cancel Render");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::CHANGE_TRANSFORMATION, "Change &transformation");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::VIEW_GUESSED_PIXELS, "&View guessed pixels");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::VIEW_REGULAR_COLORS, "View &Regular colors");
	AppendMenuA(hMenuOther, MF_SEPARATOR, 0, NULL);
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::BURNING_SHIP, "&Burning Ship");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::M4, "Mandelbrot power 4");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::M5, "Mandelbrot power 5");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::HIGH_POWER, "High Power Mandelbrot");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::TRIPLE_MATCHMAKER, "Triple Matchmaker");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::RECURSIVE_FRACTAL, "Recursive fractal test");
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::PURE_MORPHINGS, "Pure Julia morphings");
	AppendMenuA(hMenuOther, MF_SEPARATOR, 0, NULL);
	AppendMenuA(hMenuOther, MF_STRING, MenuOption::QUIT, "&Quit");

	AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuFile, "&File");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::WINDOW_OPTIONS, "&Options");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::WINDOW_JSON, "&JSON");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::RESET, "Reset");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::TOGGLE_JULIA, "&Toggle Julia");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::M2, "&Mandelbrot");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::M3, "Mandelbrot power 3");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::CHECKERS, "&Checkers");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::ROTATION_LEFT, "&Left");
	AppendMenuA(hMenubar, MF_STRING, MenuOption::ROTATION_RIGHT, "&Right");
	AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuOther, "Mor&e");
	if (include_BI) {
		AppendMenuA(hMenubar, MF_STRING, MenuOption::BI_SHOW, "BI");
		AppendMenuA(hMenubar, MF_STRING, MenuOption::DEPARTMENT_IT, "IT");
		AppendMenuA(hMenubar, MF_STRING, MenuOption::DEPARTMENT_MT, "Management Team");
		AppendMenuA(hMenubar, MF_STRING, MenuOption::DEPARTMENT_RND, "Research and Development");
		AppendMenuA(hMenubar, MF_STRING, MenuOption::DEPARTMENT_SALES, "Sales");
	}

	SetMenu(hwnd, hMenubar);
}

void recalculate() {
	cout << "recalculate" << endl;
	canvas.cancelRender();
	thread t(&FractalCanvas::createNewRender, &canvas, false);
	t.detach();
	PostMessageA(hOptions, CUSTOM_REFRESH, 0, 0);
}

enum class ReadResult {
	succes,
	parseError,
	fileError
};

/*
	Stores the values in the json string in S. It first tries to change values in a copy of S so that when parsing fails, S is not left in an inconsistent state.
*/
ReadResult readParametersJson(FractalParameters& S, string& json) {
	FractalParameters newS = S;
	if ( ! newS.fromJson(json)) {
		return ReadResult::parseError;
	}
	S = newS;
	return ReadResult::succes;
}

/*
	Reads JSON from the file and stores the values in S.
*/
ReadResult readParametersFile(FractalParameters& S, string fileName) {
	ReadResult ret;
	{
		ifstream infile;
		infile.open(fileName);
		if (infile.is_open()) {
			stringstream strStream; strStream << infile.rdbuf();
			string json = strStream.str();
			ret = readParametersJson(S, json); //this changes S if succesful
		}
		else {
			ret = ReadResult::fileError;
		}
		infile.close();
	}
	return ret;
}

void handleResizeResultGUI(FractalCanvas::ResizeResult res) {
	if (res.resultType == FractalCanvas::ResizeResultType::OutOfRangeError) {
		MessageBox(NULL,
			_T("Changing the resolution failed: width and/or height out of range."),
			_T("Error"),
			NULL
		);
	}
	else if (res.resultType == FractalCanvas::ResizeResultType::MemoryError) {
		if (res.changed) {
			MessageBox(NULL,
				_T("Changing the resolution failed: out of memory."),
				_T("Error"),
				NULL
			);
		}
		else {
			MessageBox(NULL,
				_T("Changing the resolution failed: out of memory. The resolution has not changed."),
				_T("Error"),
				NULL
			);
		}
	}
	if (res.success) {
		setWindowSize(canvas.S.get_screenWidth(), canvas.S.get_screenHeight());
	}
}

/*
	Wrapper around readParameters-functions for the current GUI
	This shows error messages and changes the window size.
	There is only one FractalCanvas so that one instance is always used.

	If fromFile if true, then input is used as a filename
	else input is used as a JSON string.
*/
void readParametersGUI(string& input, bool fromFile) {
	FractalParameters newS = canvas.S;
	ReadResult res;
	if (fromFile)
		res = readParametersFile(newS, input);
	else
		res = readParametersJson(newS, input);

	switch (res) {
		case ReadResult::succes: {
			FractalCanvas::ResizeResult res = canvas.changeParameters(newS);
			handleResizeResultGUI(res);
			if (res.success) {
				recalculate();
			}
			break;
		}
		case ReadResult::parseError: {
			MessageBox(NULL,
				_T("The parameter file doesn't have the expected format. If this is a binary file (from program version 4 and earlier), try to open and resave it with ExploreFractals 5."),
				_T("Error"),
				NULL
			);
			break;
		}
		case ReadResult::fileError: {
			MessageBox(NULL,
				_T("The parameter file cannot be openened."),
				_T("Error"),
				NULL
			);
			break;
		}
	}
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
			if (canvasContext.lastRenderID == renderID) { //to prevent drawing after the render is already finished, because a drawing is already made immediately after the render finishes
				canvasContext.bitmapManager->draw();
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
		cout << "zoom: " << S.get_zoomLevel() << endl;
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
				bitmapManager->draw();
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
		cout << "iterations per second: " << ((uint64)(R.computedIterations / R.getElapsedTime()) / 1000000.0) << " M" << endl;
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
		case PROCEDURE_RECURSIVE_FRACTAL: {
			createNewRenderTemplated<PROCEDURE_RECURSIVE_FRACTAL, true, false, false>(headless);
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
		case PROCEDURE_BI: {
			createNewRenderTemplated<PROCEDURE_BI, true, false, false>(headless);
			break;
		}
		case PROCEDURE_PURE_MORPHINGS: {
			createNewRenderTemplated<PROCEDURE_PURE_MORPHINGS, true, false, false>(headless);
			break;
		}
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

void saveImage(string filename, bool cleanup = true) {
	uint screenWidth = canvas.S.get_screenWidth();
	uint screenHeight = canvas.S.get_screenHeight();

	/*
		This loop performs conversion.

		PNG requires RGBA-values in big-endian order. The colors in this program are ARGB stored in little-endian order. Lodepng interprets the data correctly when delivered as ABGR (the reserve order of RGBA) because of the endianness difference.
		 I convert the colors to ABGR in the original array to reduce memory usage. That means that after saving the PNG, the colors need to be reverted to their original values.

		 LodePNG also expects the colors to be ordered from bottom to top. In this program the colors are ordered top to bottom. That means the order of the horizontal lines of pixels should be reversed.
	*/
	
	uint upToMiddle = screenHeight / 2;
	if (screenHeight % 2 == 1) upToMiddle += 1;

	for (uint y=0; y < upToMiddle; y++) {
		for (uint x=0; x < screenWidth; x++) {

			uint pixelIndexHigh = canvas.pixelIndex_of_pixelXY(x, y);
			uint pixelIndexLow = canvas.pixelIndex_of_pixelXY(x, screenHeight - y - 1);
			ARGB& high = canvas.ptPixels[pixelIndexHigh];
			ARGB& low = canvas.ptPixels[pixelIndexLow];
		
			ARGB low_copy = low;

			ARGB r,g,b;
			r = (high & 0x00ff0000);
			g = (high & 0x0000ff00);
			b = (high & 0x000000ff);

			//ARGB to ABGR. This comes down to swapping red and blue.
			low = (
				0xff000000 //always use full opacity (no transparency)
				| r >> 16
				| g
				| b << 16
			);

			r = (low_copy & 0x00ff0000);
			g = (low_copy & 0x0000ff00);
			b = (low_copy & 0x000000ff);

			high = (
				0xff000000 //always use full opacity (no transparency)
				| r >> 16
				| g
				| b << 16
			);
		}
	}
	
	uint8* out;
	size_t outsize;
	unsigned errorcode = lodepng_encode32(
		&out, &outsize				//will contain the PNG data
		,(uint8*)canvas.ptPixels		//image data to encode
		,screenWidth, screenHeight	//width and height
	);
	if (errorcode != 0) {
		cout << "error " << errorcode << " occurred while encoding PNG" << endl;
		assert(false);
	}
	else {
		ofstream outfile;
		outfile.open(filename, ios::binary);
		if (outfile.is_open()) {
			outfile.write((char*)out, outsize);
			outfile.close();
		}
		else {
			cout << "error while opening file " << filename << endl;
			assert(false);
		}
	}
	free(out);

	if (cleanup) {
		canvas.createNewBitmapRender(true, false);
	}
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
int skipframes = 0;
bool save_as_efp = false;



void animation(
	string path
	,bool save_only_parameters
	,int skipframes
	,int framesPerInflection
	,int framesPerZoom
	,FractalParameters& S
) {
	cout << "rendering animation with" << endl;
	cout << fps << " fps" << endl;
	cout << framesPerInflection << " frames per inflection" << endl;
	cout << framesPerZoom << " frames per zoom" << endl;

	auto makeFrame = [&](int frame, bool recalculate) {
		if (frame <= skipframes) {
			cout << "skipping frame " << frame << endl;
			return;
		}

		std::stringstream num;
		num << std::setfill('0') << std::setw(6); //numbering 000001, 000002, 000003, ...
		num << frame;

		if (save_only_parameters) {
			string filename = "frame" + num.str() + ".efp";
			writeParameters(canvas.S, write_directory + filename);
			return;
		}

		if (recalculate)
			canvas.createNewRender(true); //the whole render takes place in this thread so after this the render is done
		else
			canvas.createNewBitmapRender(true, false);

		string filename = "frame" + num.str() + ".png";
		cout << "saving image " << filename << endl;
		saveImage(path + filename, false);
	};


	S.pre_transformation_type = 7; //broken inflection power transformation
	vector<double_c> inflections = S.get_inflectionCoords();
	int inflectionCount = S.get_inflectionCount();
	while(S.removeInflection()); //reset to 0 inflections
	double_c originalCenter = S.get_center();
	S.setCenterAndZoomAbsolute(0, 0); //start the animation unzoomed and centered
	S.partialInflectionPower = 1;
	S.partialInflectionCoord = 0;

	double inflectionPowerStepsize = 1.0 / (framesPerInflection - 1);
	int frame = 1;

	makeFrame(frame++, true);		

	double_c centerTarget = inflectionCount > 0 ? inflections[0] : originalCenter;
	double_c currentCenter = S.get_center();
	double_c diff = centerTarget - currentCenter;

	//gradually move the center towards the next inflection location
	if (centerTarget != 0.0 + 0.0*I) {
		for (int i=1; i<=framesPerInflection; i++) {

			S.setCenter( currentCenter + diff * ((1.0 / framesPerInflection) * i) );

			makeFrame(frame++, true);
		}
	}
	S.setCenter(centerTarget);

	double currentZoom = S.get_zoomLevel();
	double targetZoom = S.get_inflectionZoomLevel() * (1 / pow(2, S.get_inflectionCount()));
	double zoomDiff = targetZoom - currentZoom;
	double zoomStepsize = 1.0 / framesPerZoom;

	//zoom to the inflection zoom level
	if (zoomStepsize > 0.001) {
		for (int i=1; i <= framesPerZoom * zoomDiff; i++) {

			S.setZoomLevel( currentZoom + zoomStepsize * i );

			makeFrame(frame++, true);
		}
	}

	for (int inflection=0; inflection < inflectionCount; inflection++) {

		S.partialInflectionPower = 1;
		S.partialInflectionCoord = 0;

		double_c thisInflectionCoord = inflections[inflection];
		
		double_c currentCenter = S.get_center();
		double_c diff = thisInflectionCoord - currentCenter;

		S.partialInflectionCoord = thisInflectionCoord;

		//gradually apply the next inflection by letting the inflection power go from 1 to 2, and meanwhile also gradually move to the next inflection location.
		for (int i=0; i<framesPerInflection; i++) {
			S.setCenterAndZoomAbsolute(0, (
				S.get_inflectionZoomLevel()
				* (1 / pow(2, S.get_inflectionCount()))
				* (1 / S.partialInflectionPower)
			));
			S.setCenter( currentCenter + diff * ((1.0 / (framesPerInflection-1)) * i) );

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

//old animation style:
/*
void animation2() {
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
	S.setCenterAndZoomAbsolute(0, 0); //start the animation unzoomed and centered
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

				S.setCenter( currentCenter + diff * ((1.0 / framesPerInflection) * i) );

				makeFrame(frame++, true);
			}
		}

		double currentZoom = S.get_zoomLevel();
		double targetZoom = S.get_inflectionZoomLevel() * (1 / pow(2, S.get_inflectionCount()));
		double zoomDiff = targetZoom - currentZoom;
		double zoomStepsize = 1.0 / framesPerZoom;

		//and zoom to the inflection zoom level
		if (zoomStepsize > 0.001) {
			for (int i=1; i <= framesPerZoom * zoomDiff; i++) {

				S.setZoomLevel( currentZoom + zoomStepsize * i );

				makeFrame(frame++, true);
			}
		}

		S.partialInflectionCoord = thisInflectionCoord;

		//gradually apply the next inflection by letting the inflection power go from 1 to 2.
		for (int i=0; i<framesPerInflection; i++) {
			S.setCenterAndZoomAbsolute(0, (
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
*/

//identifiers start at 300:
const int mainWndStatusBar = 300;

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	
	case WM_CREATE: {
		hWndMain = hWnd;
		AddMenus(hWnd, false);
		statusBar = CreateWindowExW(0, STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)mainWndStatusBar, hInst, NULL);
		int widths[4] = { 80, 180, 380, -1 };
		SendMessage(statusBar, SB_SETPARTS, 4, (LPARAM)&widths);

		setWindowSize(canvas.S.get_screenWidth(), canvas.S.get_screenHeight());

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
		int menuOption = LOWORD(wParam);
		int formula_identifier;

		switch (menuOption) {
			//Menu options that change the fractal type:
			case MenuOption::BURNING_SHIP: { formula_identifier = PROCEDURE_BURNING_SHIP; break; }
			case MenuOption::CHECKERS: { formula_identifier = PROCEDURE_CHECKERS; break; }
			case MenuOption::M2: { formula_identifier = PROCEDURE_M2; break; }
			case MenuOption::M3: { formula_identifier = PROCEDURE_M3; break; }
			case MenuOption::M4: { formula_identifier = PROCEDURE_M4; break; }
			case MenuOption::M5: { formula_identifier = PROCEDURE_M5; break; }
			case MenuOption::TRIPLE_MATCHMAKER: { formula_identifier = PROCEDURE_TRIPLE_MATCHMAKER; break; }
			case MenuOption::RECURSIVE_FRACTAL:  { formula_identifier = PROCEDURE_RECURSIVE_FRACTAL; break; }
			case MenuOption::HIGH_POWER:  { formula_identifier = PROCEDURE_HIGH_POWER; break; }
			case MenuOption::BI_SHOW: { formula_identifier = PROCEDURE_BI; break; }
			case MenuOption::PURE_MORPHINGS: { formula_identifier = PROCEDURE_PURE_MORPHINGS; break; }
			default:
				formula_identifier = -1;
		}

		if (formula_identifier != -1) {
			bool fractalTypeChange = canvas.S.changeFormula(formula_identifier);
			if (formula_identifier == PROCEDURE_BI) {
				string fixedParams = R"(
				{
					"programVersion": 6.2,
					"oversampling": 1,
					"screenWidth": 800,
					"screenHeight": 800,
					"rotation": 0.0,
					"center": {
						"Re": 0.5501794819945975,
						"Im": 0.5487185177196201
					},
					"zoomLevel": 1.6,
					"maxIters": 4600,
					"juliaSeed": {
						"Re": -0.75,
						"Im": 0.1
					},
					"julia": false,
					"formula_identifier": 16,
					"post_transformation_type": 0,
					"pre_transformation_type": 0,
					"inflectionCount": 0,
					"inflectionCoords": [],
					"gradientColors": [
						{
							"r": 255,
							"g": 255,
							"b": 255
						},
						{
							"r": 10,
							"g": 10,
							"b": 10
						},
						{
							"r": 200,
							"g": 20,
							"b": 20
						}
					],
					"gradientSpeed": 1.0,
					"gradientOffset": 0.0,
					"inflectionZoomLevel": 1.6
				}
				)";
				readParametersGUI(fixedParams, false);
				recalculate();
			}
			else if (fractalTypeChange) {
				canvas.S.setCenterAndZoomAbsolute(0, 0);
				recalculate();
			}
			break;
		}

		switch (menuOption) {
			//Other options:
			case MenuOption::RESET: {
				canvas.S.reset(defaultParameters);
				recalculate();
				break;
			}
			case MenuOption::TOGGLE_JULIA: {
				canvas.S.toggleJulia();
				recalculate();
				break;
			}
			case MenuOption::CHANGE_TRANSFORMATION: {
				canvas.S.changeTransformation();
				canvas.S.setCenterAndZoomAbsolute(0, 0);
				recalculate();
				break;
			}
			case MenuOption::WINDOW_OPTIONS: {
				DestroyWindow(hOptions);
				hOptions = createOptionsWindow(hInst);
				break;
			}
			case MenuOption::WINDOW_JSON: {
				hJSON = createJsonWindow(hInst);
				break;
			}
			case MenuOption::VIEW_GUESSED_PIXELS: {
				refreshBitmapThread(true);
				break;
			}
			case MenuOption::VIEW_REGULAR_COLORS: {
				refreshBitmapThread(false);
				break;
			}
			case MenuOption::SAVE_IMAGE: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, FALSE, "Save PNG", "Portable Network Graphics (PNG)\0*.png\0\0", filename)) {
					saveImage(filename);
				}
				break;
			}
			case MenuOption::SAVE_PARAMETERS: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, FALSE, "Save parameters", "Parameters\0*.efp\0\0", filename)) {
					writeParameters(canvas.S, filename);
				}
				break;
			}
			case MenuOption::SAVE_BOTH: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, FALSE, "Save parameters and image", "Parameters\0*.efp\0\0", filename)) {
					writeParameters(canvas.S, filename);
					saveImage(filename + ".png");
				}
				break;
			}
			case MenuOption::LOAD_PARAMETERS: {
				string filename = getDate() + " " + canvas.S.get_formula().name;
				if (BrowseFile(hWnd, TRUE, "Load parameters", "Parameters\0*.efp\0\0", filename)) {
					readParametersGUI(filename, true);
				}
				break;
			}
			case MenuOption::CANCEL_RENDER: {
				canvas.cancelRender();
				break;
			}
			case MenuOption::QUIT: {
				PostQuitMessage(0);
				break;
			}
			case MenuOption::DEPARTMENT_IT: {
				canvas.S.BI_choice = BI_choices::DEPARTMENT_IT;
				recalculate();
				break;
			}
			case MenuOption::DEPARTMENT_MT: {
				canvas.S.BI_choice = BI_choices::DEPARTMENT_MT;
				recalculate();
				break;
			}
			case MenuOption::DEPARTMENT_RND: {
				canvas.S.BI_choice = BI_choices::DEPARTMENT_RND;
				recalculate();
				break;
			}
			case MenuOption::DEPARTMENT_SALES: {
				canvas.S.BI_choice = BI_choices::DEPARTMENT_SALES;
				recalculate();
				break;
			}
			case MenuOption::ROTATION_LEFT: {
				canvas.S.setRotation(canvas.S.get_rotation_angle() - 0.05);
				recalculate();
				break;
			}
			case MenuOption::ROTATION_RIGHT: {
				canvas.S.setRotation(canvas.S.get_rotation_angle() + 0.05);
				recalculate();
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
		DrawBitmap(hdc, 0, 0, bitmapManager.screenBMP, SRCCOPY, canvas.S.get_screenWidth(), canvas.S.get_screenHeight());
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
		if (xPos < 0 || xPos >= width || yPos < 0 || yPos >= height) {
			return 0;
		}

		IterData thisPixel = canvas.getIterData(xPos, yPos);
		double_c thisComplexNumber = canvas.S.map_with_transformations(xPos, yPos);
		string complexNumber = to_string(real(thisComplexNumber)) + " + " + to_string(imag(thisComplexNumber)) + "* I";
		string iterationData = "iters: " + to_string(thisPixel.iterationCount());
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

		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam); //whether the zoom is in or out

		double zooms = zDelta > 0 ? 2 : -2; //these 2 and -2 are the zoom sizes used for zooming in and out. They could be any number so it could be a setting.
		double magnificationFactor = pow(2, -zooms);

		double_c zoomLocation = canvas.map(xPos * oversampling, yPos * oversampling);

		//This is the difference between the location that is being zoomed in on (the location of the mouse cursor) and the right and top borders of the viewport, expressed as a fraction.
		double margin_right = xPos / (double)screenWidth;
		double margin_top = yPos / (double)screenHeight;

		double margin_right_size = width * margin_right * canvas.S.get_pixelWidth();
		double margin_top_size = height * margin_top * canvas.S.get_pixelHeight();

		double margin_right_new_size = margin_right_size * magnificationFactor;
		double margin_top_new_size = margin_top_size * magnificationFactor;

		double_c new_topleftcorner =
			real(zoomLocation) - margin_right_new_size
			+ (imag(zoomLocation) + margin_top_new_size) * I;

		double_c new_center =
			real(new_topleftcorner) + (margin_right_new_size / margin_right) * 0.5
			+ (imag(new_topleftcorner) - (margin_top_new_size / margin_top) * 0.5) * I;

		canvas.S.setCenterAndZoomRelative(new_center, canvas.S.get_zoomLevel() + zooms);

		//timerstart
		//generate preview bitmap
		BITMAP Bitmap;
		HDC screenHDC = CreateCompatibleDC(NULL);
		GetObject(bitmapManager.screenBMP, sizeof(BITMAP), (LPSTR)&Bitmap);
		SelectObject(screenHDC, bitmapManager.screenBMP);

		if (zDelta > 0) {
			StretchBlt(screenHDC, 0, 0, screenWidth, screenHeight, screenHDC, xPos - xPos / 4, yPos - yPos / 4, screenWidth / 4, screenHeight / 4, SRCCOPY); //stretch the screen bitmap
		}
		else {
			SetStretchBltMode(screenHDC, HALFTONE);
			StretchBlt(screenHDC, xPos - xPos / 4, yPos - yPos / 4, screenWidth / 4, screenHeight / 4, screenHDC, 0, 0, screenWidth, screenHeight, SRCCOPY);
		}
		DeleteDC(screenHDC);
		//timerend(resize_bitmap)

		recalculate();
		break;
	}
	case WM_LBUTTONUP: {
		//create inflection
		uint oversampling = canvas.S.get_oversampling();
		uint xPos = GET_X_LPARAM(lParam) * oversampling;
		uint yPos = GET_Y_LPARAM(lParam) * oversampling;

		if ( ! (xPos < 0 || xPos > canvas.S.get_width() || yPos < 0 || yPos > canvas.S.get_height())) {
			canvas.S.addInflection(xPos, yPos);
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

		if ( ! (xPos < 0 || xPos > canvas.S.get_width() || yPos < 0 || yPos > canvas.S.get_height())) {
			canvas.S.removeInflection();
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
		optionsElt(optionGradientSpeedTrackbarCoarse) = CreateTrackbar(hOptions, -50, 100, 60, 85, 400, 30, optionGradientSpeedTrackbarCoarse, hInst);
		optionsElt(optionGradientSpeedTextFine) = CreateWindowExA(0, "STATIC", "Fine", WS_VISIBLE | WS_CHILD, 0, 115, 110, 30, hOptions, (HMENU)optionGradientSpeedTextFine, hInst, NULL);
		optionsElt(optionGradientSpeedTrackbarFine) = CreateTrackbar(hOptions, 0, 99, 60, 115, 400, 30, optionGradientSpeedTrackbarFine, hInst);
		optionsElt(optionGradientOffsetText) = CreateWindowExA(0, "STATIC", "Offset", WS_VISIBLE | WS_CHILD, 0, 145, 130, 30, hOptions, (HMENU)optionGradientOffsetText, hInst, NULL);
		optionsElt(optionGradientOffsetTrackbar) = CreateTrackbar(hOptions, 0, 100, 60, 145, 400, 30, optionGradientOffsetTrackbar, hInst);
		double gradientSpeed = canvas.S.get_gradientSpeed();
		SendMessage(optionsElt(optionGradientSpeedTrackbarCoarse), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)((int)gradientSpeed));
		if ((int)gradientSpeed >= 0)
			SendMessage(optionsElt(optionGradientSpeedTrackbarFine), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(100 * (gradientSpeed - (int)gradientSpeed)));
		else
			SendMessage(optionsElt(optionGradientSpeedTrackbarFine), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(-100 * (gradientSpeed - (int)gradientSpeed)));
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

		SendMessage(optionsElt(getOverSampleIdentifier()), BM_SETCHECK, BST_CHECKED, 0);

		//set gradient speed
		SetDlgItemTextA(hOptions, optionGradientSpeed, to_string(canvas.S.get_gradientSpeed()).c_str());

		//set width, height
		SetDlgItemTextA(hOptions, optionHeight, to_string(canvas.S.get_screenHeight()).c_str());
		SetDlgItemTextA(hOptions, optionWidth, to_string(canvas.S.get_screenWidth()).c_str());

		//Set zoom level
		SetDlgItemTextA(hOptions, optionZoomLevel, to_string(canvas.S.get_zoomLevel()).c_str());

		//Set max iters
		SetDlgItemTextA(hOptions, optionMaxIters, to_string(canvas.S.get_maxIters()).c_str());
		break;
	}
	case WM_HSCROLL: {
		if(debug) cout << "WM_HSCROLL " << lParam << endl;
		bool refreshBitmapRequired = false;
		if (lParam == (LPARAM)optionsElt(optionGradientSpeedTrackbarCoarse)) {
			int posCoarse = SendMessage(optionsElt(optionGradientSpeedTrackbarCoarse), TBM_GETPOS, 0, 0);
			double currentSpeed = canvas.S.get_gradientSpeed();
			double newSpeed = posCoarse + (currentSpeed - (int)currentSpeed);
			refreshBitmapRequired = canvas.S.setGradientSpeed(newSpeed);
		}
		else if (lParam == (LPARAM)optionsElt(optionGradientSpeedTrackbarFine)) {
			int posFine = SendMessage(optionsElt(optionGradientSpeedTrackbarFine), TBM_GETPOS, 0, 0);
			double currentSpeed = canvas.S.get_gradientSpeed();
			double newSpeed;
			if ((int)(currentSpeed) < 0)
				newSpeed = (int)(currentSpeed - 1) + (1 - 0.01*(double)posFine);
			else
				newSpeed = (int)(currentSpeed) + 0.01*(double)posFine;
			refreshBitmapRequired = canvas.S.setGradientSpeed(newSpeed);
		}
		else if (lParam == (LPARAM)optionsElt(optionGradientOffsetTrackbar)) {
			double newGradientOffset = 0.01 * SendMessage(optionsElt(optionGradientOffsetTrackbar), TBM_GETPOS, 0, 0);
			cout << "new gradientOffset: " << newGradientOffset << endl;
			refreshBitmapRequired = canvas.S.setGradientOffset(newGradientOffset);

		}
		if (refreshBitmapRequired)
			refreshBitmapThread(false);
		SendMessage(hOptions, CUSTOM_REFRESH, 0, 0);
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
			if (newMaxIters == 0) {
				AddMenus(hWndMain, true);
			}
			recalcNeeded |= canvas.S.setMaxIters(newMaxIters);

			//change gradient speed:
			char aGradientSpeed[16]; GetDlgItemTextA(hOptions, optionGradientSpeed, aGradientSpeed, sizeof(aGradientSpeed));
			double newGradientSpeed = strtod(aGradientSpeed, NULL);
			bool refreshNeeded = canvas.S.setGradientSpeed(newGradientSpeed);

			//change zoom level:
			char aZoomLevel[16]; GetDlgItemTextA(hOptions, optionZoomLevel, aZoomLevel, sizeof(aZoomLevel));
			double newZoomLevel = strtod(aZoomLevel, NULL);
			recalcNeeded |= canvas.S.setCenterAndZoomRelative(canvas.S.get_center(), newZoomLevel);

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

			FractalCanvas::ResizeResult res = canvas.resize(newOversampling, newScreenWidth, newScreenHeight);
			handleResizeResultGUI(res);
			recalcNeeded |= res.changed;

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
			readParametersGUI(json, false);
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
		else if (c == "--skipframes" ) {
			if (i+1 < argc) {
				skipframes = stoi(commands[i+1]);
			}
		}
		else if (c == "--efp") {
			save_as_efp = true;
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
	-o directory    use the directory as output directory (example: C:\folder)
    --width         override the width parameter
    --height        override the height parameter
    --oversampling  override the oversampling parameter
    --image         render the initial parameter file to an image
    --animation     render an animation of the initial parameters
	--efp           save the parameters instead of rendering to an image (can be used to convert old parameter files or to store parameter files for every frame in an animation)
    --fps number    the number of frames per second (integer)
    --spi number    the number of seconds per inflection (floating point)
    --spz number    the number of seconds per zoom (floating point)
    --skipframes    number of frames to skip (for example to continue an unfinished animation render)
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
		uint64 xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
		using_avx = (xcrFeatureMask & 0x6) == 0x6;
	}
	cout << "using AVX: " << (using_avx ? "Yes" : "No") << endl;


	readParametersFile(defaultParameters, parameterfile);
	{
		int& o = override_oversampling, w = override_width, h = override_height;
		if (o != -1 || w != -1 || h != -1)
			defaultParameters.resize(
				(o != -1 ? o : defaultParameters.get_oversampling())
				,(w != -1 ? w : defaultParameters.get_screenWidth())
				,(h != -1 ? h : defaultParameters.get_screenHeight())
			);
	}

	//Here the only bitmapManager and canvas for the window are created.
	bitmapManager = *new Win32BitmapManager(&hWndMain);
	canvas = *new FractalCanvas(defaultParameters, NUMBER_OF_THREADS, bitmapManager);


	if (save_as_efp && ! render_animation) {
		writeParameters(canvas.S, write_directory + parameterfile);
	}
	else if (render_image) {
		cout << "rendering image" << endl;
		canvas.createNewRender(true);
		saveImage(write_directory + parameterfile + ".png");
	}

	if (render_animation) {
		cout << "rendering animation" << endl;
		int framesPerInflection = (int)(fps * secondsPerInflection);
		int framesPerZoom = (int)(fps * secondsPerZoom);
		animation(write_directory, save_as_efp, skipframes, framesPerInflection, framesPerZoom, canvas.S);
	}

	if (!interactive) {
		return 0;
	}

	
	static TCHAR szWindowClass[] = _T("win32app"); // The main window class name.  
	static TCHAR szTitle[] = _T("ExploreFractals"); // The string that appears in the application's title bar.

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
