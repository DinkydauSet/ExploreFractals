//?
#include "stdafx.h"

//WinApi
#include <Windowsx.h>
#include <windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <ocidl.h>
#include <olectl.h>
#include <atlbase.h>

//C++ standard library
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <tchar.h>
#include <complex>
#include <math.h>
#include <time.h>
#include <chrono>
#include <thread>
#include <random>
#include <string>
#include <vector>
#include <cassert>
#include <fstream>
#include <mutex>
#include <regex>

//Intrinsics
#include <intrin.h>
#include <immintrin.h>

//rapidjson
#include "rapidjson/document.h"     // rapidjson's DOM-style API
#include "rapidjson/prettywriter.h" // for stringify JSON

/*
#include <gmp.h>
#include <gmpxx.h>
*/

//Link to libraries
//#pragma comment(lib,"comctl32.lib") //is this important? for InitCommonControls();
//#pragma comment(lib,"C:/msys64/mingw64/lib/libgmp.dll.a")

//Macro
#define RGB2(r,g,b) RGB(b,g,r) //once placed in a bitmap, colors are displayed in reverse order for some reason.
#define CUSTOM_REFRESH 8000 //custom message

using namespace std;
using namespace rapidjson;

typedef std::complex<double> double_c;
const double_c I(0, 1);

std::mt19937_64 generator;
uniform_real_distribution<double> distribution(0.0, 1.0);

double random() {
	return distribution(generator);
}

mutex threadCountChange;
mutex renders;
mutex drawingBitmap;

//Global variables that should not be user influenceable
const double programVersion = 5.0;
const int MAX_NUMBER_OF_INFLECTIONS = 500;
const int MAXRES_WIDTH = 20000;
const int MAXRES_HEIGHT = 20000;
unsigned NUMBER_OF_THREADS;
bool USING_AVX = false;
bool firstPaint = true;
const int NUMBER_OF_COLORS = 4; //must be power of 2 to use "AND" to calculate the color array index
const int NUMBER_OF_TRANSFORMATIONS = 6 + 1;

HINSTANCE hInst; //application instance
HWND hWndMain; //main window
HWND statusBar; //status bar of the main window
HWND hOptions; //options window
HWND hJSON; //JSON window

static TCHAR szWindowClass[] = _T("win32app"); // The main window class name.  
static TCHAR szTitle[] = _T("Explore fractals"); // The string that appears in the application's title bar.  

static TCHAR TITLE_OPTIONS[] = _T("Options");
static TCHAR CLASS_OPTIONS[] = _T("Options Window");

static TCHAR TITLE_JSON[] = _T("JSON");
static TCHAR CLASS_JSON[] = _T("JSON Window");

// Forward declarations of window procedure functions
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK OptionsProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK JsonProc(HWND, UINT, WPARAM, LPARAM);

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR	 lpCmdLine,
	_In_ int	   nCmdShow
)
{
	setbuf(stdout, NULL);

	/*
	INITCOMMONCONTROLSEX icce;
	icce.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icce.dwICC = ICC_WIN95_CLASSES;

	//InitCommonControlsEx(&icce);
	InitCommonControls();
	*/

	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
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
			_T("Win32 Guided Tour"),
			NULL);
		return 1;
	}
	HWND hWnd = CreateWindow(
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
	if (!hWnd) {
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Win32 Guided Tour"),
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
	optionsEx.hIcon = LoadIcon(NULL, IDI_APPLICATION); /* Load a standard icon */
	optionsEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	optionsEx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	optionsEx.lpszMenuName = NULL;
	optionsEx.lpszClassName = CLASS_OPTIONS;
	optionsEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION); /* use the name "A" to use the project icon */
	if (!RegisterClassEx(&optionsEx)) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed for optionsEx!"),
			_T("Win32 Guided Tour"),
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
	JsonEx.hIcon = LoadIcon(NULL, IDI_APPLICATION); /* Load a standard icon */
	JsonEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	JsonEx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	JsonEx.lpszMenuName = NULL;
	JsonEx.lpszClassName = CLASS_JSON;
	JsonEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION); /* use the name "A" to use the project icon */
	if (!RegisterClassEx(&JsonEx)) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed for JsonEx!"),
			_T("Win32 Guided Tour"),
			NULL);
		return 1;
	}
	//end register JSON window

	hInst = hInstance;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

int createOptionsWindow() {
	hOptions = CreateWindow(CLASS_OPTIONS, TITLE_OPTIONS, WS_OVERLAPPEDWINDOW, 0, 0, 610, 340, NULL, NULL, hInst, NULL);
	if (!hOptions) {
		cout << "error: " << GetLastError() << endl;
		MessageBox(NULL,
			_T("Call to CreateWindow failed for hOptions!"),
			_T("Win32 Guided Tour"),
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
			_T("Win32 Guided Tour"),
			NULL);
		return 1;
	}
	ShowWindow(hJSON, SW_SHOW);
	return 0;
}

HWND WINAPI CreateTrackbar(
	HWND hwndDlg,  // handle of dialog box (parent window) 
	UINT selMin,     // minimum value in trackbar range 
	UINT selMax,     // maximum value in trackbar range
	int xPos, int yPos,
	int hSize, int vSize,
	int identifier)
{
	HWND hwndTrack = CreateWindowEx(
		0,                               // no extended styles 
		TRACKBAR_CLASS,                  // class name 
		_T("Trackbar Control"),              // title (caption) 
		WS_CHILD |
		WS_VISIBLE |
		TBS_AUTOTICKS |
		TBS_ENABLESELRANGE,              // style 
		xPos, yPos,                          // position 
		hSize, vSize,                         // size 
		hwndDlg,                         // parent window 
		(HMENU)identifier,                     // control identifier 
		hInst,                         // instance 
		NULL                             // no WM_CREATE parameter 
	);
	SendMessage(hwndTrack, TBM_SETRANGE,
		(WPARAM)TRUE,                   // redraw flag 
		(LPARAM)MAKELONG(selMin, selMax));  // min. & max. positions
	SendMessage(hwndTrack, TBM_SETPAGESIZE,
		0, (LPARAM)4);                  // new page size 
	SendMessage(hwndTrack, TBM_SETPOS,
		(WPARAM)TRUE,                   // redraw flag 
		(LPARAM)selMin);
	SetFocus(hwndTrack);
	return hwndTrack;
}

int BrowseFile(HWND hwParent, BOOL bOpen, const char *szTitle, const char *szExt, std::string &szFile) {
	char buffer[1024] = { 0 };
	strncpy(buffer, szFile.c_str(), sizeof(buffer));
	buffer[sizeof(buffer) - 1] = 0;
	OPENFILENAMEA ofn = { sizeof(OPENFILENAME) };
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.lpstrFile = buffer;
	ofn.lpstrTitle = szTitle;
	ofn.nMaxFile = sizeof(buffer);
	ofn.hwndOwner = hwParent;
	ofn.lpstrFilter = szExt;
	ofn.nFilterIndex = 1;
	if (bOpen) {
		ofn.Flags = OFN_SHOWHELP | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		int ret = GetOpenFileNameA(&ofn);
		if (ret)
		{
			szFile = buffer;
		}
		return ret;
	}
	else {
		ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
		if (GetSaveFileNameA(&ofn)) {
			szFile = buffer;
			char *s = strrchr(buffer, '\\');
			if (!s)
				s = buffer;
			else
				s++;
			if (!strrchr(s, '.')) {
				const char *e = szExt;
				e += strlen(e) + 1;
				if (*e) {
					for (int i = 1; i < (int)ofn.nFilterIndex; i++) {
						e += strlen(e) + 1;
						if (!*e)
							break;
						e += strlen(e) + 1;
						if (!*e)
							break;
					}
				}
				if (strstr(e, "."))
					e = strstr(e, ".");
				szFile += e;
			}
			return 1;
		}
		else
			return 0;
	}
}

//Menu options
const int QUIT = 1;
const int RESET = 2;
const int TOGGLE_JULIA = 3;
const int FORMULA_M_2 = 4;
const int FORMULA_BURNING_SHIP = 5;
const int FORMULA_M_3 = 6;
const int FORMULA_M_4 = 7;
const int FORMULA_M_5 = 8;
const int FORMULA_CHECKERS = 12;
const int VIEW_GUESSED_PIXELS = 9;
const int WINDOW_OPTIONS = 10;
const int FORMULA_TRIPLE_MATCHMAKER = 11;
const int FORMULA_HIGH_POWER = 13;
const int CHANGE_TRANSFORMATION = 14;
const int FORMULA_TEST_CONTROL = 15;
const int SAVE_IMAGE = 16;
const int SAVE_PARAMETERS = 17;
const int LOAD_PARAMETERS = 18;
const int CANCEL_RENDER = 19;
const int WINDOW_JSON = 20;

void AddMenus(HWND hwnd) {
	HMENU hMenubar = CreateMenu();
	HMENU hMenuOther = CreateMenu();
	HMENU hMenuFile = CreateMenu();

	//hMenuOther, item type, message (number), button text
	AppendMenuA(hMenuFile, MF_STRING, SAVE_IMAGE, "Save image");
	AppendMenuA(hMenuFile, MF_STRING, SAVE_PARAMETERS, "&Save parameters");
	AppendMenuA(hMenuFile, MF_STRING, LOAD_PARAMETERS, "&Load parameters");

	AppendMenuA(hMenuOther, MF_STRING, CANCEL_RENDER, "&Cancel Render");
	AppendMenuA(hMenuOther, MF_STRING, CHANGE_TRANSFORMATION, "Change &transformation");
	AppendMenuA(hMenuOther, MF_STRING, VIEW_GUESSED_PIXELS, "&View guessed pixels");
	AppendMenuA(hMenuOther, MF_SEPARATOR, 0, NULL);
	AppendMenuA(hMenuOther, MF_STRING, FORMULA_BURNING_SHIP, "&Burning Ship");
	AppendMenuA(hMenuOther, MF_STRING, FORMULA_M_4, "Mandelbrot power 4&");
	AppendMenuA(hMenuOther, MF_STRING, FORMULA_M_5, "Mandelbrot power 5&");
	AppendMenuA(hMenuOther, MF_STRING, FORMULA_TRIPLE_MATCHMAKER, "Triple Matchmaker&");
	AppendMenuA(hMenuOther, MF_STRING, FORMULA_TEST_CONTROL, "Test formula 2 (control)&");
	AppendMenuA(hMenuOther, MF_STRING, FORMULA_HIGH_POWER, "High Power Mandelbrot&");
	AppendMenuA(hMenuOther, MF_SEPARATOR, 0, NULL);
	AppendMenuA(hMenuOther, MF_STRING, QUIT, "&Quit");

	AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuFile, "&File");
	AppendMenuA(hMenubar, MF_STRING, WINDOW_OPTIONS, "&Options");
	AppendMenuA(hMenubar, MF_STRING, WINDOW_JSON, "&JSON");
	AppendMenuA(hMenubar, MF_STRING, RESET, "&Reset");
	AppendMenuA(hMenubar, MF_STRING, TOGGLE_JULIA, "&Toggle Julia");
	AppendMenuA(hMenubar, MF_STRING, FORMULA_M_2, "&Mandelbrot");
	AppendMenuA(hMenubar, MF_STRING, FORMULA_M_3, "Mandelbrot power 3");
	AppendMenuA(hMenubar, MF_STRING, FORMULA_CHECKERS, "&Checkers");
	AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuOther, "Mor&e");

	SetMenu(hwnd, hMenubar);
}

struct iterData {
	int iterationCount;
	bool guessed;
	bool inMinibrot;
};

class Formula {
public:
	int identifier;
	bool isGuessable;
	int inflectionPower;
	bool isLikeMandelbrot; //This means the procedure is iterating a Mandelbrot type formula (z->f(z, c))
	double escapeRadius;
	string name;

	double_c apply(double_c, double_c);
};

//constants for Triple Matchmaker:
const double sqrt3 = sqrt(3);
const double a = 2.2;
const double b = 1.4;
const double d = 1.1;

double_c Formula::apply(double_c z, double_c c) {
	switch (identifier) {
	case FORMULA_M_2:
		return pow(z, 2) + c;
	case FORMULA_M_3:
		return pow(z, 3) + c;
	case FORMULA_M_4:
		return pow(z, 4) + c;
	case FORMULA_M_5:
		return pow(z, 5) + c;
	case FORMULA_BURNING_SHIP:
		return pow((abs(real(z)) + abs(imag(z))*I), 2) + c;
	case FORMULA_TRIPLE_MATCHMAKER: {
		return (z + a / sqrt3) / (b*(pow(z, 3) - sqrt3 * a*pow(z, 2) + c * z + a * c / sqrt3)) + d;
	}
	case FORMULA_HIGH_POWER:
		return pow(z, 33554432) + c;
	default:
		return 0;
	}
}

//identifier; isGuessable; inflectionPower, isLikeMandelbrot, escapeRadius, name
const Formula M2 = { FORMULA_M_2, true, 2, true, 4, "Mandelbrot power 2" };
const Formula M3 = { FORMULA_M_3, true, 3, true, 2, "Mandelbrot power 3" };
const Formula M4 = { FORMULA_M_4, true, 4, true, pow(2, 2 / 3.), "Mandelbrot power 4" }; //Escape radius for Mandelbrot power n: pow(2, 2/(n-1))
const Formula M5 = { FORMULA_M_5, true, 5, true, pow(2, 2 / 4.), "Mandelbrot power 5" };
const Formula BURNING_SHIP = { FORMULA_BURNING_SHIP, false, 2, true, 4, "Burning ship" };
const Formula CHECKERS = { FORMULA_CHECKERS, true, 2, false, 4, "Checkers" };
const Formula TRIPLE_MATCHMAKER = { FORMULA_TRIPLE_MATCHMAKER, true, 2, false, 550, "Triple Matchmaker" };
const Formula HIGH_POWER = { FORMULA_HIGH_POWER, true, 2, true, 4, "High power Mandelbrot" };
const Formula TEST_CONTROL = { FORMULA_TEST_CONTROL, true, 2, false, 4, "Test" };

Formula getFormulaObject(int identifier) {
	switch (identifier) {
	case FORMULA_M_2:
		return M2;
	case FORMULA_M_3:
		return M3;
	case FORMULA_M_4:
		return M4;
	case FORMULA_M_5:
		return M5;
	case FORMULA_BURNING_SHIP:
		return BURNING_SHIP;
	case FORMULA_CHECKERS:
		return CHECKERS;
	case FORMULA_TRIPLE_MATCHMAKER:
		return TRIPLE_MATCHMAKER;
	case FORMULA_HIGH_POWER:
		return HIGH_POWER;
	case FORMULA_TEST_CONTROL:
		return TEST_CONTROL;
	}
	//Not found:
	Formula f; f.identifier = -1;
	return f;
}

//Program data
HBITMAP screenBMP;
UINT * ptPixels; //bitmap colors representing the iteration data
iterData * fractalCanvas; //iteration counts
UINT* gradientColors;
int lastRenderID;
int activeRenders = 0;

int cancelRender() {
	lock_guard<mutex> guard(threadCountChange);
	int renderID = ++lastRenderID;
	return renderID;
	//The program handles user input during a render by checking, every so often, whether a newer render has started.
	cout << "Active renders cancelled by increasing the last render ID to " << lastRenderID << endl;
}

double_c map(int i, int j);

#define get_trick(...) get_
#define readonly(type, name) \
private: type name; \
public: inline type get_trick()name() {\
	return name;\
}

struct FractalParameters {
	readonly(int, width)
		readonly(int, height)
		readonly(int, screenWidth)
		readonly(int, screenHeight)
		readonly(int, oversampling)
		readonly(double, rotation)
		readonly(double_c, center)
		readonly(double_c, topleftCorner)
		readonly(double, x_range)
		readonly(double, y_range)
		readonly(double, pixelWidth)
		readonly(double, pixelHeight)
		readonly(int, maxIters)
		readonly(double_c, juliaSeed)
		readonly(bool, julia)
		readonly(Formula, formula)
		readonly(int, transformation_type)
		readonly(vector<double_c>, inflectionCoords)

		readonly(int, inflectionCount)
		readonly(double, inflectionZoomLevel);

	readonly(double, gradientSpeed)
		readonly(double, gradientOffset)
		readonly(double, gradientSpeedFactor)
		readonly(int, gradientOffsetTerm)

public:

	double transferFunction(double gradientSpeedd) {
		return pow(1.1, gradientSpeedd - 1);
	}

	bool setGradientOffset(double newGradientOffset) {
		if (newGradientOffset >= 0.0 && newGradientOffset <= 1.0 && gradientOffset != newGradientOffset) {
			gradientOffset = newGradientOffset;
			int interpolatedGradientLength = NUMBER_OF_COLORS * transferFunction(gradientSpeed);
			gradientOffsetTerm = interpolatedGradientLength * gradientOffset;
			return true;
		}
		return false;
	}

	bool setGradientSpeed(double newGradientSpeed) {
		if (newGradientSpeed >= 0.0 && newGradientSpeed != gradientSpeed) {
			gradientSpeed = newGradientSpeed;
			double computedGradientSpeed = transferFunction(gradientSpeed);
			gradientSpeedFactor = 1.0 / computedGradientSpeed;
			gradientOffsetTerm = NUMBER_OF_COLORS * computedGradientSpeed * gradientOffset;
			return true;
		}
		return false;
	}

	double getZoomLevel() {
		return -log2(x_range) + 2;
	}

	bool setMaxIters(int newMaxIters) {
		if (newMaxIters < 1 || newMaxIters == maxIters)
			return false;
		maxIters = newMaxIters;
		return true;
	}

	bool setCenterAndZoom(double_c newCenter, double zoom) {
		//Set both of these settings together because the zoom level, topleftCorner coordinate and pixel size are related.

		auto setRenderRange = [&](double zoom) {
			double x_range_new = 4 / pow(2, zoom);
			double y_range_new = x_range_new * ((double)height / (double)width);
			if (x_range_new != x_range || y_range_new != y_range) {
				x_range = x_range_new;
				y_range = y_range_new;
				return true;
			}
			return false;
		};

		auto setCoordinates = [&](double_c newCenter) {
			bool recalcRequired = (center != newCenter);
			center = newCenter;
			topleftCorner = center - x_range / 2 + (y_range / 2)*I;
			return recalcRequired;
		};

		auto updatePixelSize = [&]() {
			double newPixelWidth = x_range / width;
			double newPixelHeight = y_range / height;
			if (newPixelHeight != pixelHeight || newPixelWidth != pixelWidth) {
				pixelWidth = newPixelWidth;
				pixelHeight = newPixelHeight;
				return true;
			}
			return false;
		};

		bool recalcRequired = false;
		//This order is important:
		recalcRequired |= setRenderRange(zoom);
		recalcRequired |= setCoordinates(newCenter);
		recalcRequired |= updatePixelSize();

		return recalcRequired;
	}

private:
	bool updateCenterAndZoom() {
		return setCenterAndZoom(center, getZoomLevel());
	}

public:
	bool resize(int newOversampling, int newWidth, int newHeight, int newScreenWidth, int newScreenHeight) {
		if (newWidth == width && newHeight == height) {
			cout << "entered resize, newWidth=width and newHeight=height. Nothing happens." << endl;
			return false;
		}
		else {
			if (
				!(newWidth > 0
					&& newWidth < MAXRES_WIDTH
					&& newHeight > 0
					&& newHeight < MAXRES_HEIGHT)
				) {
				cout << "Width or height outside of allowed range." << endl;
				cout << "width: " << newWidth << "  " << "height: " << newHeight << endl;
				cout << "maximum allowed: width: " << MAXRES_WIDTH << "  " << "height: " << MAXRES_HEIGHT << endl;
				return false;
			}
		}

		cout << "resize happens" << endl;

		{
			lock_guard<mutex> guard(renders);
			if (activeRenders != 0) {
				MessageBox(NULL,
					_T("Can't resize while a render is active."),
					_T("Problem"),
					NULL
				);
				return false;
			}

			oversampling = newOversampling;
			width = newWidth;
			height = newHeight;
			screenWidth = newScreenWidth;
			screenHeight = newScreenHeight;
			updateCenterAndZoom();
			//y_range = x_range * ((double)height / (double)width);

			cout << "before freeing memory" << endl;
			DeleteObject(screenBMP);
			free(fractalCanvas);
			fractalCanvas = (iterData*)malloc(width*height * sizeof(iterData));
			cout << "realloced fractalcanvas" << endl;

			HDC hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
			BITMAPINFO RGB32BitsBITMAPINFO;
			ZeroMemory(&RGB32BitsBITMAPINFO, sizeof(BITMAPINFO));
			RGB32BitsBITMAPINFO.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			RGB32BitsBITMAPINFO.bmiHeader.biWidth = width;
			RGB32BitsBITMAPINFO.bmiHeader.biHeight = height;
			RGB32BitsBITMAPINFO.bmiHeader.biPlanes = 1;
			RGB32BitsBITMAPINFO.bmiHeader.biBitCount = 32;
			screenBMP = CreateDIBSection(
				hdc,
				(BITMAPINFO *)&RGB32BitsBITMAPINFO,
				DIB_RGB_COLORS,
				(void**)&ptPixels,
				NULL, 0
			);
			SetWindowPos(hWndMain, HWND_TOP, 0, 0, screenWidth + 12, screenHeight + 84, SWP_NOMOVE);
			InvalidateRect(hWndMain, NULL, TRUE);
		}
		return true;
	}

	void toggleJulia() {
		julia = !julia;
		if (julia) {
			juliaSeed = center;
			setCenterAndZoom(0, 0);
		}
		else {
			setCenterAndZoom(juliaSeed, 0);
		}
	}

	bool changeFormula(int identifier) {
		Formula newFormula = getFormulaObject(identifier);
		if (newFormula.identifier == -1) return false; //The idenfitifer was not found by getFormulaObject
		if (formula.identifier != newFormula.identifier) {
			formula = newFormula;
			return true;
		}
		return false;
	}

	void changeTransformation() {
		transformation_type = (transformation_type + 1) % NUMBER_OF_TRANSFORMATIONS;
	}

	inline double_c map(int xPos, int yPos) {
		return get_topleftCorner() + xPos * get_pixelWidth() - yPos * get_pixelHeight()*I;
	}

	void setInflectionZoomLevel() {
		/*
			Normally when an inflection is set the zoom level gets reset to 0.
			This sets that "reset zoom level" to the current zoom level.

			However, addInflection uses a correction factor of 1/2 for every inflection set, to keep the pattern the same size.
			The reason is: An inflection halves the distance (the power of the magnification factor) to stuff lying deeper below.

			inflectionZoomLevel is used as a base zoom level (no correction factor applied).
			Therefore to derive it from the current zoom level, the correction needs to be undone (by multiplying by power).
		*/
		double power = pow(2, inflectionCount);
		inflectionZoomLevel = getZoomLevel()*power;
	}

	bool addInflection(int xPos, int yPos) {
		if (xPos < 0 || xPos > width || yPos < 0 || yPos > height) {
			return false;
		}
		while (inflectionCount >= inflectionCoords.size()) {
			cout << "inflectionCount: " << inflectionCount << "    inflectionCoords.size(): " << inflectionCoords.size() << endl;
			inflectionCoords.resize(inflectionCoords.size() * 2 + 1);
		}
		double_c thisInflectionCoord = map(xPos, yPos);
		inflectionCoords[inflectionCount] = thisInflectionCoord;
		inflectionCount++;
		setCenterAndZoom(0, inflectionZoomLevel*(1 / pow(2, inflectionCount)));
		return true;
	}

	bool removeInflection(int xPos, int yPos) {
		if (xPos < 0 || xPos > width || yPos < 0 || yPos > height) {
			return false;
		}
		if (inflectionCount > 0) {
			inflectionCount--;
			double_c newCenter = 0;
			if (inflectionCount == 0) {
				newCenter = inflectionCoords[0];
			}
			setCenterAndZoom(newCenter, inflectionZoomLevel*(1 / pow(2, inflectionCount)));
			return true;
		}
		return false;
	}

	void printInflections() {
		cout << "inflectionPower: " << formula.inflectionPower;
		cout << "inflection coords:" << endl;
		for (int i = 0; i < inflectionCount; i++) {
			printf("i=%d", i); printf(": %f ", real(inflectionCoords[i])); printf("+ %f * I\n", imag(inflectionCoords[i]));
		}
	}

private:
	bool initialized = false;

public:
	void initializeParameters() {
		if (initialized) return;
		else initialized = true;
		formula = M2;
		this->julia = false;
		this->juliaSeed = -0.75 + 0.1*I;
		this->inflectionCoords.resize(MAX_NUMBER_OF_INFLECTIONS);
		this->inflectionCount = 0;
		this->inflectionZoomLevel = 0;
		setCenterAndZoom(0, 0);
		setGradientSpeed(17.0);
		setGradientOffset(0);
		this->transformation_type = 0;
		setMaxIters(4600);
		resize(1, 1200, 800, 1200, 800);

		this->rotation = 0; //not implemented
	}

	string toJson() {
		//Create JSON object:
		Document document;
		document.Parse(" {} ");
		Document::AllocatorType& a = document.GetAllocator();

		//Add data:
		document.AddMember("programVersion", programVersion, a);
		document.AddMember("oversampling", oversampling, a);
		document.AddMember("width", screenWidth, a);
		document.AddMember("height", screenHeight, a);
		document.AddMember("rotation", rotation, a);

		Value centerv(kObjectType);
		centerv.AddMember("Re", real(center), a);
		centerv.AddMember("Im", imag(center), a);
		document.AddMember("center", centerv, a);

		document.AddMember("zoomLevel", getZoomLevel(), a);
		document.AddMember("maxIters", maxIters, a);

		Value juliaSeedv(kObjectType);
		juliaSeedv.AddMember("Re", real(juliaSeed), a);
		juliaSeedv.AddMember("Im", imag(juliaSeed), a);
		document.AddMember("juliaSeed", juliaSeedv, a);

		document.AddMember("julia", julia, a);
		document.AddMember("formula_identifier", formula.identifier, a);
		document.AddMember("transformation_type", transformation_type, a);
		document.AddMember("inflectionCount", inflectionCount, a);

		Value inflectionCoordsv(kArrayType);
		for (int i = 0; i < inflectionCount; i++) {
			Value thisCoordv(kObjectType);
			thisCoordv.AddMember("Re", real(inflectionCoords[i]), a);
			thisCoordv.AddMember("Im", imag(inflectionCoords[i]), a);
			inflectionCoordsv.PushBack(thisCoordv, a);
		}
		document.AddMember("inflectionCoords", inflectionCoordsv, a);

		document.AddMember("gradientSpeed", gradientSpeed, a);
		document.AddMember("gradientOffset", gradientOffset, a);
		document.AddMember("inflectionZoomLevel", inflectionZoomLevel, a);

		//Get string
		StringBuffer sb;
		PrettyWriter<StringBuffer> writer(sb);
		document.Accept(writer);
		cout << sb.GetString() << endl;

		return sb.GetString();
	}

	bool fromJson(string jsonString) {
		Document document;
		if (document.Parse(jsonString.c_str()).HasParseError()) {
			//parsing JSON failed
			return false;
		}
		else {
			//read from JSON content:
			int oversampling_r = document["oversampling"].GetInt();
			int width_r = document["width"].GetInt();
			int height_r = document["height"].GetInt();
			cout << height_r << " " << width_r << " " << oversampling_r << " " << endl;
			resize(oversampling_r, width_r*oversampling_r, height_r*oversampling_r, width_r, height_r);

			double_c center_r = document["center"]["Re"].GetDouble() + document["center"]["Im"].GetDouble() * I;
			double zoom_r = document["zoomLevel"].GetDouble();
			setCenterAndZoom(center_r, zoom_r);

			int maxIters_r = document["maxIters"].GetInt();
			setMaxIters(maxIters_r);

			double_c juliaSeed_r = document["juliaSeed"]["Re"].GetDouble() + document["juliaSeed"]["Im"].GetDouble() * I;
			bool julia_r = document["julia"].GetBool();
			juliaSeed = juliaSeed_r;
			julia = julia_r;

			int formula_identifier_r = document["formula_identifier"].GetInt();
			int transformation_type_r = document["transformation_type"].GetInt();
			changeFormula(formula_identifier_r);
			transformation_type = transformation_type_r;

			int inflectionCount_r = document["inflectionCount"].GetInt();
			inflectionCount = inflectionCount_r;
			cout << "inflectionCount: " << inflectionCount;
			Value& inflectionCoordsv = document["inflectionCoords"];
			for (int i = 0; i < inflectionCount_r; i++) {
				inflectionCoords[i] = inflectionCoordsv[i]["Re"].GetDouble() + inflectionCoordsv[i]["Im"].GetDouble() * I;
			}

			gradientOffset = 0; //is dit nodig? voor de zekerheid initialiseren?
			double gradientSpeed_r = document["gradientSpeed"].GetDouble();
			double gradientOffset_r = document["gradientOffset"].GetDouble();

			setGradientSpeed(gradientSpeed_r);
			setGradientOffset(gradientOffset_r);

			double inflectionZoomlevel_r = document["inflectionZoomLevel"].GetDouble();
			inflectionZoomLevel = inflectionZoomlevel_r;
		}
		return true;
	}

	bool readWriteParameters(bool read, string fileName) {
		bool succes = true;
		if (read) {
			cout << "enters reading parameters" << endl;

			//First try to interpret the file as JSON (files from program version >= 5)
			{
				ifstream infile;
				infile.open(fileName);
				if (infile.is_open()) {
					stringstream strStream;
					strStream << infile.rdbuf(); //now the JSON-string can be accessed by strStream.str()
					if (!fromJson(strStream.str())) {
						infile.close();
						goto binary_file_label;
					}
					//else: fromJson already parsed the json and changed the parameters in the program
				}
				else {
					cout << "opening infile failed" << endl;
					succes = false;
				}
				infile.close();
			}
			return succes;

		binary_file_label:
			//When here, the file can't be parsed as JSON. Try to interpret as binary data (files from program version <= 4) 
			{
				ifstream infile;
				infile.open(fileName, ios::binary);
				if (infile.is_open()) {
					double programVersion_r;
					infile.read((char*)&programVersion_r, sizeof(double));
					cout << "read ProgramVersion: " << programVersion_r << endl;

					initializeParameters();

					//read parameters:
					if (programVersion_r <= 4.0) { //how to handle files from version 4.0 and before

						cout << "entered reading with version <= 4.0" << endl;
						int oversampling_r; infile.read((char*)&oversampling_r, sizeof(int));
						int width_r; infile.read((char*)&width_r, sizeof(int));
						int height_r; infile.read((char*)&height_r, sizeof(int));
						resize(oversampling_r, width_r*oversampling_r, height_r*oversampling_r, width_r, height_r);

						double_c center_r; infile.read((char*)&center_r, sizeof(double_c));
						double zoom_r; infile.read((char*)&zoom_r, sizeof(double));
						setCenterAndZoom(center_r, zoom_r);

						int maxIters_r; infile.read((char*)&maxIters_r, sizeof(int));
						setMaxIters(maxIters_r);

						double bailout_r; infile.read((char*)&bailout_r, sizeof(double));
						//bailout = bailout_r; //not used anymore

						double_c juliaSeed_r; infile.read((char*)&juliaSeed_r, sizeof(double_c));
						bool julia_r; infile.read((char*)&julia_r, sizeof(bool));
						juliaSeed = juliaSeed_r;
						julia = julia_r;

						int formula_identifier_r; infile.read((char*)&formula_identifier_r, sizeof(int));
						int transformation_type_r; infile.read((char*)&transformation_type_r, sizeof(int));
						changeFormula(formula_identifier_r);
						transformation_type = transformation_type_r;

						int inflectionCount_r; infile.read((char*)&inflectionCount_r, sizeof(int));
						int inflectionPower_r; infile.read((char*)&inflectionPower_r, sizeof(int));
						inflectionCount = inflectionCount_r;
						//inflectionPower = inflectionPower_r; //not used anymore
						for (int i = 0; i < inflectionCount_r; i++) {
							infile.read((char*)&inflectionCoords[i], sizeof(double_c));
						}

						gradientOffset = 0; //is dit nodig? voor de zekerheid initialiseren?
						double gradientSpeed_r; infile.read((char*)&gradientSpeed_r, sizeof(double));
						double gradientOffset_r; infile.read((char*)&gradientOffset_r, sizeof(double));

						setGradientSpeed(gradientSpeed_r);
						setGradientOffset(gradientOffset_r);
					}
					else if (programVersion_r <= programVersion) {

						cout << "entered reading with version < 5.0" << endl;
						int oversampling_r; infile.read((char*)&oversampling_r, sizeof(int));
						int width_r; infile.read((char*)&width_r, sizeof(int));
						int height_r; infile.read((char*)&height_r, sizeof(int));
						cout << height_r << " " << width_r << " " << oversampling_r << " " << endl;
						resize(oversampling_r, width_r*oversampling_r, height_r*oversampling_r, width_r, height_r);

						double_c center_r; infile.read((char*)&center_r, sizeof(double_c));
						double zoom_r; infile.read((char*)&zoom_r, sizeof(double));
						setCenterAndZoom(center_r, zoom_r);

						int maxIters_r; infile.read((char*)&maxIters_r, sizeof(int));
						setMaxIters(maxIters_r);

						double_c juliaSeed_r; infile.read((char*)&juliaSeed_r, sizeof(double_c));
						bool julia_r; infile.read((char*)&julia_r, sizeof(bool));
						juliaSeed = juliaSeed_r;
						julia = julia_r;

						int formula_identifier_r; infile.read((char*)&formula_identifier_r, sizeof(int));
						int transformation_type_r; infile.read((char*)&transformation_type_r, sizeof(int));
						changeFormula(formula_identifier_r);
						transformation_type = transformation_type_r;

						int inflectionCount_r; infile.read((char*)&inflectionCount_r, sizeof(int));
						inflectionCount = inflectionCount_r;
						for (int i = 0; i < inflectionCount_r; i++) {
							infile.read((char*)&inflectionCoords[i], sizeof(double_c));
						}

						gradientOffset = 0; //is dit nodig? voor de zekerheid initialiseren?
						double gradientSpeed_r; infile.read((char*)&gradientSpeed_r, sizeof(double));
						double gradientOffset_r; infile.read((char*)&gradientOffset_r, sizeof(double));

						setGradientSpeed(gradientSpeed_r);
						setGradientOffset(gradientOffset_r);

						double inflectionZoomlevel_r; infile.read((char*)&inflectionZoomlevel_r, sizeof(double));
						inflectionZoomLevel = inflectionZoomlevel_r;
					}
					else {
						succes = false;
					}
					infile.close();
				}
				else {
					cout << "opening infile failed" << endl;
					succes = false;
				}
			}
		}
		else {
			cout << "enters writing parameters" << endl;
			//Saving parameters to file
			ofstream outfile(fileName);
			if (outfile.is_open()) {
				outfile << toJson(); //converts parameter struct to JSON
				outfile.close();
			}
			else {
				cout << "opening outfile failed" << endl;
				succes = false;
			}
			return succes;
		}
		return false;
	}
};

FractalParameters S;

void initializeProgramData() {
	S.initializeParameters();

	gradientColors = (UINT*)malloc(NUMBER_OF_COLORS * sizeof(UINT));
	gradientColors[0] = RGB(255, 255, 255);
	gradientColors[1] = RGB(52, 140, 167);
	gradientColors[2] = RGB(0, 0, 0);
	gradientColors[3] = RGB(229, 140, 45);

	lastRenderID = 0;
}

inline UINT gradient(int iterationCount) {
	double gradientPosition = (iterationCount + S.get_gradientOffsetTerm()) * S.get_gradientSpeedFactor();
	UINT asInt = (UINT)gradientPosition;
	UINT previousColor = gradientColors[asInt & (NUMBER_OF_COLORS - 1)];
	UINT nextColor = gradientColors[(asInt + 1) & (NUMBER_OF_COLORS - 1)];
	double ratio = gradientPosition - asInt;
	return RGB2(
		(unsigned char)((GetRValue(previousColor))*(1 - ratio) + (GetRValue(nextColor))*ratio),
		(unsigned char)((GetGValue(previousColor))*(1 - ratio) + (GetGValue(nextColor))*ratio),
		(unsigned char)((GetBValue(previousColor))*(1 - ratio) + (GetBValue(nextColor))*ratio)
	);
}

/*
void inline setPixel(int i, int j, int iterationCount, bool isGuessed) {
	//only for debug this if:
	//if (i < 0 || j < 0 || i > width || j > height || iterationCount < 0) {
	//	cout << "Setting pixel failed at location i: " << i << "  " << "j: " << j << endl;
	//	return false;
	//}
	bool isInMinibrot = iterationCount >= maxIters;
	fractalCanvas[i*height + j] = {
		iterationCount,
		isGuessed,
		isInMinibrot
	};
	if (isInMinibrot) ptPixels[width*(height - j - 1) + i] = RGB2(0, 0, 0);
	else			  ptPixels[width*(height - j - 1) + i] = gradient(iterationCount);
}
*/

#define ISCALCULATED false
#define ISGUESSED true

#define setPixel(i,j,iterationCount,isGuessed) \
assert(i < S.get_width() && j < S.get_height()); \
bool __isInMinibrot = iterationCount == maxIters; \
fractalCanvas[i*height + j] = { \
	iterationCount, \
	isGuessed, \
	__isInMinibrot \
}; \
ptPixels[width*(height - j - 1) + i] = (__isInMinibrot ? RGB2(0, 0, 0) : gradient(iterationCount));

inline int getPixel(int i, int j) {
	return fractalCanvas[i*S.get_height() + j].iterationCount;
}

iterData getIterData(int i, int j) {
	return fractalCanvas[i*S.get_height() + j];
}

void loadFractalCanvas(bool falseColor) {
	int width = S.get_width();
	int height = S.get_height();
	iterData it;

	if (falseColor) {
		for (int i = 0; i < width; i++) {
			for (int j = 0; j < height; j++) {
				it = fractalCanvas[i*height + j];
				if (it.inMinibrot && !it.guessed) ptPixels[width*(height - j - 1) + i] = RGB2(255, 0, 0);
				else if (it.inMinibrot)			  ptPixels[width*(height - j - 1) + i] = RGB2(0, 0, 255);
				else if (it.guessed)			  ptPixels[width*(height - j - 1) + i] = RGB2(0, 255, 0);
				else							  ptPixels[width*(height - j - 1) + i] = gradient(it.iterationCount);
			}
		}
	}
	else {
		for (int i = 0; i < width; i++) {
			for (int j = 0; j < height; j++) {
				it = fractalCanvas[i*height + j];
				if (it.inMinibrot)				  ptPixels[width*(height - j - 1) + i] = RGB2(0, 0, 0);
				else							  ptPixels[width*(height - j - 1) + i] = gradient(it.iterationCount);
			}
		}
	}
}

BOOL DrawBitmap(HDC hDC, int x, int y, HBITMAP hBitmap, DWORD dwROP) {
	int width = S.get_width();
	int height = S.get_height();
	int screenWidth = S.get_screenWidth();
	int screenHeight = S.get_screenHeight();
	HDC hDCBits;
	BOOL bResult;
	if (!hDC || !hBitmap)
		return FALSE;
	hDCBits = CreateCompatibleDC(hDC);
	SelectObject(hDCBits, hBitmap);
	if (width != screenWidth || height != screenHeight) {
		cout << "screenwidth != width (or height) " << endl;
		SetStretchBltMode(hDC, HALFTONE);
		bResult = StretchBlt(hDC, x, y, screenWidth, screenHeight, hDCBits, 0, 0, width, height, dwROP);
	}
	else {
		bResult = BitBlt(hDC, x, y, width, height, hDCBits, 0, 0, dwROP);
	}
	DeleteDC(hDCBits);
	return bResult;
}

void refreshBitmap(bool viewGuessedPixels) {

	HDC hdc = GetDC(hWndMain);
	loadFractalCanvas(viewGuessedPixels);
	DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY);
	ReleaseDC(hWndMain, hdc);

	/*
	InvalidateRect(hWndMain, NULL, TRUE);
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWndMain, &ps);
	loadFractalCanvas(viewGuessedPixels);
	DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY);
	EndPaint(hWndMain, &ps);
	*/
}

inline double_c map(int i, int j) {
	//Maps a pixel at location (i, j) in the window to a complex number.
	return S.get_topleftCorner() + i * S.get_pixelWidth() - j * S.get_pixelHeight()*I;
}

double_c transformation(double_c c) {
	switch (S.get_transformation_type()) {
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
	}
}

/*
inline double_c inflections(double_c c) {
	for (int i = S.get_inflectionCount() - 1; i >= 0; i--) {
		c = pow(c, S.get_formula().inflectionPower) + S.get_inflectionCoords()[i];
	}
	return c;
}
*/

class Render {
public:
	FractalParameters S2;
	int renderID;
	int width;
	int height;
private:
	int threadCount;
	Formula formula;
	bool julia;
	double_c juliaSeed;
	int maxIters;
	double escapeRadius;
	vector<double_c> inflectionCoords;
	int inflectionCount;
	int inflectionPower;
public:
	chrono::time_point<chrono::steady_clock> startTime;
	chrono::time_point<chrono::steady_clock> endTime;
	bool isFinished;
	int guessedPixelCount;
	int calculatedPixelCount;
	int pixelGroupings;
	long long computedIterations;
	Render(int);
	~Render();
private:
	void addToThreadcount(int);
	int calcPixel(int, int);
	bool calcHorizontalLine(int, int, int);
	bool calcVerticalLine(int, int, int);
	bool calcPixelVector(vector<int>&, int, int);
	bool isSameHorizontalLine(int, int, int);
	bool isSameVerticalLine(int, int, int);
	void renderSilverRect(bool, int, int, int, int, bool, int, bool, int, bool, int, bool, int);
	void renderSilverFull();
	double_c inflections(double_c);
	double_c inflections_m2(double_c);
public:
	void execute();
	double getElapsedTime();
};

Render::Render(int currentRenderID) {
	cout << "creating render" << endl;
	renderID = currentRenderID;
	threadCount = 0;
	pixelGroupings = 0;
	guessedPixelCount = 0;
	calculatedPixelCount = 0;
	computedIterations = 0;
	isFinished = false;

	S2 = S; //snapshot of the current settings S

	formula = S2.get_formula();
	julia = S2.get_julia();
	juliaSeed = S2.get_juliaSeed();
	width = S2.get_width();
	height = S2.get_height();
	maxIters = S2.get_maxIters();
	escapeRadius = S2.get_formula().escapeRadius;
	inflectionCoords = S2.get_inflectionCoords(); //This makes a copy of the whole vector.
	inflectionCount = S2.get_inflectionCount();
	inflectionPower = S2.get_formula().inflectionPower;
}

Render::~Render(void) {
	cout << "Render " << renderID << " is being deleted" << endl;
}

double_c Render::inflections_m2(double_c c) {
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

		//c = pow(c, inflectionPower) + inflectionCoords[i];
	}
	return zr + zi * I;
}

double_c Render::inflections(double_c c) {
	for (int i = inflectionCount - 1; i >= 0; i--) {
		c = pow(c, inflectionPower) + inflectionCoords[i];
	}
	return c;
}

int usedThreads = 0;

void Render::addToThreadcount(int n) {
	lock_guard<mutex> guard(threadCountChange);
	threadCount += n;
	usedThreads++;
}

int Render::calcPixel(int i, int j) {
	int iterationCount = 0;

	if (formula.isLikeMandelbrot) {
		switch (formula.identifier) {
		case FORMULA_M_2: {
			double_c c = transformation(inflections_m2(map(i, j)));

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
					setPixel(i, j, maxIters, ISCALCULATED);
					return maxIters;
				}
				zx = zx2;
				zy = zy2;
				zx++;
				if (zx * zx + zy * zy < 0.0625) {
					setPixel(i, j, maxIters, ISCALCULATED);
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
			break;
		}

		default: {
			double_c c;
			double_c z;
			if (julia) {
				c = juliaSeed;
				z = transformation(inflections(map(i, j)));
			}
			else {
				c = transformation(inflections(map(i, j)));
				z = 0;
			}
			while (real(z)*real(z) + imag(z)*imag(z) < escapeRadius && iterationCount < maxIters) {
				z = formula.apply(z, c);
				iterationCount++;
			}
			break;
		}
		}
	}
	else {
		switch (formula.identifier) {
		case FORMULA_TEST_CONTROL: { //FORMULA_TEST_CONTROL
			int thisMaxIters = 100;
			double_c c = transformation(inflections(map(i, j)));
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
			break;
		}

		case FORMULA_CHECKERS: {
			double_c c = transformation(inflections_m2(map(i, j)));
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
			break;
		}

		case FORMULA_TRIPLE_MATCHMAKER: {
			double_c c;
			double_c z;
			double summ = 0;
			/*
			double minn = 1000000000000.0;
			double abss;
			*/
			if (julia) {
				c = juliaSeed;
				z = transformation(inflections(map(i, j)));
			}
			else {
				c = transformation(inflections(map(i, j)));
				z = 0;
			}
			for (int k = 2; k < maxIters; k++) {
				z = formula.apply(z, c);
				summ += abs(z);
				/*
				abss = abs(z);
				if (abss < minn) {
					minn = abss;
				}
				*/
			}
			iterationCount = (int)(summ);
			break;
		}
		}
	}

	setPixel(i, j, iterationCount, ISCALCULATED);

	return iterationCount;
}

#define setPixelAndThisIter(i, j, iterationCount, mode) \
setPixel(i, j, iterationCount, mode); \
if (isSame) \
	if (iterationCount != thisIter) { \
		if (thisIter == -1) \
			thisIter = iterationCount; \
		else \
			isSame = false; \
	}

bool Render::calcPixelVector(vector<int>& pixelVect, int fromPixel, int toPixel) {
	if (renderID != lastRenderID) {
		cout << "Render " << renderID << " cancelled; terminating thread" << endl;
		return true; //Returning true but it doesn't really matter what the result is when the render is cancelled anyway.
	}

	int pixelCount = toPixel - fromPixel;
	bool isSame = true;
	int thisIter = -1;

	if (formula.identifier != FORMULA_M_2 || !USING_AVX || pixelCount < 4) {
		int thisI = pixelVect[fromPixel + fromPixel];
		int thisJ = pixelVect[fromPixel + fromPixel + 1];
		int thisIter = calcPixel(thisI, thisJ);

		for (int k = fromPixel + 1; k < toPixel; k++) {
			assert(thisI < width); assert(thisJ < height);

			thisI = pixelVect[k + k];
			thisJ = pixelVect[k + k + 1];
			if (calcPixel(thisI, thisJ) != thisIter)
				isSame = false;
		}
	}
	else { //AVX only for Mandelbrot power 2
		//AVX is used. We construct length 4 arrays and vectors to iterate 4 pixels at once. That means 4 i-values, 4 j-values, 4 c-values etc.
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
			inflections_m2(transformation(map(i[0], j[0])))
			,inflections_m2(transformation(map(i[1], j[1])))
			,inflections_m2(transformation(map(i[2], j[2])))
			,inflections_m2(transformation(map(i[3], j[3])))
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

							setPixelAndThisIter(i[k], j[k], iterationCounts[k], ISCALCULATED); //it is done so we set it
							iterationCounts[k] = -1; //to mark pixel k in the vectors as done/invalid

							bool pixelIsValid = false;

							while (!pixelIsValid && nextPixel < toPixel) {
								//take new pixel:
								i[k] = pixelVect[nextPixel + nextPixel];
								j[k] = pixelVect[nextPixel + nextPixel + 1];
								c[k] = inflections_m2(transformation(map(i[k], j[k])));
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
										setPixelAndThisIter(i[k], j[k], maxIters, ISCALCULATED);
									}
									else {
										zx = zx_backup;
										zy = zy_backup;
										zx -= 0.25;
										zy *= zy;
										double q = zx * zx + zy;
										if (4 * q*(q + zx) < zy) {
											setPixelAndThisIter(i[k], j[k], maxIters, ISCALCULATED);
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
				setPixelAndThisIter(i[k], j[k], iterationCount, ISCALCULATED);
			}
		}
	}
	calculatedPixelCount += pixelCount;
	return isSame && formula.isGuessable;
}

bool Render::isSameHorizontalLine(int iFrom, int iTo, int height) {
	assert(iTo >= iFrom);
	if (!formula.isGuessable) return false;

	bool same = true;
	int thisIter = getPixel(iFrom, height);
	for (int i = iFrom + 1; i < iTo; i++) {
		if (thisIter != getPixel(i, height)) {
			same = false;
			break;
		}
	}
	return same;
}

bool Render::isSameVerticalLine(int jFrom, int jTo, int width) {
	assert(jTo >= jFrom);
	if (!formula.isGuessable) return false;

	bool same = true;
	int thisIter = getPixel(width, jFrom);
	for (int j = jFrom + 1; j < jTo; j++) {
		if (thisIter != getPixel(width, j)) {
			same = false;
			break;
		}
	}
	return same;
}

bool Render::calcHorizontalLine(int iFrom, int iTo, int height) {
	assert(iTo >= iFrom);
	int size = iTo - iFrom;
	vector<int> hLine(size * 2);
	int thisIndex = 0;
	for (int i = iFrom; i < iTo; i++) {
		hLine[thisIndex++] = i;
		hLine[thisIndex++] = height;
	}
	return calcPixelVector(hLine, 0, thisIndex / 2);
}

bool Render::calcVerticalLine(int jFrom, int jTo, int width) {
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


void Render::renderSilverRect(bool inNewThread, int imin, int imax, int jmin, int jmax,
	bool sameTop, int iterTop, bool sameBottom, int iterBottom, bool sameLeft, int iterLeft, bool sameRight, int iterRight)
{
	int size;
	if (renderID != lastRenderID) {
		cout << "Render " << renderID << " cancelled; terminating thread" << endl;
		return;
	}

	if (sameRight && sameLeft && sameTop && sameBottom && iterRight == iterTop && iterTop == iterLeft && iterLeft == iterBottom && iterRight != 1 && iterRight != 0) {
		//The complete boundary of the tile has the same iterationCount. Fill with that same value:
		for (int i = imin + 1; i < imax; i++) {
			for (int j = jmin + 1; j < jmax; j++) {
				setPixel(i, j, iterLeft, ISGUESSED);
			}
		}
		guessedPixelCount += (imax - imin - 1)*(jmax - jmin - 1);
		goto returnLabel;
	}

	size = (imax - imin + 1)*(jmax - jmin + 1);
	if (size < 50) {
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
		goto returnLabel;
	}

	//The tile gets split up:
	if (imax - imin < jmax - jmin) {
		//The tile is rather tall. We split the tile with a horizontal line. Our fixed height is:
		int j = jmin + (jmax - jmin) / 2;

		//compute new line
		bool sameNewLine = calcHorizontalLine(imin + 1, imax, j);
		int iterNewLine = getPixel(imin + 1, j);

		//check right and left for equality
		bool sameRightTop = true;
		bool sameLeftTop = true;
		bool sameLeftBottom = true;
		bool sameRightBottom = true;
		int iterRightTop = getPixel(imax, jmin);
		int iterRightBottom = getPixel(imax, j);
		int iterLeftTop = getPixel(imin, jmin);
		int iterLeftBottom = getPixel(imin, j);

		if (!sameRight) {
			sameRightTop = isSameVerticalLine(jmin, j, imax);
			sameRightBottom = isSameVerticalLine(j, jmax, imax);
		}
		if (!sameLeft) {
			sameLeftTop = isSameVerticalLine(jmin, j, imin);
			sameLeftBottom = isSameVerticalLine(j, jmax, imin);

		}
		if (threadCount < NUMBER_OF_THREADS) {
			addToThreadcount(1);
			thread t1(&Render::renderSilverRect, this, true, imin, imax, jmin, j,
				sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
			renderSilverRect(inNewThread, imin, imax, j, jmax,
				sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
			t1.join();
			return;
		}
		else {
			if (height - jmax < 0.5*height) {
				renderSilverRect(false, imin, imax, jmin, j,
					sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
				renderSilverRect(false, imin, imax, j, jmax,
					sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
			}
			else {
				//same in different order. The intention is that the center of the screen receives priority.
				renderSilverRect(false, imin, imax, j, jmax,
					sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
				renderSilverRect(false, imin, imax, jmin, j,
					sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
			}

		}
	}
	else {
		//The tile is wider than it's tall. We split the tile with a vertical line. Our fixed width is:
		int i = imin + (imax - imin) / 2;

		//Compute new line
		bool sameNewLine = calcVerticalLine(jmin + 1, jmax, i);
		int iterNewLine = getPixel(i, jmin + 1);

		//Check Top and Bottom for equality
		bool sameRightTop = true;
		bool sameLeftTop = true;
		bool sameLeftBottom = true;
		bool sameRightBottom = true;
		int iterRightTop = getPixel(i, jmin);
		int iterLeftTop = getPixel(imin, jmin);
		int iterRightBottom = getPixel(i, jmax);
		int iterLeftBottom = getPixel(imin, jmax);

		if (!sameTop) {
			sameLeftTop = isSameHorizontalLine(imin, i, jmin);
			sameRightTop = isSameHorizontalLine(i, imax, jmin);
		}
		if (!sameBottom) {
			sameLeftBottom = isSameHorizontalLine(imin, i, jmax);
			sameRightBottom = isSameHorizontalLine(i, imax, jmax);
		}

		if (threadCount < NUMBER_OF_THREADS) {
			addToThreadcount(1);
			thread t1(&Render::renderSilverRect, this, true, imin, i, jmin, jmax,
				sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
			renderSilverRect(inNewThread, i, imax, jmin, jmax,
				sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
			t1.join();
			return;
		}
		else {
			if (width - imax < 0.5*width) {
				renderSilverRect(false, imin, i, jmin, jmax,
					sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
				renderSilverRect(false, i, imax, jmin, jmax,
					sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
			}
			else {
				renderSilverRect(false, i, imax, jmin, jmax,
					sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
				renderSilverRect(false, imin, i, jmin, jmax,
					sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
			}
		}
	}

returnLabel:
	pixelGroupings += 2;
	if (inNewThread) {
		addToThreadcount(-1);
	}
	return;
}

void Render::renderSilverFull() {
	//This calculates a raster of pixels first and then launches threads for each tile in the raster.
	//Use the option "View guessed pixels" in the program while in a sparse area to see how it works.
	int imin = 0;
	int imax = width - 1;
	int jmin = 0;
	int jmax = height - 1;

	double tileGridSize = sqrt(NUMBER_OF_THREADS);
	int floor = (int)(tileGridSize);

	int widthStep = width / floor;
	int heightStep = height / floor;

	vector<bool> isSameList(2 * floor*(floor + 1));
	vector<int> heights(floor + 1);
	vector<int> widths(floor + 1);

	for (int k = 0; k < floor; k++) {
		heights[k] = k * heightStep;
		widths[k] = k * widthStep;
	}
	heights[floor] = jmax;
	widths[floor] = imax;

	vector<int> rasterCoordinates((floor + 1)*(height + width) * 2); //all pixels in the raster in (x-coord, y-coord) format: x-coords are at odd indices (0, 3, ....)
	int rasterCoordinateIndex = 0;

	int thisHeight;
	int thisWidth;
	for (int lineNumH = 0; lineNumH < floor; lineNumH++) {
		for (int lineNumV = 0; lineNumV < floor; lineNumV++) {
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
	for (int lineNumH = 0; lineNumH < floor; lineNumH++) {
		for (int k = widths[lineNumH]; k < widths[lineNumH + 1]; k++) {
			rasterCoordinates[rasterCoordinateIndex++] = k;
			rasterCoordinates[rasterCoordinateIndex++] = jmax;
		}
	}
	for (int lineNumV = 0; lineNumV < floor; lineNumV++) {
		for (int k = heights[lineNumV]; k < heights[lineNumV + 1]; k++) {
			rasterCoordinates[rasterCoordinateIndex++] = imax;
			rasterCoordinates[rasterCoordinateIndex++] = k;
		}
	}
	rasterCoordinates[rasterCoordinateIndex++] = imax;
	rasterCoordinates[rasterCoordinateIndex++] = jmax;


	//Calculate the array of raster pixels multithreaded:
	int usingThreadCount = floor * floor;
	int pixelCount = rasterCoordinateIndex / 2;
	int thrVectPieceSize = pixelCount / usingThreadCount;
	int createdRasterThreads = 0;
	vector<thread> threadsRaster(usingThreadCount);

	for (int k = 0; k < usingThreadCount - 1; k++) {
		threadsRaster[createdRasterThreads++] = thread(&Render::calcPixelVector, this, ref(rasterCoordinates), k * thrVectPieceSize, (k + 1)*thrVectPieceSize);
	}
	threadsRaster[createdRasterThreads++] = thread(&Render::calcPixelVector, this, ref(rasterCoordinates), (usingThreadCount - 1) * thrVectPieceSize, pixelCount);

	cout << "joining " << createdRasterThreads << " raster threads" << endl;
	for (int k = 0; k < createdRasterThreads; k++) {
		threadsRaster[k].join();
	}
	cout << "threads joined" << endl;


	//Check which rectangles in the raster can be guessed:
	for (int lineNumH = 0; lineNumH < floor; lineNumH++) {
		for (int lineNumV = 0; lineNumV < floor; lineNumV++) {
			isSameList[(lineNumH * floor + lineNumV) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], heights[lineNumV]);
			isSameList[(lineNumH * floor + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], widths[lineNumH]);
		}
	}
	for (int lineNumH = 0; lineNumH < floor; lineNumH++) {
		isSameList[(lineNumH * floor + floor) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], jmax);
	}
	for (int lineNumV = 0; lineNumV < floor; lineNumV++) {
		isSameList[(floor * floor + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], imax);
	}

	//Launch threads to fully compute each rectangle
	vector<thread> threadsTiles(usingThreadCount);
	int createdTileThreads = 0;
	threadCount = usingThreadCount;

	for (int lineNumH = 0; lineNumH < floor; lineNumH++) {
		for (int lineNumV = 0; lineNumV < floor; lineNumV++) {
			int thisImin = widths[lineNumH];
			int thisImax = widths[lineNumH + 1];
			int thisJmin = heights[lineNumV];
			int thisJmax = heights[lineNumV + 1];

			int iterTop = getPixel(thisImax, thisJmin);
			int iterBottom = getPixel(thisImax, thisJmax);
			int iterLeft = getPixel(thisImin, thisJmax);
			int iterRight = getPixel(thisImax, thisJmax);


			bool sameTop = isSameList[(lineNumH * floor + lineNumV) * 2];
			bool sameBottom = isSameList[(lineNumH * floor + (lineNumV + 1)) * 2];
			bool sameLeft = isSameList[(lineNumH * floor + lineNumV) * 2 + 1];
			bool sameRight = isSameList[((lineNumH + 1) * floor + lineNumV) * 2 + 1];

			threadsTiles[createdTileThreads++] = thread(&Render::renderSilverRect, this, true, thisImin, thisImax, thisJmin, thisJmax,
				sameTop, iterTop, sameBottom, iterBottom, sameLeft, iterLeft, sameRight, iterRight);
		}
	}

	cout << "joining " << createdTileThreads << " tile threads" << endl;
	for (int k = 0; k < createdTileThreads; k++) {
		threadsTiles[k].join();
	}
	cout << "threads joined" << endl;
	return;
}

double Render::getElapsedTime() {
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

void Render::execute() {
	cout << "in execute " << renderID << endl;
	startTime = chrono::high_resolution_clock::now();
	renderSilverFull();
	endTime = chrono::high_resolution_clock::now();
	isFinished = true;

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			computedIterations += fractalCanvas[i*height + j].iterationCount;
		}
	}
}

void refreshDuringRender(Render& R) {
	this_thread::sleep_for(std::chrono::milliseconds(70));

	while (lastRenderID == R.renderID && !R.isFinished) {
		{
			lock_guard<mutex> guard(drawingBitmap);
			if (R.isFinished == false) { //to prevent drawing after the render is already finished, because a drawing is already made immediately after the render finishes
				HDC hdc = GetDC(hWndMain);
				DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY);
				ReleaseDC(hWndMain, hdc);
				double percentage = (double)(R.guessedPixelCount + R.calculatedPixelCount) / (R.width * R.height) * 100;
				stringstream progressString; progressString << fixed << setprecision(2) << percentage << "%";
				string elapsedString = to_string(R.getElapsedTime()) + " s";
				SendMessageA(statusBar, SB_SETTEXTA, 0, (LPARAM)(progressString.str().c_str()));
				SendMessageA(statusBar, SB_SETTEXTA, 1, (LPARAM)(elapsedString.c_str()));
				firstPaint = false;
			}
		}
		this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
	
void createNewRender(HWND hWnd) {
	int renderID;
	{
		lock_guard<mutex> guard(renders);
		renderID = ++lastRenderID;
		activeRenders++;
	}

	Render R(renderID);

	int width = R.S2.get_width();
	int height = R.S2.get_height();

	{
		//Printing information before the render
		double_c center = R.S2.get_center();
		double_c juliaSeed = R.S2.get_juliaSeed();
		cout << "-----New Render-----" << endl;
		cout << "renderID: " << renderID << endl;
		cout << "Formula: " << R.S2.get_formula().name << endl;
		cout << "width: " << width << endl;
		cout << "height: " << height << endl;
		cout << "center: " << real(center) << " + " << imag(center) << " * I" << endl;
		cout << "xrange: " << R.S2.get_x_range() << endl;
		cout << "yrange: " << R.S2.get_y_range() << endl;
		cout << "zoom: " << R.S2.getZoomLevel() << endl;
		cout << "Julia: ";
		if (R.S2.get_julia()) cout << "Yes, with seed " << real(juliaSeed) << " + " << imag(juliaSeed) << " * I" << endl;
		else cout << "no" << endl;
	}

	thread refreshThread(refreshDuringRender, ref(R));
	R.execute(); //after this the render is done

	{
		lock_guard<mutex> guard(drawingBitmap);
		if (lastRenderID == renderID) { //which means: if the render was not cancelled
			HDC hdc = GetDC(hWndMain);
			DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY);
			ReleaseDC(hWndMain, hdc);
			string elapsedString = to_string(R.getElapsedTime()) + " s";
			SendMessageA(statusBar, SB_SETTEXTA, 0, (LPARAM)("100%"));
			SendMessageA(statusBar, SB_SETTEXTA, 1, (LPARAM)(elapsedString.c_str()));
			firstPaint = false;
		}
	}

	{
		//Printing information after the render
		int pixelGroupings = R.pixelGroupings;
		int guessedPixels = R.guessedPixelCount;

		string elapsedString = to_string(R.getElapsedTime()) + " s";
		cout << "Elapsed time: " << elapsedString << endl;
		cout << "computed iterations: " << R.computedIterations << endl;
		cout << "iterations per second: " << ((long long)(R.computedIterations / R.getElapsedTime()) / 1000000.0) << " M" << endl;
		cout << "used threads: " << usedThreads / 2 << endl; // divide by 2 because each thread is counted when created and when destroyed by Render::addToThreadcount
		cout << "guessedPixelCount: " << R.guessedPixelCount << " / " << width * height << " = " << (double)R.guessedPixelCount / (width*height) << endl;
		cout << "calculatedPixelCount" << R.calculatedPixelCount << " / " << width * height << " = " << (double)R.calculatedPixelCount / (width*height) << endl;
		cout << "pixelGroupings: " << R.pixelGroupings << endl;
	}

	usedThreads = 0;
	refreshThread.join();
	{
		lock_guard<mutex> guard(renders);
		activeRenders--;
	}
	return;
}

void recalculate() {
	thread t(createNewRender, hWndMain);
	t.detach();
	PostMessageA(hOptions, CUSTOM_REFRESH, 0, 0);
}

void tests();

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	case WM_CREATE: {
		tests();

		cout << setprecision(21); //precision when printing doubles

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
			USING_AVX = (xcrFeatureMask & 0x6) == 0x6;
		}
		cout << "using AVX: " << (USING_AVX ? "Yes" : "No") << endl;

		hWndMain = hWnd;
		AddMenus(hWnd);
		statusBar = CreateWindowExW(0, STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);
		int widths[4] = { 80, 180, 380, -1 };
		SendMessage(statusBar, SB_SETPARTS, 4, (LPARAM)&widths);

		initializeProgramData();
		recalculate();

		break;
	}
	case WM_SIZE: {
		SendMessage(statusBar, message, wParam, lParam);
		break;
	}
	case WM_COMMAND: {
		//menu selection
		bool fractalTypeChange = S.changeFormula(LOWORD(wParam));
		if (fractalTypeChange) {
			S.setCenterAndZoom(0, 0);
			recalculate();
			break;
		}

		switch (LOWORD(wParam)) {//other menu buttons
		case RESET:

			S.setGradientSpeed(17.0);
			S.setGradientOffset(0);
			S.setMaxIters(4600);
			while (S.removeInflection(0, 0));
			S.setCenterAndZoom(0, 0);
			S.setInflectionZoomLevel();
			recalculate();
			/*
			 //benchmark
			S.setCenterAndZoom(-1.211137349 + 0.133899745*I, 20);
			S.setGradientSpeed(17.0);
			S.setGradientOffset(0);g
			S.setMaxIters(150000);
			while (S.removeInflection(0, 0));
			recalculate();
			*/
			break;
		case TOGGLE_JULIA:
			S.toggleJulia();
			recalculate();
			break;
		case CHANGE_TRANSFORMATION:
			S.changeTransformation();
			S.setCenterAndZoom(0, 0);
			recalculate();
			break;
		case WINDOW_OPTIONS:
			//SendMessage(hOptions, WM_DESTROY, NULL, NULL); //destroy currently active options window? doesn't work
			createOptionsWindow();
			break;
		case WINDOW_JSON:
			createJsonWindow();
			break;
		case VIEW_GUESSED_PIXELS:
			refreshBitmap(true);
			break;
		case SAVE_IMAGE: {
			string filename;
			if (BrowseFile(hWnd, FALSE, "Save bitmap", "Bitmap\0*.bmp\0\0", filename)) {

				int screenWidth = S.get_screenWidth();
				int screenHeight = S.get_screenHeight();
				int width = S.get_width();
				int height = S.get_height();

				PICTDESC pictdesc = {};
				pictdesc.cbSizeofstruct = sizeof(pictdesc);
				pictdesc.picType = PICTYPE_BITMAP;

				HBITMAP smallerBMP;

				if (screenHeight != height || screenWidth != width) {
					//Anti-aliasing is used. A shrunk bitmap needs to be used.

					UINT * saveBitmapPixels;
					HDC hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
					BITMAPINFO RGB32BitsBITMAPINFO;
					ZeroMemory(&RGB32BitsBITMAPINFO, sizeof(BITMAPINFO));
					RGB32BitsBITMAPINFO.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
					RGB32BitsBITMAPINFO.bmiHeader.biWidth = screenWidth;
					RGB32BitsBITMAPINFO.bmiHeader.biHeight = screenHeight;
					RGB32BitsBITMAPINFO.bmiHeader.biPlanes = 1;
					RGB32BitsBITMAPINFO.bmiHeader.biBitCount = 32;
					smallerBMP = CreateDIBSection(
						hdc,
						(BITMAPINFO *)&RGB32BitsBITMAPINFO,
						DIB_RGB_COLORS,
						(void**)&saveBitmapPixels,
						NULL, 0
					);

					hdc = GetDC(hWndMain);

					HDC smaller_hdc = CreateCompatibleDC(hdc);
					SelectObject(smaller_hdc, smallerBMP);

					HDC truesize_hdc = CreateCompatibleDC(hdc);
					SelectObject(truesize_hdc, screenBMP);

					SetStretchBltMode(smaller_hdc, HALFTONE);
					StretchBlt(smaller_hdc, 0, 0, S.get_screenWidth(), S.get_screenHeight(), truesize_hdc, 0, 0, S.get_width(), S.get_height(), SRCCOPY);

					pictdesc.bmp.hbitmap = smallerBMP;

					DeleteDC(smaller_hdc);
					DeleteDC(truesize_hdc);
					ReleaseDC(hWndMain, hdc);
				}
				else {
					pictdesc.bmp.hbitmap = screenBMP;
				}

				CComPtr<IPicture> picture;
				OleCreatePictureIndirect(&pictdesc, __uuidof(IPicture), FALSE, (LPVOID*)&picture);

				//save to file
				CComPtr<IPictureDisp> disp;
				picture->QueryInterface(&disp);
				OleSavePictureFile(disp, CComBSTR(filename.c_str()));

				DeleteObject(smallerBMP);
			}
			break;
		}
		case SAVE_PARAMETERS: {
			string filename;
			if (BrowseFile(hWnd, FALSE, "Save parameters", "Parameters\0*.efp\0\0", filename)) {

				S.readWriteParameters(false, filename);
			}
			break;
		}
		case LOAD_PARAMETERS: {
			string filename;
			if (BrowseFile(hWnd, TRUE, "Load parameters", "Parameters\0*.efp\0\0", filename)) {

				S.readWriteParameters(true, filename);
				recalculate();
			}
			break;
		}
		case CANCEL_RENDER: {
			cancelRender();
			break;
		}
		case QUIT:
			PostQuitMessage(0);
			break;
		}
		break;
	}
	case WM_PAINT: {
		if (firstPaint) break;
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOWTEXT + 1));
		DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_MOUSEMOVE: {
		int oversampling = S.get_oversampling();
		int width = S.get_width();
		int height = S.get_height();
		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		int xPos = point.x * oversampling;
		int yPos = point.y * oversampling;
		if (xPos < 0 || xPos > width || yPos < 0 || yPos > height) {
			return 0;
		}

		iterData thisPixel = getIterData(xPos, yPos);
		double_c thisComplexNumber = map(xPos, yPos);
		string complexNumber = to_string(real(thisComplexNumber)) + " + " + to_string(imag(thisComplexNumber)) + "* I";
		string iterationData = "iters: " + to_string(thisPixel.iterationCount);
		SendMessageA(statusBar, SB_SETTEXTA, 2, (LPARAM)(complexNumber.c_str()));
		SendMessageA(statusBar, SB_SETTEXTA, 3, (LPARAM)(iterationData.c_str()));

		break;
	}
	case WM_MOUSEWHEEL: {
		//zoom action
		int oversampling = S.get_oversampling();
		int width = S.get_width();
		int height = S.get_height();
		double_c center = S.get_center();

		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ScreenToClient(hWnd, &point);
		int xPos = point.x * oversampling;
		int yPos = point.y * oversampling;
		if (xPos < 0 || xPos > width || yPos < 0 || yPos > height) {
			return 0;
		}

		//set new parameters
		//center = (map(xPos, yPos)+(map(xPos, yPos)+center)/2)/2; //working formula, maybe can be simplified
		//the way this works is applying the known transformation for a zoom size of 2, 2 times.
		//for the other direction: 2*(2*center-map(xPos, yPos))-map(xPos, yPos) is two times the inverse transformation
		//center = map(xPos, yPos); //old method
		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (zDelta > 0) {
			S.setCenterAndZoom(
				0.75*(map(xPos, yPos)) + 0.25*center,
				S.getZoomLevel() + 2
			);
		}
		else {
			S.setCenterAndZoom(
				4.*center - 3.*(map(xPos, yPos)),
				S.getZoomLevel() - 2
			);
		}

		//generate preview bitmap
		BITMAP Bitmap;
		HDC screenHDC = CreateCompatibleDC(NULL);
		GetObject(screenBMP, sizeof(BITMAP), (LPSTR)&Bitmap);
		SelectObject(screenHDC, screenBMP);

		if (zDelta > 0) {
			StretchBlt(screenHDC, 0, 0, width, height, screenHDC, xPos - xPos / 4, yPos - yPos / 4, width / 4, height / 4, SRCCOPY); //stretch the screen bitmap
		}
		else {
			SetStretchBltMode(screenHDC, HALFTONE);
			StretchBlt(screenHDC, xPos - xPos / 4, yPos - yPos / 4, width / 4, height / 4, screenHDC, 0, 0, width, height, SRCCOPY);
		}
		DeleteDC(screenHDC);

		recalculate();
		break;
	}
	case WM_LBUTTONUP: {
		//create inflection
		int oversampling = S.get_oversampling();
		int xPos = GET_X_LPARAM(lParam) * oversampling;
		int yPos = GET_Y_LPARAM(lParam) * oversampling;
		bool added = S.addInflection(xPos, yPos);

		if (added) {
			S.printInflections();
			recalculate();
		}
		break;
	}
	case WM_RBUTTONUP: {
		//remove inflection
		int oversampling = S.get_oversampling();
		int xPos = GET_X_LPARAM(lParam) * oversampling;
		int yPos = GET_Y_LPARAM(lParam) * oversampling;
		bool removed = S.removeInflection(xPos, yPos);

		int additionalInflections = S.get_width() % 100;
		cout << "using " << additionalInflections << " additional inflections" << endl;
		for (int i = 1; i < additionalInflections; i++) {
			S.removeInflection(xPos, yPos);
		}

		if (removed) {
			S.printInflections();
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
	switch (S.get_oversampling()) {
	case 1:
		return optionsRadioAA_1;
	case 2:
		return optionsRadioAA_2;
	case 3:
		return optionsRadioAA_3;
	case 4:
		return optionsRadioAA_4;
	case 6:
		return optionsRadioAA_6;
	case 8:
		return optionsRadioAA_8;
	case 12:
		return optionsRadioAA_12;
	case 16:
		return optionsRadioAA_16;
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
		optionsElt(optionGradientSpeedTrackbarCoarse) = CreateTrackbar(hOptions, 0, 100, 60, 85, 400, 30, optionGradientSpeedTrackbarCoarse);
		optionsElt(optionGradientSpeedTextFine) = CreateWindowExA(0, "STATIC", "Fine", WS_VISIBLE | WS_CHILD, 0, 115, 110, 30, hOptions, (HMENU)optionGradientSpeedTextFine, hInst, NULL);
		optionsElt(optionGradientSpeedTrackbarFine) = CreateTrackbar(hOptions, 0, 100, 60, 115, 400, 30, optionGradientSpeedTrackbarFine);
		optionsElt(optionGradientOffsetText) = CreateWindowExA(0, "STATIC", "Offset", WS_VISIBLE | WS_CHILD, 0, 145, 130, 30, hOptions, (HMENU)optionGradientOffsetText, hInst, NULL);
		optionsElt(optionGradientOffsetTrackbar) = CreateTrackbar(hOptions, 0, 100, 60, 145, 400, 30, optionGradientOffsetTrackbar);
		double gradientSpeed = S.get_gradientSpeed();
		SendMessage(optionsElt(optionGradientSpeedTrackbarCoarse), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)((int)gradientSpeed));
		SendMessage(optionsElt(optionGradientSpeedTrackbarFine), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(100 * (gradientSpeed - (int)gradientSpeed)));
		SendMessage(optionsElt(optionGradientOffsetTrackbar), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(100 * S.get_gradientOffset()));

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
		assert(S.get_oversampling() == S.get_height() / S.get_screenHeight());
		//cout << "Warning: screen width and height don't match: widthxheight vs screenwidthxscreenheight:   " << width << "x" << height << " vs " << screenWidth << "x" << screenHeight << endl;
		SendMessage(optionsElt(getOverSampleIdentifier()), BM_SETCHECK, BST_CHECKED, 0);

		//set gradient speed
		SetDlgItemTextA(hOptions, optionGradientSpeed, to_string(S.get_gradientSpeed()).c_str());

		//set width, height
		SetDlgItemTextA(hOptions, optionHeight, to_string(S.get_screenHeight()).c_str());
		SetDlgItemTextA(hOptions, optionWidth, to_string(S.get_screenWidth()).c_str());

		//Set zoom level
		SetDlgItemTextA(hOptions, optionZoomLevel, to_string(S.getZoomLevel()).c_str());

		//Set max iters
		SetDlgItemTextA(hOptions, optionMaxIters, to_string(S.get_maxIters()).c_str());
		break;
	}
	case WM_HSCROLL: {
		SendMessage(hOptions, CUSTOM_REFRESH, 0, 0);
		bool refreshBitmapRequired = false;
		if (lParam == (LPARAM)optionsElt(optionGradientSpeedTrackbarCoarse) || lParam == (LPARAM)optionsElt(optionGradientSpeedTrackbarFine)) {
			int posCoarse = SendMessage(optionsElt(optionGradientSpeedTrackbarCoarse), TBM_GETPOS, 0, 0);
			int posFine = SendMessage(optionsElt(optionGradientSpeedTrackbarFine), TBM_GETPOS, 0, 0);
			double newGradientSpeed = posCoarse + 0.01*(double)posFine;
			refreshBitmapRequired = S.setGradientSpeed(newGradientSpeed);
		}
		else if (lParam == (LPARAM)optionsElt(optionGradientOffsetTrackbar)) {
			double newGradientOffset = 0.01 * SendMessage(optionsElt(optionGradientOffsetTrackbar), TBM_GETPOS, 0, 0);
			cout << "new gradientOffset: " << newGradientOffset << endl;
			refreshBitmapRequired = S.setGradientOffset(newGradientOffset);
		}
		if (refreshBitmapRequired)
			refreshBitmap(false);
		break;
	}
	case WM_COMMAND: {
		if (wParam == optionSetInflectionZoom) {
			S.setInflectionZoomLevel();
		}
		if (wParam == optionApply || wParam == optionOk) {
			bool recalcNeeded = false;

			//change iteration count:
			char aMaxIters[16]; GetDlgItemTextA(hOptions, optionMaxIters, aMaxIters, sizeof(aMaxIters));
			int newMaxIters = atoi(aMaxIters);
			recalcNeeded |= S.setMaxIters(newMaxIters);

			//change gradient speed:
			char aGradientSpeed[16]; GetDlgItemTextA(hOptions, optionGradientSpeed, aGradientSpeed, sizeof(aGradientSpeed));
			double newGradientSpeed = strtod(aGradientSpeed, NULL);
			S.setGradientSpeed(newGradientSpeed);
			refreshBitmap(false);

			//change zoom level:
			char aZoomLevel[16]; GetDlgItemTextA(hOptions, optionZoomLevel, aZoomLevel, sizeof(aZoomLevel));
			double newZoomLevel = strtod(aZoomLevel, NULL);
			recalcNeeded |= S.setCenterAndZoom(S.get_center(), newZoomLevel);

			//resize:
			char aWidth[16]; GetDlgItemTextA(hOptions, optionWidth, aWidth, sizeof(aWidth));
			char aHeight[16]; GetDlgItemTextA(hOptions, optionHeight, aHeight, sizeof(aHeight));
			int newScreenWidth = atoi(aWidth);
			int newScreenHeight = atoi(aHeight);

			//change newOversampling
			int newOversampling = S.get_oversampling();
			if (IsDlgButtonChecked(hOptions, optionsRadioAA_1) == BST_CHECKED) newOversampling = 1;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_2) == BST_CHECKED) newOversampling = 2;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_3) == BST_CHECKED) newOversampling = 3;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_4) == BST_CHECKED) newOversampling = 4;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_6) == BST_CHECKED) newOversampling = 6;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_8) == BST_CHECKED) newOversampling = 8;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_12) == BST_CHECKED) newOversampling = 12;
			else if (IsDlgButtonChecked(hOptions, optionsRadioAA_16) == BST_CHECKED) newOversampling = 16;

			recalcNeeded |= S.resize(newOversampling, newScreenWidth*newOversampling, newScreenHeight*newOversampling, newScreenWidth, newScreenHeight);

			if (recalcNeeded) {
				recalculate();
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
		string textFieldContent = regex_replace(S.toJson(), regex("\n"), "\r\n"); //this is necessary because the text field doesn't display newlines otherwise
		SetDlgItemTextA(hJson, JsonText, textFieldContent.c_str());
		break;
	}
	case WM_COMMAND: {
		cout << "WM_COMMAND " << (int)wParam << endl;
		if (wParam == JsonRefresh) {
			SendMessage(hJson, CUSTOM_REFRESH, 0, 0);
		}
		if (wParam == JsonApply || wParam == JsonOk) {
			//allocating 1 MB of memory for the JSON string typed by the user, which should usually be enough. This should be improved when possible.
			int jsonCharsLimit = 1000000;
			char* aJson = (char*)calloc(jsonCharsLimit, sizeof(char));
			GetDlgItemTextA(hJson, JsonText, aJson, jsonCharsLimit);
			string json(aJson);
			if (!S.fromJson(json)) cout << "The JSON that you entered could not be parsed.";
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

void tests() {
	return;
}