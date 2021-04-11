#ifndef FRACTALCANVAS_H
#define FRACTALCANVAS_H

//standard library
#include <iostream>
#include <cassert>
#include <thread>

//this program
#include "common.cpp"
#include "FractalParameters.cpp"

using namespace std;

const uint64 MAXIMUM_BITMAP_SIZE = 2147483648; // 2^31

/*
	This class uses one integer as as data container to store multiple properties of a calculated point as follows:
	first 30 bits: iterationcount (integer)
	31st bit: inMinibrot (boolean)
	32nd bit: guessed (boolean)

	The reason for this construction is that storing those values normally uses two times as much memory. The corresponding struct would be:
	struct IterData {
		uint iterationCount; //4 bytes
		bool inMinibrot; //1 byte
		bool guessed; //1 byte
	};
	A bool uses 1 byte of memory (8 times as much as strictly needed). Together that's 6 bytes for the struct, but the compiler rounds that to a multiple of 4, making the struct cost 8 bytes.

	So by using 2 bits of the data to store the boolean values, memory usage is reduced by half at the cost of 2 bits of precision for the iterationcount, which means the maximum value of the iterationcount is 2^30 = 1,073,741,824.

	The iterationcount limit can be easily raised to 2^62 by using a uint64 instead of a uint, providing a much higher limit than the struct above while using the same amount of memory.
*/
class IterData {
	uint data;
public:
	IterData(uint iterationCount, bool guessed, bool inMinibrot) {
		data = iterationCount << 2;
		if (guessed)
			data = data | 0b1;
		if (inMinibrot) {
			data = data | 0b10;
		}
	}
	inline uint iterationCount() {
		return data >> 2;
	}
	inline bool guessed() {
		return data & 0b1;
	}
	inline bool inMinibrot() {
		return data & 0b10;
	}
};

class FractalCanvas {
public:
	IterData* iters;
	ARGB* ptPixels; //bitmap colors representing the iteration data
	FractalParameters S;
	uint lastRenderID;
	uint activeRenders;
	uint lastBitmapRenderID;
	uint activeBitmapRenders;
	uint number_of_threads;
	BitmapManager* bitmapManager;

	uint cancelRender() {
		/*
			This causes active renders to stop. It doesn't actively cancel anything, rather it's the render itself that checks, every so often, whether it should stop.
			Changing the lastRenderID here doesn't require a lock. If it happens that another threads is also changing lastRenderID, that must be because of another cancellation or new render, which already has the desired effect.
		*/
		uint renderID = ++lastRenderID;
		return renderID;
	}

	template <int formula_identifier, bool guessing, bool use_avx, bool julia>
	void createNewRenderTemplated(bool);

	void createNewRender(bool);

	FractalCanvas() {} //required to declare a FractalCanvas without assigning a value

	FractalCanvas(FractalParameters& parameters, uint number_of_threads, BitmapManager& bitmapManager) {
		assert(number_of_threads > 0);
		cout << "constructing FractalCanvas" << endl;
		this->bitmapManager = &bitmapManager;
		this->lastRenderID = 0;
		this->activeRenders = 0;
		this->lastBitmapRenderID = 0;
		this->activeBitmapRenders = 0;
		this->number_of_threads = number_of_threads;
		this->iters = nullptr;
		this->ptPixels = nullptr;

		/*
			This sets the width and height to 1 and allocates memory for that size. This is to ensure that the FractalCanvas' state is consistent. Currently it's not consistent because memory has been allocated, which corresponds to a width and height of 0, which is an invalid value for many of the functions.
		*/
		resize(1,1,1);
		changeParameters(parameters);
		cout << "canvas constructed with dimensions " << S.get_width() << "x" << S.get_height() << endl;
	}
	~FractalCanvas() {
		if(debug) cout << "deleting fractalcanvas" << endl;
	}

	inline ARGB gradient(int iterationCount) {
		uint number_of_colors = S.gradientColors.size();
		double gradientPosition = (iterationCount + S.get_gradientOffsetTerm()) * S.get_gradientSpeedFactor();
		uint asInt = (uint)gradientPosition;
		ARGB previousColor = S.gradientColors[asInt % number_of_colors];
		ARGB nextColor = S.gradientColors[(asInt + 1) % number_of_colors];
		double ratio = gradientPosition - asInt;
		assert(ratio >= 0);
		assert(ratio <= 1);

		return rgbColorAverage(previousColor, nextColor, ratio);
	}

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
	
	ResizeResult resize(int newOversampling, int newScreenWidth, int newScreenHeight) {
		assert(newOversampling > 0);
		assert(newScreenWidth > 0);
		assert(newScreenHeight > 0);

		auto fractalcanvas_realloc = [&](uint64 size) {
			cout << "reallocating fractalcanvas to size: " << size << endl;
			free(iters);
			iters = (IterData*)malloc(size * sizeof(IterData));
			cout << "reallocated fractalcanvas" << endl;
		};

		auto bitmap_realloc = [&](uint width, uint height) {
			cout << "reallocating bitmap to width, height: " << width << ", " << height << endl;
			ptPixels = bitmapManager->realloc(width, height);
			cout << "reallocated bitmap" << endl;
		};

		uint64 oldWidth = S.get_width();
		uint64 oldHeight = S.get_height();
		uint64 oldScreenWidth = S.get_screenWidth();
		uint64 oldScreenHeight = S.get_screenHeight();
		uint64 oldOversampling = S.get_oversampling();
		uint64 newWidth = newScreenWidth * newOversampling;
		uint64 newHeight = newScreenHeight * newOversampling;
		uint64 bitmap_size = newScreenWidth * newScreenHeight;
		uint64 fractalcanvas_size = newWidth * newHeight;

		bool realloc_bitmap = newScreenWidth != S.get_screenWidth() || newScreenHeight != S.get_screenHeight();
		bool realloc_fractalcanvas = newWidth != oldWidth || newHeight != oldHeight;

		if (!realloc_bitmap && !realloc_fractalcanvas) {
			cout << "entered resize. The resolutions remain the same. Nothing happens." << endl;
			return {true, false, ResizeResultType::Success};
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
				return {false, false, ResizeResultType::OutOfRangeError};
			}
		}

		cout << "resize happens" << endl;

		ResizeResultType res = ResizeResultType::Success;
		bool success = true;
		bool changed = true;

		{
			cancelRender(); //This will stop active renders as soon as possible which will release the mutex locks that are needed here.
			lock_guard<mutex> guard(renders);
			lock_guard<mutex> guard2(renderingBitmap);

			S.resize(newOversampling, newScreenWidth, newScreenHeight);

			if (realloc_fractalcanvas) {
				fractalcanvas_realloc(fractalcanvas_size);
			}
			if (realloc_bitmap) {
				bitmap_realloc(newScreenWidth, newScreenHeight);
			}

			if (iters == nullptr || ptPixels == nullptr) {
				//Allocating memory failed.
				res = ResizeResultType::MemoryError;
				success = false;

				// Try to get back the old size
				S.resize(oldOversampling, oldScreenWidth, oldScreenHeight);

				if (realloc_fractalcanvas) {
					fractalcanvas_realloc(oldWidth * oldHeight);
				}
				if (realloc_bitmap) {
					bitmap_realloc(oldScreenWidth, oldScreenHeight);
				}

				if (iters != nullptr && ptPixels != nullptr) {
					changed = false;
					cout << "Allocating memory failed. The previous resolution has been restored." << endl;
				}
				else {
					//As a last resort, change the image size to 1x1. This can't fail in any reasonable situation.
					S.resize(1,1,1);
					fractalcanvas_realloc(1);
					bitmap_realloc(1,1);
					changed = true;
					cout << "Allocating memory failed. The resolution has changed to 1x1." << endl;
				}
			}
		}
		return {success, changed, res};
	}

	ResizeResult changeParameters(FractalParameters& newS) {
		ResizeResult res = resize(newS.get_oversampling(), newS.get_screenWidth(), newS.get_screenHeight());
		if (res.success)	 S = newS;
		else	             cout << "Changing the FractalParameters of the FractalCanvas failed." << endl;
		return res;
	}

	void refreshDuringBitmapRender(int bitmapRenderID) {
		this_thread::sleep_for(chrono::milliseconds(70));
		//if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " awakened" << endl;
		int screenWidth = S.get_screenWidth();
		int screenHeight = S.get_screenHeight();

		while (lastBitmapRenderID == bitmapRenderID && activeBitmapRenders != 0) {
			//if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " waiting for lock drawingBitmap" << endl;
			{
				lock_guard<mutex> guard(drawingBitmap);
				//if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " has lock drawingBitmap" << endl;
				if (lastBitmapRenderID == bitmapRenderID && activeRenders == 0) { //if a render is active, it also has a refreshthread
					bitmapManager->draw();
				}
				else {
					//if(debug) if(activeRenders == 0) cout << "refreshDuringBitmapRender " << bitmapRenderID << " doesn't draw because the render was cancelled" << endl;
				}
			}
			//if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " released lock drawingBitmap" << endl;
			this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		//if(debug) cout << "refreshDuringBitmapRender thread " << bitmapRenderID << " ended" << endl;
	}

	inline uint pixelIndex_of_pixelXY(uint x, uint y) {
		//returns the index in ptPixels of (x, y) in the bitmap
		assert(x >= 0); assert(x < S.get_screenWidth());
		assert(y >= 0); assert(y < S.get_screenHeight());
		return S.get_screenWidth()*(S.get_screenHeight() - y - 1) + x;
	}

	inline uint pixelIndex_of_itersXY(uint x, uint y) {
		//returns the corresponding index in ptPixels of (x, y) in the fractal canvas
		assert(x >= 0); assert(x < S.get_width());
		assert(y >= 0); assert(y < S.get_height());
		int oversampling = S.get_oversampling();
		return pixelIndex_of_pixelXY(x / oversampling, y / oversampling);
	}

	inline uint itersIndex_of_itersXY(uint x, uint y) {
		assert(x >= 0); assert(x < S.get_width());
		assert(y >= 0); assert(y < S.get_height());
		//returns the index in iters of (x, y) in the fractalcanvas
		uint screenWidth = S.get_screenWidth();
		uint screenHeight = S.get_screenHeight();
		uint oversampling = S.get_oversampling();
		uint samples = oversampling * oversampling;
		uint dx = x % oversampling;
		uint dy = y % oversampling;
		uint _x = x / oversampling;
		uint _y = y / oversampling;
		return (_x * screenHeight + _y) * samples + (dy * oversampling) + dx;
	}

	inline uint getIterationcount(uint x, uint y) {
		return iters[itersIndex_of_itersXY(x,y)].iterationCount();
	}

	inline IterData getIterData(uint x, uint y) {
		return iters[itersIndex_of_itersXY(x,y)];
	}

	inline double_c map(uint xPos, uint yPos) {
		return S.map(xPos, yPos);
	}

	inline void setPixel(uint i, uint j, uint iterationCount, bool guessed, bool isInMinibrot)
	{
		assert(i >= 0 && j >= 0);
		assert(i < S.get_width() && j < S.get_height());

		IterData result(iterationCount, guessed, isInMinibrot);
		uint index = itersIndex_of_itersXY(i, j);
		iters[index] = result;
	}
	
	void renderBitmapRect(bool highlight_guessed, uint xfrom, uint xto, uint yfrom, uint yto, uint bitmapRenderID) {
		uint width = S.get_width();
		uint height = S.get_height();
		uint screenWidth = S.get_screenWidth();
		uint screenHeight = S.get_screenHeight();
		uint oversampling = S.get_oversampling();
		uint samples = oversampling * oversampling;

		assert(xfrom >= 0); assert(xfrom <= screenWidth);
		assert(xto >= xfrom); assert(xto <= screenWidth);
		assert(yfrom >= 0); assert(yfrom <= screenHeight);
		assert(yto >= yfrom); assert(yto <= screenHeight);

		if (highlight_guessed) {
			for (uint px=xfrom; px<xto; px++) {
				for (uint py=yfrom; py<yto; py++) {

					uint itersStartIndex = (px * screenHeight + py) * samples;

					uint sumR=0, sumG=0, sumB=0;
					ARGB color;

					for (uint i=0; i<samples; i++) {
						IterData it = iters[itersStartIndex + i];
						if (it.inMinibrot() && !it.guessed()) color = rgb(255, 0, 0);
						else if (it.inMinibrot())             color = rgb(0, 0, 255);
						else if (it.guessed())                color = rgb(0, 255, 0);
						else                                  color = gradient(it.iterationCount());
						sumR += getRValue(color);
						sumG += getGValue(color);
						sumB += getBValue(color);
					}
				
					ptPixels[screenWidth * (screenHeight - py - 1) + px] = rgb(
						(uint8)(sumR / samples),
						(uint8)(sumG / samples),
						(uint8)(sumB / samples)
					);
				}
				if (lastBitmapRenderID != bitmapRenderID) {
					if (debug) cout << "cancelling bitmap render " << bitmapRenderID << endl;
					return;
				}
			}
		}
		else {
			for (uint px=xfrom; px<xto; px++) {
				for (uint py=yfrom; py<yto; py++) {

					uint itersStartIndex = (px * screenHeight + py) * samples;

					uint sumR=0, sumG=0, sumB=0;
					ARGB color;
					
					for (uint i=0; i<samples; i++) {
						IterData it = iters[itersStartIndex + i];
						if (it.inMinibrot())                color = rgb(0, 0, 0);
						else                                color = gradient(it.iterationCount());
						sumR += getRValue(color);
						sumG += getGValue(color);
						sumB += getBValue(color);
					}
				
					ptPixels[screenWidth * (screenHeight - py - 1) + px] = rgb(
						(uint8)(sumR / samples),
						(uint8)(sumG / samples),
						(uint8)(sumB / samples)
					);
				}
				if (lastBitmapRenderID != bitmapRenderID) {
					if (debug) cout << "Bitmap render " << bitmapRenderID << " cancelled; terminating thread" << endl;
					return;
				}
			}
		}
	}

	void renderBitmapFull(bool highlight_guessed, bool multithreading, uint bitmapRenderID) {
		uint screenWidth = S.get_screenWidth();
		uint screenHeight = S.get_screenHeight();

		lock_guard<mutex> guard(renderingBitmap);
		//use multithreading for extra speed when there's no render active
		if (multithreading) {
			if(debug) cout << "using multiple threads for renderBitmapFull" << endl;
			uint tiles = (uint)(sqrt(number_of_threads)); //the number of tiles in both horizontal and vertical direction, so in total there are (tiles * tiles)

			uint widthStep = screenWidth / tiles;
			uint heightStep = screenHeight / tiles;
			
			uint usingThreads = tiles * tiles;
			vector<thread> tileThreads(usingThreads);
			uint createdThreads = 0;

			for (uint i=0; i<tiles; i++) {
				uint xfrom = i * widthStep;
				uint xto;
				if (i == tiles - 1)	xto = screenWidth;	//the last tile
				else					xto = (i+1) * widthStep;
				for (uint j=0; j<tiles; j++) {
					uint yfrom = j * heightStep;
					uint yto;
					if (j == tiles - 1)	yto = screenHeight;	//the last tile
					else					yto = (j+1) * heightStep;

					tileThreads[createdThreads++] = thread(&FractalCanvas::renderBitmapRect, this, highlight_guessed, xfrom, xto, yfrom, yto, bitmapRenderID);
				}
			}

			for (uint i=0; i<createdThreads; i++)
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
					bitmapManager->draw();
				}
			}
			refreshThread.join();
		}
	}
};

#endif