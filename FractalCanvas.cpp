//Visual Studio requires this for no clear reason
#include "stdafx.h"

//windows
#include <windows.h>

//standard library
#include <iostream>
#include <cassert>
#include <thread>

//this program
#include "common.cpp"
#include "FractalParameters.cpp"

#include "windows_util.cpp"


#ifndef _FractalCanvas_
#define _FractalCanvas_

using namespace std;

const long long MAXIMUM_BITMAP_SIZE = 2147483648; // 2^31

HBITMAP screenBMP;

inline UINT rgbColorAverage(UINT u1, UINT u2, double ratio) {
	assert(ratio >= 0);
	assert(ratio <= 1);
	return RGB(
		(unsigned char)((GetRValue(u1))*(1 - ratio) + (GetRValue(u2))*ratio) | 1,
		(unsigned char)((GetGValue(u1))*(1 - ratio) + (GetGValue(u2))*ratio) | 1,
		(unsigned char)((GetBValue(u1))*(1 - ratio) + (GetBValue(u2))*ratio) | 1
	);
}

struct IterData {
	int iterationCount;
	bool guessed;
	bool inMinibrot;
};

class FractalCanvas {
public:
	IterData* iters;
	uint* ptPixels; //bitmap colors representing the iteration data
	int* ptPixelsMetadata; //represents how many calculated pixels the color is an average of
	FractalParameters S;
	int lastRenderID;
	int activeRenders;
	int lastBitmapRenderID;
	int activeBitmapRenders;
	unsigned int number_of_threads;

	int cancelRender() {
		/*
			This causes active renders to stop. It doesn't actively cancel anything, rather it's the render itself that checks, every so often, whether it should stop.
			Changing the lastRenderID here doesn't require a lock. If it happens that another threads is also changing lastRenderID, that must be because of another cancellation or new render, which already has the desired effect.
		*/
		int renderID = ++lastRenderID;
		return renderID;
	}

	template <int formula_identifier, bool guessing, bool use_avx, bool julia>
	void createNewRenderTemplated(bool);

	void createNewRender(bool);

	FractalCanvas() {}
	FractalCanvas(FractalParameters& parameters, unsigned int number_of_threads) {
		cout << "constructing FractalCanvas" << endl;
		lastRenderID = 0;
		activeRenders = 0;
		lastBitmapRenderID = 0;
		activeBitmapRenders = 0;
		this->number_of_threads = number_of_threads;

		S.initialize();
		/*
			S.resize sets the width and height of S to 0. Having 0 pixels is consistent with the fact that no memory has yet been allocated. This is required for changeParameters to work correctly.
		*/
		S.resize(1, 0, 0);
		changeParameters(parameters);
		cout << "canvas constructed with dimensions " << S.get_width() << "x" << S.get_height() << endl;
	}
	~FractalCanvas() {
		cout << "deleting fractalcanvas" << endl;
	}

	bool changeParameters(FractalParameters& newS) {
		bool success;
		resize(newS.get_oversampling(), newS.get_screenWidth(), newS.get_screenHeight(), success);
		if (success)		S = newS;
		else				cout << "Changing the FractalParameters of the FractalCanvas failed." << endl;
		return success;
	}

	inline uint gradient(int iterationCount) {
		int number_of_colors = S.gradientColors.size();
		double gradientPosition = (iterationCount + S.get_gradientOffsetTerm()) * S.get_gradientSpeedFactor();
		uint asInt = (uint)gradientPosition;
		/*
		uint previousColor = gradientColors[asInt & (NUMBER_OF_COLORS - 1)];
		uint nextColor = gradientColors[(asInt + 1) & (NUMBER_OF_COLORS - 1)];
		*/
		uint previousColor = S.gradientColors[asInt % number_of_colors];
		uint nextColor = S.gradientColors[(asInt + 1) % number_of_colors];
		double ratio = gradientPosition - asInt;
		assert(ratio >= 0);
		assert(ratio <= 1);

		return rgbColorAverage(previousColor, nextColor, ratio);
	}

	/*
		Returns true when the size has changed, false otherwise. success will indicate if there were errors. (If there are no changes, that's not an error.)
	*/
	bool resize(int newOversampling, int newScreenWidth, int newScreenHeight, bool& success) {
		success = true;
		int oldWidth = S.get_width();
		int oldHeight = S.get_height();
		int newWidth = newScreenWidth * newOversampling;
		int newHeight = newScreenHeight * newOversampling;
		long long bitmap_size = ((long long)newScreenWidth) * newScreenHeight;

		bool realloc_bitmap = newScreenWidth != S.get_screenWidth() || newScreenHeight != S.get_screenHeight();
		bool realloc_fractalcanvas = newWidth != oldWidth || newHeight != oldHeight;

		if (!realloc_bitmap && !realloc_fractalcanvas) {
			cout << "entered resize. The resolutions remain the same. Nothing happens." << endl;
			return false;
		}
		else {
			if ( ! (
				newWidth > 0
				&& newHeight > 0
				&& bitmap_size <= MAXIMUM_BITMAP_SIZE)
			) {
				cout << "Size outside of allowed range." << endl;
				cout << "width: " << newWidth << "  " << "height: " << newHeight << endl;
				cout << "maximum allowed size: " << MAXIMUM_BITMAP_SIZE << endl;
				MessageBox(NULL,
					_T("Size outside of allowed range"),
					_T("Problem"),
					NULL
				);
				success = false;
				return false;
			}
		}

		cout << "resize happens" << endl;

		{
			cancelRender(); //This will stop active renders as soon as possible which will release the mutex locks that are needed here.
			lock_guard<mutex> guard(renders);
			lock_guard<mutex> guard2(renderingBitmap);

			S.resize(newOversampling, newScreenWidth, newScreenHeight);

			if (realloc_fractalcanvas) {
				cout << "reallocating fractalcanvas" << endl;
				int size = S.get_width() * S.get_height();
				if (oldWidth * oldHeight != 0) free(iters);
				iters = (IterData*)malloc(size * sizeof(IterData));
				cout << "reallocated fractalcanvas, size: " << size << endl;
			}

			if (realloc_bitmap) {
				cout << "reallocating ptPixelMetadata" << endl;
				int size = S.get_screenWidth() * S.get_screenHeight();
				if (oldWidth * oldHeight != 0) free(ptPixelsMetadata);
				ptPixelsMetadata = (int*)malloc(size * sizeof(int));
				cout << "reallocated ptPixelMetadata, size: " << size << endl;

				cout << "reallocating bitmap" << endl;
				DeleteObject(screenBMP);
				HDC hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
				BITMAPINFO RGB32BitsBITMAPINFO;
				ZeroMemory(&RGB32BitsBITMAPINFO, sizeof(BITMAPINFO));
				RGB32BitsBITMAPINFO.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				RGB32BitsBITMAPINFO.bmiHeader.biWidth = newScreenWidth;
				RGB32BitsBITMAPINFO.bmiHeader.biHeight = newScreenHeight;
				RGB32BitsBITMAPINFO.bmiHeader.biPlanes = 1;
				RGB32BitsBITMAPINFO.bmiHeader.biBitCount = 32;
				screenBMP = CreateDIBSection(
					hdc,
					(BITMAPINFO *)&RGB32BitsBITMAPINFO,
					DIB_RGB_COLORS,
					(void**)&ptPixels,
					NULL, 0
				);
				cout << "reallocated bitmap" << endl;
			}
		}
		return true;
	}

	void refreshDuringBitmapRender(int bitmapRenderID) {
		this_thread::sleep_for(chrono::milliseconds(70));
		if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " awakened" << endl;
		int screenWidth = S.get_screenWidth();
		int screenHeight = S.get_screenHeight();

		while (lastBitmapRenderID == bitmapRenderID && activeBitmapRenders != 0) {
			if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " waiting for lock drawingBitmap" << endl;
			{
				lock_guard<mutex> guard(drawingBitmap);
				if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " has lock drawingBitmap" << endl;
				if (lastBitmapRenderID == bitmapRenderID && activeRenders == 0) { //if a render is active, it also has a refreshthread
					HDC hdc = GetDC(hWndMain);
					DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY, screenWidth, screenHeight);
					ReleaseDC(hWndMain, hdc);
				}
				else {
					if(debug) if(activeRenders == 0) cout << "refreshDuringBitmapRender " << bitmapRenderID << " doesn't draw because the render was cancelled" << endl;
				}
			}
			if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " released lock drawingBitmap" << endl;
			this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if(debug) cout << "refreshDuringBitmapRender thread " << bitmapRenderID << " ended" << endl;
	}

	inline int pixelIndex_of_pixelXY(int x, int y) {
		//returns the index in ptPixels of (x, y) in the bitmap
		return S.get_screenWidth()*(S.get_screenHeight() - y - 1) + x;
	}

	inline int pixelIndex_of_itersXY(int x, int y) {
		//returns the corresponding index in ptPixels of (x, y) in the fractal canvas
		int oversampling = S.get_oversampling();
		return pixelIndex_of_pixelXY(x / oversampling, y / oversampling);
	}

	inline int itersIndex_of_itersXY(int x, int y) {
		//returns the index in iters of (x, y) in the fractalcanvas
		int screenWidth = S.get_screenWidth();
		int screenHeight = S.get_screenHeight();
		int oversampling = S.get_oversampling();
		int samples = oversampling * oversampling;
		int dx = x % oversampling;
		int dy = y % oversampling;
		int _x = x / oversampling;
		int _y = y / oversampling;
		return (_x * screenHeight + _y) * samples + (dy * oversampling) + dx;
	}

	inline int getIterationcount(int x, int y) {
		return iters[itersIndex_of_itersXY(x,y)].iterationCount;
	}

	inline IterData getIterData(int x, int y) {
		return iters[itersIndex_of_itersXY(x,y)];
	}
	
	void renderBitmapRect(bool highlight_guessed, int xfrom, int xto, int yfrom, int yto, int bitmapRenderID) {
		//lock_guard<mutex> guard(test);
		int width = S.get_width();
		int height = S.get_height();
		int screenWidth = S.get_screenWidth();
		int screenHeight = S.get_screenHeight();
		int oversampling = S.get_oversampling();
		int samples = oversampling * oversampling;
		IterData it;

		if (highlight_guessed) {
			for (int px=xfrom; px<xto; px++) {
				for (int py=yfrom; py<yto; py++) {

					int itersStartIndex = (px * screenHeight + py) * samples;

					uint sumR=0, sumG=0, sumB=0;
					uint color;
					IterData it;

					for (int i=0; i<samples; i++) {
						it = iters[itersStartIndex + i];
						if (it.inMinibrot && !it.guessed) color = RGB(0, 0, 255);
						else if (it.inMinibrot)           color = RGB(255, 0, 0);
						else if (it.guessed)              color = RGB(0, 255, 0);
						else                              color = gradient(it.iterationCount);
						sumR += GetRValue(color);
						sumG += GetGValue(color);
						sumB += GetBValue(color);
					}
				
					ptPixels[screenWidth * (screenHeight - py - 1) + px] = RGB(
						(unsigned char)(sumR / samples),
						(unsigned char)(sumG / samples),
						(unsigned char)(sumB / samples)
					);
				}
				if (lastBitmapRenderID != bitmapRenderID) {
					if (debug) cout << "cancelling bitmap render " << bitmapRenderID << endl;
					return;
				}
			}
		}
		else {
			for (int px=xfrom; px<xto; px++) {
				for (int py=yfrom; py<yto; py++) {

					int itersStartIndex = (px * screenHeight + py) * samples;

					uint sumR=0, sumG=0, sumB=0;
					uint color;
					IterData it;

					for (int i=0; i<samples; i++) {
						it = iters[itersStartIndex + i];
						if (it.inMinibrot)                color = RGB(0, 0, 0);
						else                              color = gradient(it.iterationCount);
						sumR += GetRValue(color);
						sumG += GetGValue(color);
						sumB += GetBValue(color);
					}
				
					ptPixels[screenWidth * (screenHeight - py - 1) + px] = RGB(
						(unsigned char)(sumR / samples),
						(unsigned char)(sumG / samples),
						(unsigned char)(sumB / samples)
					);
				}
				if (lastBitmapRenderID != bitmapRenderID) {
					if (debug) cout << "Bitmap render " << bitmapRenderID << " cancelled; terminating thread" << endl;
					return;
				}
			}
		}
	}

	void renderBitmapFull(bool highlight_guessed, bool multithreading, int bitmapRenderID) {
		int screenWidth = S.get_screenWidth();
		int screenHeight = S.get_screenHeight();

		lock_guard<mutex> guard(renderingBitmap);
		//use multithreading for extra speed when there's no render active
		if (multithreading) {
			if(debug) cout << "using multiple threads for renderBitmapFull" << endl;
			int tiles = (int)(sqrt(number_of_threads)); //the number of tiles in both horizontal and vertical direction, so in total there are (tiles * tiles)

			int widthStep = screenWidth / tiles;
			int heightStep = screenHeight / tiles;

			int usingThreads = tiles * tiles;
			vector<thread> tileThreads(usingThreads);
			int createdThreads = 0;

			for (int i=0; i<tiles; i++) {
				int xfrom = i * widthStep;
				int xto;
				if (i == tiles - 1)	xto = screenWidth;	//the last tile
				else					xto = (i+1) * widthStep;
				for (int j=0; j<tiles; j++) {
					int yfrom = j * heightStep;
					int yto;
					if (j == tiles - 1)	yto = screenHeight;	//the last tile
					else					yto = (j+1) * heightStep;

					tileThreads[createdThreads++] = thread(&FractalCanvas::renderBitmapRect, this, highlight_guessed, xfrom, xto, yfrom, yto, bitmapRenderID);
				}
			}

			for (int i=0; i<createdThreads; i++)
				tileThreads[i].join();
		}
		else {
			if(debug) cout << "using 1 thread for renderBitmapFull" << endl;
			renderBitmapRect(highlight_guessed, 0, screenWidth, 0, screenHeight, bitmapRenderID);
		}
	}

	void createNewBitmapRender(bool headless, bool highlight_guessed) {
		if(debug) cout << "enters renderBitmapFull" << endl;
		int bitmapRenderID;
		{
			lock_guard<mutex> guard(renderingBitmap);
			bitmapRenderID = ++lastBitmapRenderID;
			activeBitmapRenders++;
		}
		if(debug) cout << "performing bitmap render " << bitmapRenderID << endl;

		if (headless) {
			renderBitmapFull(highlight_guessed, true, bitmapRenderID);
			{
				lock_guard<mutex> guard(renderingBitmap);
				activeBitmapRenders--;
			}
		}
		else {
			thread refreshThread(&FractalCanvas::refreshDuringBitmapRender, this, bitmapRenderID);
			bool multithreading = activeRenders == 0;
			renderBitmapFull(highlight_guessed, multithreading, bitmapRenderID);
			{
				lock_guard<mutex> guard(renderingBitmap);
				activeBitmapRenders--;
			}
			{
				lock_guard<mutex> guard(drawingBitmap);
				if (lastBitmapRenderID == bitmapRenderID) {
					HDC hdc = GetDC(hWndMain);
					DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY, S.get_screenWidth(), S.get_screenHeight());
					ReleaseDC(hWndMain, hdc);
				}
			}
			refreshThread.join();
		}
	}
};

#endif