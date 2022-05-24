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

#ifndef FRACTALCANVAS_H
#define FRACTALCANVAS_H

//standard library
#include <algorithm>
#include <functional>

//lodepng
#include "lodepng/lodepng.cpp"

//this program
#include "common.cpp"
#include "FractalParameters.cpp"

constexpr uint64 MAXIMUM_BITMAP_SIZE = 2147483648; // 2^31

// dontdo: make this a template? to allow larger iterationcounts, and therefore increased memory, but only when necessary
class IterData {
public:
	uint iterationCount : 30;
	bool guessed : 1;
	bool inMinibrot : 1;
};

inline ARGB gradient(int iterationCount, const vector<ARGB>& gradientColors, uint number_of_colors, float offset_term, float speed_factor)
{
	float gradientPosition = (iterationCount + offset_term) * speed_factor;
	uint asInt = (uint)gradientPosition;
	ARGB c1 = gradientColors[asInt % number_of_colors];
	ARGB c2 = gradientColors[(asInt + 1) % number_of_colors];
	float ratio = gradientPosition - asInt;
	return rgb(
		uint8(c1.R*(1 - ratio) + c2.R*ratio) | 1,
		uint8(c1.G*(1 - ratio) + c2.G*ratio) | 1,
		uint8(c1.B*(1 - ratio) + c2.B*ratio) | 1
	);
	//The returned expression used to be a function but manual inlining turns out to be faster.
}

class FractalCanvas {
public:
	IterData* iters{ nullptr }; //iteration data array, managed by FractalCanvas
	ARGB* ptPixels{ nullptr }; //bitmap colors representing the iteration data, managed by the bitmapManager, not FractalCanvas
private:
	FractalParameters mP;
public:
	inline const FractalParameters& P() { return mP; } //read access to P
	inline FractalParameters& Pmutable() { return mP; } //read-write access to P

	mutex genericMutex; //mutex for various race condition tasks such as updating the uints below; should not be held for a long time
	mutex activeRender; //only one render can be executing at any one time
	mutex activeBitmapRender;
	//The mutexes are used to guarantee correctness of these 5 values:
	//dontdo: inconsistent use of int and uint for renderIDs
	int lastRenderID{ 0 };
	int activeRenders{ 0 };
	int renderQueueSize{ 0 };
	int lastBitmapRenderID{ 0 };
	int activeBitmapRenders{ 0 };
	int bitmapRenderQueueSize{ 0 };
	int otherActiveThreads{ 0 };

	uint number_of_threads;
	shared_ptr<BitmapManager> bitmapManager;
	vector<GUIInterface*> GUIs; //there will usually be 1 GUI, but I want to make it possible to have 0 GUIs for commandline rendering.

	// The definition of this can't be here because it uses the Render class, but the Render class can only be defined after FractalCanvas.
	template <int procedure_identifier, bool use_avx, bool julia>
	void createNewRenderTemplated(uint renderID);

	// This only works if no new renders and bitmapRenders are queued while this function is busy.
	void end_all_usage()
	{
		if(debug) cout << "FractalCanvas ending all usage" << endl;
		while (true) {
			cancelRender();
			lock_guard<mutex> guard(activeRender);

			if(debug) cout << "waiting for renders to stop: activeRenders: " << activeRenders << ", queue size: " << renderQueueSize << endl;

			if (renderQueueSize == 0)
				break;
		}

		while (true) {
			cancelBitmapRender();
			lock_guard<mutex> guard(activeBitmapRender);

			if(debug) cout << "waiting for bitmap renders to stop: activeBitmapRenders: " << activeBitmapRenders << ", queue size: " << bitmapRenderQueueSize << ", other active threads: " << otherActiveThreads << endl;

			if (bitmapRenderQueueSize == 0 && otherActiveThreads == 0)
				break;
		}
		if(debug) cout << "FractalCanvas ended all usage" << endl;
	}

	FractalCanvas(uint number_of_threads, shared_ptr<BitmapManager> bitmapManager, vector<GUIInterface*> GUIs = {})
	: bitmapManager(bitmapManager)
	, GUIs(GUIs)
	{
		assert(number_of_threads > 0);
		if(debug) cout << "constructing FractalCanvas" << endl;
		this->number_of_threads = number_of_threads;

		/*
			This sets the width and height to 1 and allocates memory for that size. This is to ensure that the FractalCanvas' state is consistent. Currently it's not consistent because no memory has been allocated, which corresponds to a width and height of 0, which is an invalid value for many of the functions.
		*/
		resize(1,1,1,1);
		mP.clearModified();
		if(debug) cout << "canvas constructed with dimensions " << mP.width_canvas() << "x" << mP.height_canvas() << endl;
	}

	//constructor with initial parameters
	FractalCanvas(FractalParameters& parameters, uint number_of_threads, shared_ptr<BitmapManager> bitmapManager, vector<GUIInterface*> GUIs)
	: FractalCanvas(number_of_threads, bitmapManager, GUIs)
	{
		changeParameters(parameters);
		if(debug) cout << "canvas parameters changed with dimensions " << mP.width_canvas() << "x" << mP.height_canvas() << endl;
	}

	FractalCanvas(const FractalCanvas& other) = delete;
	FractalCanvas& operator=(const FractalCanvas& other) = delete;

	//todo: consider changing this. The destructor waits for all threads using the FractalCanvas to end. This includes threads created by the FractalCanvas member functions, which I think is good. The FractalCanvas creates them, and so it's responsible for them. But it also includes threads created by the GUI. The GUI should be responsible for ending those threads before destroying a FractalCanvas.
	//Maybe it's also better to use unique_ptr instead of a normal pointer for iters (not important because it already works).
	~FractalCanvas() {
		if(debug) cout << "deleting FractalCanvas " << this << endl;

		end_all_usage();
		free(iters);

		if(debug) cout << "deleted FractalCanvas " << this << endl;
	}

	inline void* voidPtr() { return reinterpret_cast<void*>(this); }

	void parametersChangedEvent(int source_id = 0) {
		if(debug) cout << this << " parametersChangedEvent" << endl;
		for (GUIInterface* gui : GUIs) {
			gui->parametersChanged(voidPtr(), source_id);
		}
		if(debug) cout << this << " parametersChangedEvent done" << endl;
	}

	void sizeChangedEvent() {
		if(debug) cout << this << " sizeChangedEvent" << endl;
		for (GUIInterface* gui : GUIs) {
			gui->canvasSizeChanged(voidPtr());
		}
		if(debug) cout << this << " sizeChangedEvent done" << endl;
	}

	void canvasResizeFailedEvent(ResizeResult result) {
		if(debug) cout << this << " canvasResizeFailedEvent" << endl;
		for (GUIInterface* gui : GUIs) {
			gui->canvasResizeFailed(voidPtr(), result);
		}
		if(debug) cout << this << " canvasResizeFailedEvent done" << endl;
	}

	void renderStartedEvent(shared_ptr<RenderInterface> render, int renderID) {
		if(debug) cout << this << " renderStartedEvent" << endl;
		for (GUIInterface* gui : GUIs) {
			gui->renderStarted(move(render));
		}
		if(debug) cout << this << " renderStartedEvent done" << endl;
	}

	void renderFinishedEvent(shared_ptr<RenderInterface> render, int renderID) {
		if(debug) cout << this << " renderFinishedEvent" << endl;
		for (GUIInterface* gui : GUIs) {
			gui->renderFinished(move(render));
		}
		if(debug) cout << this << " renderFinishedEvent done" << endl;
	}

	void bitmapRenderStartedEvent(int bitmapRenderID) {
		if(debug) cout << this << " bitmapRenderStartedEvent" << endl;
		for (GUIInterface* gui : GUIs) {
			gui->bitmapRenderStarted(voidPtr(), bitmapRenderID);
		}
		if(debug) cout << this << " bitmapRenderStartedEvent done" << endl;
	}

	void bitmapRenderFinishedEvent(int bitmapRenderID) {
		if(debug) cout << this << " bitmapRenderFinishedEvent" << endl;
		for (GUIInterface* gui : GUIs) {
			gui->bitmapRenderFinished(voidPtr(), bitmapRenderID);
		}
		if(debug) cout << this << " bitmapRenderFinishedEvent done" << endl;
	}

	void cancelRender() {
		/*
			This causes active renders to stop. It doesn't actively cancel anything, rather it's the render itself that checks, every so often, whether it should stop.
			Changing the lastRenderID here doesn't require a lock. If it happens that another threads is also changing lastRenderID, that must be because of another cancellation or new render, which already has the desired effect.
		*/
		++lastRenderID;
	}

	void cancelBitmapRender() {
		++lastBitmapRenderID;
	}

	ResizeResult resize(uint newOversampling, uint new_target_width, uint new_target_height, uint newBitmapZoom) {
		uint old_target_width = mP.get_target_width();
		uint old_target_height = mP.get_target_height();
		uint oldOversampling = mP.get_oversampling();
		uint oldBitmapZoom = mP.get_bitmap_zoom();
		return resize(
			newOversampling, new_target_width, new_target_height, newBitmapZoom
			,oldOversampling, old_target_width, old_target_height, oldBitmapZoom
		);
	}
	
	ResizeResult resize(
		uint newOversampling, uint new_target_width, uint new_target_height, uint newBitmapZoom
		,uint oldOversampling, uint old_target_width, uint old_target_height, uint oldBitmapZoom
	) {
		assert(newOversampling > 0);
		assert(new_target_width > 0);
		assert(new_target_height > 0);

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

		//I create FractalParameters objects to be able to use the functions that calculate the resolution, canvas size etc. (not a great solution but it works)
		FractalParameters old, new_;
		old.resize(old_target_width, old_target_height, oldOversampling, oldBitmapZoom);
		new_.resize(new_target_width, new_target_height, newOversampling, newBitmapZoom);

		uint64 old_width_canvas = old.width_canvas();
		uint64 old_height_canvas = old.height_canvas();
		uint64 old_width_bitmap = old.width_bitmap();
		uint64 old_height_bitmap = old.height_bitmap();
		uint64 new_width_canvas = new_.width_canvas();
		uint64 new_height_canvas = new_.height_canvas();
		uint64 new_width_bitmap = new_.width_bitmap();
		uint64 new_height_bitmap = new_.height_bitmap();

		uint64 bitmap_size = new_width_bitmap * new_height_bitmap;
		uint64 old_bitmap_size = old_width_bitmap * old_height_bitmap;
		uint64 fractalcanvas_size = new_width_canvas * new_height_canvas; //this really needs the 64-bit accuracy
		uint64 old_fractalcanvas_size = old_width_canvas * old_height_canvas;

		bool realloc_bitmap = old_bitmap_size != bitmap_size;
		bool realloc_fractalcanvas = old_fractalcanvas_size != fractalcanvas_size;

		if (!realloc_bitmap && !realloc_fractalcanvas) {
			cout << "entered resize. The resolutions remain the same. Nothing happens." << endl;
			return {true, false, ResizeResultType::Success};
		}
		else {
			if (
				new_width_canvas == 0
				|| new_height_canvas == 0
				|| bitmap_size > MAXIMUM_BITMAP_SIZE
			) {
				cout << "Size outside of allowed range." << endl;
				cout << "width: " << new_width_canvas << "  " << "height: " << new_height_canvas << endl;
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
			lock_guard<mutex> guard(activeRender);
			cancelBitmapRender();
			lock_guard<mutex> guard2(activeBitmapRender);

			mP.resize(new_target_width, new_target_height, newOversampling, newBitmapZoom);

			if (realloc_fractalcanvas) {
				fractalcanvas_realloc(fractalcanvas_size);
			}
			if (realloc_bitmap) {
				bitmap_realloc(new_width_bitmap, new_height_bitmap);
			}

			if (iters == nullptr || ptPixels == nullptr) {
				//Allocating memory failed.
				res = ResizeResultType::MemoryError;
				success = false;

				// Try to get back the old size
				mP.resize(old_target_width, old_target_height, oldOversampling, oldBitmapZoom);

				if (realloc_fractalcanvas) {
					fractalcanvas_realloc(old_width_canvas * old_height_canvas);
				}
				if (realloc_bitmap) {
					bitmap_realloc(old_width_bitmap, old_height_bitmap);
				}

				if (iters != nullptr && ptPixels != nullptr) {
					changed = false;
					cout << "Allocating memory failed. The previous resolution has been restored." << endl;
				}
				else {
					//As a last resort, change the image size to 1x1. This can't fail in any reasonable situation.
					mP.resize(1,1,1,1);
					fractalcanvas_realloc(1);
					bitmap_realloc(1,1);
					changed = true;
					cout << "Allocating memory failed. The resolution has changed to 1x1." << endl;
				}
			}
		}
		return {success, changed, res};
	}

	void postResizeActions(ResizeResult res, int source_id)
	{
		//If memory is changed, that also changes the calculations.
		assert( ! (mP.modifiedMemory && ! mP.modifiedCalculations) );

		//If the size is changed, that also changes the memory.
		assert( ! (mP.modifiedSize && ! mP.modifiedMemory) );

		if (res.success == false)
			canvasResizeFailedEvent(res);
		if (res.changed)
			sizeChangedEvent();
		parametersChangedEvent(source_id);
	};

	private: FractalParameters temp;
public:

	/*
		action is a function that modified the parameters it's given. changeParameters resizes the canvas if necessary and notifies the GUI by emitting events.

		The bools modifiedCalculations, modifiedColors and modifiedMemory are updated automatically by the publicly available functions of FractalParameters.

		parametersChangedEvent is always called, even if there's no change to calculations, colors and memory. This is necessary for the nana GUI to update after setting the inflection zoom level, which doesn't affect the calculations, coloring or size, but still requires a GUI update.
	*/
	ResizeResult changeParameters(std::function<void(FractalParameters&)> action, int source_id = 0, bool check_modified_memory = true)
	{
		ResizeResult res = {true, false, ResizeResultType::Success};

		uint old_target_width = mP.get_target_width();
		uint old_target_height = mP.get_target_height();
		uint old_oversampling = mP.get_oversampling();
		uint old_bitmap_zoom = mP.get_bitmap_zoom();

		check_modified_memory = check_modified_memory || mP.modifiedMemory; //If the parameters were already changed, overrule
		if(debug) cout << "modified status: " << mP.modifiedSize << mP.modifiedMemory << mP.modifiedCalculations << mP.modifiedColors << endl;

		//
		// If check_modified_memory, the action is first applied to a copy of the parameters, to see if the changes require allocating new memory. In that case, the parameters should not be changed during a render. The generality of accepting a function makes programming the GUI a lot easier, but it requires this kind of check.
		//
		// check_modified_memory can be set to false to skip the check for efficiency. Then the action is simply applied to the parameters, even if there's a render going on. It's the responsibility of the user of changeParameters to disable the check only when it is known in advance that no memory allocation can occur during a render.
		//

		if (check_modified_memory) {
			// After this temp.modifiedMemory will indicate whether the action would require allocating memory
			temp.fromParameters(mP);
			temp.clearModified();
			action(temp);
		}

		if ( ! check_modified_memory || ! temp.modifiedMemory) {
			if(debug) cout << "changeParameters at location 1" << endl;
			action(mP);
			postResizeActions(res, source_id);
		}
		else {
			if (temp.modifiedSize) {
				if(debug) cout << "changeParameters at location 2" << endl;
				{
					cancelRender();
					lock_guard<mutex> guard(activeRender);
					cancelBitmapRender();
					lock_guard<mutex> guard2(activeBitmapRender);

					action(mP);
				}
				res = resize(
					mP.get_oversampling(), mP.get_target_width(), mP.get_target_height(), mP.get_bitmap_zoom()
					,old_oversampling, old_target_width, old_target_height, old_bitmap_zoom
				);
				postResizeActions(res, source_id);
			}
			else
			{
				// Different memory change than size, can be done in another thread if needed. First try to do it in this thread:
					
				bool done = false;

				cancelRender();
				if(activeRender.try_lock())
				{
					cancelBitmapRender();
					if (activeBitmapRender.try_lock())
					{
						if(debug) cout << "changeParameters at location 3" << endl;
						action(mP);
						
						done = true;
						activeBitmapRender.unlock();
					}
					activeRender.unlock();
				}

				if (done) {
					if(debug) cout << "changeParameters at location 4" << endl;
					postResizeActions(res, source_id);
				}
				else
				{
					//Start another thread that can apply the changes after the renders are done. The GUI can start responding again while the thread waits.
					
					addToThreadcount(1);
					thread([=](ResizeResult res)
					{
						if(debug) cout << "changeParameters at location 5" << endl;
						{
							cancelRender();
							lock_guard<mutex> guard(activeRender);
							cancelBitmapRender();
							lock_guard<mutex> guard2(activeBitmapRender);

							action(mP);
						}
						postResizeActions(res, source_id);
						addToThreadcount(-1);
					}, res).detach();
				}
			}
		}

		return res;
	}

	ResizeResult changeParameters(const FractalParameters& newP, int source_id = 0)
	{
		return changeParameters([&](FractalParameters& P)
		{
			P.fromParameters(newP);
		}, source_id);
	}


	void addToThreadcount(int amount) {
		lock_guard<mutex> guard(genericMutex);
		assert((int64)otherActiveThreads + amount >= 0); //there should not be fewer than 0 threads
		otherActiveThreads += amount;
		if(debug) cout << "FractalCanvas other active threads count changed to " << otherActiveThreads << endl;
	}

	//todo: move these index calculating functions out of class FractalCanvas. They're more generally applicable.

	//	returns the index in ptPixels of (x, y) in the bitmap
	// When bitmap_zoom > 1 this returns the index of the topleft corner pixel of the block of pixels belonging to this coordinate.
	// You can think of it like this: there is a conceptual bitmap that could be in memory, but is not for performance, that has the actual size of the fractalCanvas (screenWidth * screenHeight). This function accepts coordinates in that bitmap, and gives an/the index of those coordinates in the real bitmap, which is larger if bitmap_zoom > 1.
	inline uint pixelIndex_of_pixelXY(uint x, uint y)
	{
		assert(x >= 0); assert(x < mP.width_resolution());
		assert(y >= 0); assert(y < mP.height_resolution());
		const uint& bitmap_zoom = mP.get_bitmap_zoom();
		return (mP.width_resolution() * bitmap_zoom * y * bitmap_zoom) + (x * bitmap_zoom);
	}

	//unused?
	inline uint pixelIndex_of_itersXY(uint x, uint y)
	{
		//returns the corresponding index in ptPixels of (x, y) in the fractal canvas
		assert(x >= 0); assert(x < mP.width_canvas());
		assert(y >= 0); assert(y < mP.height_canvas());
		uint oversampling = mP.get_oversampling();
		return pixelIndex_of_pixelXY(x / oversampling, y / oversampling);
	}

	inline uint itersIndex_of_itersXY(uint x, uint y) {
		assert(x >= 0); assert(x < mP.width_canvas());
		assert(y >= 0); assert(y < mP.height_canvas());
		//returns the index in iters of (x, y) in the fractalcanvas
		uint width_resolution = mP.width_resolution();
		uint oversampling = mP.get_oversampling();
		uint samples = oversampling * oversampling;
		uint dx = x % oversampling;
		uint dy = y % oversampling;
		uint _x = x / oversampling;
		uint _y = y / oversampling;
		return (_x + _y * width_resolution) * samples + dy + dx * oversampling;
	}
	

	inline uint getIterationcount(uint x, uint y) {
		return iters[itersIndex_of_itersXY(x,y)].iterationCount;
	}

	inline IterData getIterData(uint x, uint y) {
		return iters[itersIndex_of_itersXY(x,y)];
	}

	inline double_c map(uint xPos, uint yPos) {
		return mP.map(xPos, yPos);
	}

	inline void setPixel(uint i, uint j, uint iterationCount, bool guessed, bool isInMinibrot)
	{
		assert(i >= 0 && j >= 0);
		assert(i < mP.width_canvas() && j < mP.height_canvas());

		IterData result{ iterationCount, guessed, isInMinibrot };
		uint index = itersIndex_of_itersXY(i, j);
		iters[index] = result;
	}
	
	void renderBitmapRect(bool highlight_guessed, uint xfrom, uint xto, uint yfrom, uint yto) {
		const uint width_resolution = mP.width_resolution();
		const uint height_resolution = mP.height_resolution();
		const uint oversampling = mP.get_oversampling();
		const uint bitmap_zoom = mP.get_bitmap_zoom();
		const uint samples = oversampling * oversampling;

		const vector<ARGB>& gradientColors = mP.get_gradientColors();
		const float offset_term = mP.get_gradientOffsetTerm();
		const float speed_factor =  mP.get_gradientSpeedFactor();
		const uint number_of_colors = gradientColors.size();

		assert(xfrom >= 0); assert(xfrom <= width_resolution);
		assert(xto >= xfrom); assert(xto <= width_resolution);
		assert(yfrom >= 0); assert(yfrom <= height_resolution);
		assert(yto >= yfrom); assert(yto <= height_resolution);

		//One implementation that always works (with oversampling and bitmap zoom) is significantly slower. That's why I have a dedicated implementation for every case:
		if (highlight_guessed == false)
		{
			if (bitmap_zoom == 1 && samples == 1) {
				for (uint py=yfrom; py<yto; py++)
				{
					ptrdiff_t pixelIndex_col = py * width_resolution;

					for (int px=xfrom; px<xto; px++)
					{
						ARGB color;
						IterData it = iters[pixelIndex_col + px];
						if (it.inMinibrot)
							color = rgb(0, 0, 0);
						else
							color = gradient(it.iterationCount, gradientColors, number_of_colors, offset_term, speed_factor);

						ptPixels[pixelIndex_col + px] = color;
					}
				}
			}
			else if (bitmap_zoom == 1 && samples > 1)
			{
				for (uint py=yfrom; py<yto; py++)
				{
					ptrdiff_t pixelIndex_col = py * width_resolution;

					for (uint px=xfrom; px<xto; px++)
					{
						ptrdiff_t pixelIndex = pixelIndex_col + px;
						ptrdiff_t itersStartIndex = pixelIndex * samples;
						
						int sumR=0, sumG=0, sumB=0;
						ARGB color;

						for (int i=0; i<samples; i++) {
							IterData it = iters[itersStartIndex + i];
							if (it.inMinibrot)
								color = rgb(0, 0, 0);
							else
								color = color = gradient(it.iterationCount, gradientColors, number_of_colors, offset_term, speed_factor);
							sumR += color.R;
							sumG += color.G;
							sumB += color.B;
						}

						color = rgb(
							(uint8)(sumR / samples),
							(uint8)(sumG / samples),
							(uint8)(sumB / samples)
						);

						ptPixels[pixelIndex] = color;
					}
				}
			}
			else if (bitmap_zoom > 1 && samples == 1) {
				const uint row_width = width_resolution * bitmap_zoom;

				for (uint py=yfrom; py<yto; py++)
				{
					ptrdiff_t itersStartIndex = py * width_resolution;
					ptrdiff_t pixelIndex_col = width_resolution * bitmap_zoom * py;

					for (uint px=xfrom; px<xto; px++)
					{
						ARGB color;
						IterData it = iters[itersStartIndex + px];
						if (it.inMinibrot)
							color = rgb(0, 0, 0);
						else
							color = gradient(it.iterationCount, gradientColors, number_of_colors, offset_term, speed_factor);

						ptrdiff_t pixelIndex = (pixelIndex_col + px) * bitmap_zoom;
						for (int i=0; i<bitmap_zoom; i++) {
							for (int j=0; j<bitmap_zoom; j++) {
								ptPixels[pixelIndex + i + row_width * j] = color;
							}
						}
					}
				}
			}
			else {
				assert(bitmap_zoom > 1 && samples > 1);
				//This case is unlikely because it cannot be reached through the user interface, but by loading JSON parameters it can be reached. With the GUI, only oversampling OR bitmap_zoom can be set, not both.
				const uint row_width = width_resolution * bitmap_zoom;

				for (uint py=yfrom; py<yto; py++)
				{
					for (uint px=xfrom; px<xto; px++)
					{
						uint itersStartIndex = (px + py * width_resolution) * samples;

						uint sumR=0, sumG=0, sumB=0;
						ARGB color;
		
						for (uint i=0; i<samples; i++) {
							IterData it = iters[itersStartIndex + i];
							if (it.inMinibrot)			      color = rgb(0, 0, 0);
							else                              color = gradient(it.iterationCount, gradientColors, number_of_colors, offset_term, speed_factor);
							sumR += color.R;
							sumG += color.G;
							sumB += color.B;
						}

						uint pixelIndex = pixelIndex_of_pixelXY(px, py);
						ARGB new_color = rgb(
							(uint8)(sumR / samples),
							(uint8)(sumG / samples),
							(uint8)(sumB / samples)
						);

						for (int i=0; i<bitmap_zoom; i++) {
							for (int j=0; j<bitmap_zoom; j++) {
								ptPixels[pixelIndex + i + row_width * j] = new_color;
							}
						}
					}
				}
			}
		}
		else {
			//highlight_guessed is true
			const uint row_width = width_resolution * bitmap_zoom;

			for (uint py=yfrom; py<yto; py++)
			{
				for (uint px=xfrom; px<xto; px++)
				{
					uint itersStartIndex = (px + py * width_resolution) * samples;

					uint sumR=0, sumG=0, sumB=0;
					ARGB color;
		
					for (uint i=0; i<samples; i++) {
						IterData it = iters[itersStartIndex + i];
						if (it.inMinibrot && !it.guessed) color = rgb(255, 0, 0);
						else if (it.inMinibrot)           color = rgb(0, 0, 255);
						else if (it.guessed)              color = rgb(0, 255, 0);
						else
							color = gradient(it.iterationCount, gradientColors, number_of_colors, offset_term, speed_factor);
						sumR += color.R;
						sumG += color.G;
						sumB += color.B;
					}

					uint pixelIndex = pixelIndex_of_pixelXY(px, py);
					ARGB new_color = rgb(
						(uint8)(sumR / samples),
						(uint8)(sumG / samples),
						(uint8)(sumB / samples)
					);

					for (int i=0; i<bitmap_zoom; i++) {
						for (int j=0; j<bitmap_zoom; j++) {
							ptPixels[pixelIndex + i + row_width * j] = new_color;
						}
					}
				}
			}
		}
	}

	void renderBitmapFull(bool highlight_guessed, bool multithreading) {
		uint screenWidth = mP.width_resolution();
		uint screenHeight = mP.height_resolution();

		//use multithreading for extra speed when there's no render active
		if (multithreading) {
			if(debug) cout << "using multiple threads for renderBitmapFull" << endl;
			uint tiles = (uint)(sqrt(number_of_threads)); //the number of tiles in both horizontal and vertical direction, so in total there are (tiles * tiles)
			tiles = min({tiles, screenWidth, screenHeight});

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

					tileThreads[createdThreads++] = thread(&FractalCanvas::renderBitmapRect, this, highlight_guessed, xfrom, xto, yfrom, yto);
				}
			}

			for (uint i=0; i<createdThreads; i++)
				tileThreads[i].join();
		}
		else {
			if(debug) cout << "using 1 thread for renderBitmapFull" << endl;
			renderBitmapRect(highlight_guessed, 0, screenWidth, 0, screenHeight);
		}	
	}


	template <int procedure_identifier>
	void procedureRenderCase(uint renderID)
	{
		constexpr Procedure procedure = getProcedureObject(procedure_identifier);
		if (using_avx) {
			if (mP.get_julia())
				createNewRenderTemplated<procedure_identifier, procedure.hasAvxVersion, procedure.hasJuliaVersion>(renderID);
			else
				createNewRenderTemplated<procedure_identifier, procedure.hasAvxVersion, false>(renderID);
		}
		else {
			if (mP.get_julia())
				createNewRenderTemplated<procedure_identifier, false, procedure.hasJuliaVersion>(renderID);
			else
				createNewRenderTemplated<procedure_identifier, false, false>(renderID);
		}
	};

	void createNewRender(uint renderID)
	{
		if(debug) {
			int procedure_identifier = mP.get_procedure_identifier();
			assert(procedure_identifier == mP.get_procedure().id);
			cout << "creating new render with procedure: " << procedure_identifier << " (" << mP.get_procedure().name() << ")" << " and ID " << renderID << endl;
		}

		switch (mP.get_procedure_identifier()) {
			case M2.id:                return procedureRenderCase<M2.id>(renderID);
			case M3.id:                return procedureRenderCase<M3.id>(renderID);
			case M4.id:                return procedureRenderCase<M4.id>(renderID);
			case M5.id:                return procedureRenderCase<M5.id>(renderID);
			case M512.id:              return procedureRenderCase<M512.id>(renderID);
			case BURNING_SHIP.id:      return procedureRenderCase<BURNING_SHIP.id>(renderID);
			case CHECKERS.id:          return procedureRenderCase<CHECKERS.id>(renderID);
			case TRIPLE_MATCHMAKER.id: return procedureRenderCase<TRIPLE_MATCHMAKER.id>(renderID);
			case HIGH_POWER.id:        return procedureRenderCase<HIGH_POWER.id>(renderID);
			case RECURSIVE_FRACTAL.id: return procedureRenderCase<RECURSIVE_FRACTAL.id>(renderID);
			case PURE_MORPHINGS.id:    return procedureRenderCase<PURE_MORPHINGS.id>(renderID);
			case DEBUG_TEST.id:        return procedureRenderCase<DEBUG_TEST.id>(renderID);
		}
		assert(false);
	}

	void createNewRender() {
		createNewRender(++lastRenderID);
	}

	//
	// There can be multiple threads waiting to start if the user scrolls very fast and many renders per second are started and have to be cancelled again. To deal with that situation, this function places all new renders in a queue by using the mutex activeRender.
	// In addition, the mutex renderInfo is used to protect the variables that keep information about the number of renders, the last render ID etc.
	//
	void enqueueRender(bool new_thread = true)
	{
		auto action = [this](uint renderID)
		{
			{
				lock_guard<mutex> guard(activeRender);
				//if here, it's this thread's turn
				{
					lock_guard<mutex> guard(genericMutex);
					bool newerRenderExists = lastRenderID > renderID;

					if (newerRenderExists)
					{
						if(debug) cout << "not starting render " << renderID << " as there's a newer render with ID " << lastRenderID << endl;
						renderQueueSize--;
						return;
					}
					else {
						activeRenders++;
					}
				}
				if(debug) cout << "starting render with ID " << renderID << endl;
				createNewRender(renderID);
				{
					lock_guard<mutex> guard(genericMutex);
					activeRenders--;
					renderQueueSize--;
				}
			}
			//After releasing the lock on activeRender, nothing should be done that uses the FractalCanvas. When the user closes a tab during a render, the FractalCanvas gets destroyed immediately after the lock is released.
		};

		uint renderID;

		//update values to reflect that there's a new render
		{
			lock_guard<mutex> guard(genericMutex);
			renderQueueSize++;
			renderID = ++lastRenderID;
		}

		//actually start the render
		if (new_thread)
			thread(action, renderID).detach();
		else
			action(renderID);
	}


	void createNewBitmapRender(bool highlight_guessed, uint bitmapRenderID)
	{
		bitmapRenderStartedEvent(bitmapRenderID);
		renderBitmapFull(highlight_guessed, true);
		bitmapRenderFinishedEvent(bitmapRenderID);
	}

	void createNewBitmapRender(bool highlight_guessed) {
		createNewBitmapRender(highlight_guessed, ++lastBitmapRenderID);
	}

	void enqueueBitmapRender(bool new_thread = true, bool highlight_guessed = false)
	{
		auto action = [this, highlight_guessed](uint bitmapRenderID)
		{
			{
				if(debug) cout << this_thread::get_id() << " is waiting for lock activeBitmapRender" << endl;
				lock_guard<mutex> guard(activeBitmapRender);
				if(debug) cout << this_thread::get_id() << " has lock activeBitmapRender" << endl;
				//if here, it's this thread's turn
				{
					lock_guard<mutex> guard(genericMutex);
					bool newerRenderExists = lastBitmapRenderID > bitmapRenderID;

					if (newerRenderExists)
					{
						if(debug) cout << "not starting bitmap render " << bitmapRenderID << " as there's a newer bitmap render with ID " << lastBitmapRenderID << endl;
						bitmapRenderQueueSize--;
						return;
					}
					else {
						activeBitmapRenders++;
					}
				}
				if(debug) cout << "starting bitmap render with ID " << bitmapRenderID << endl;
				createNewBitmapRender(highlight_guessed, bitmapRenderID);
				{
					lock_guard<mutex> guard(genericMutex);
					activeBitmapRenders--;
					bitmapRenderQueueSize--;
				}
			}
			if(debug) cout << this_thread::get_id() << " released lock activeBitmapRender" << endl;
			//After releasing the lock on activeBitmapRender, nothing should be done that uses the FractalCanvas. When the user closes a tab during a bitmap render, the FractalCanvas gets destroyed immediately after the lock is released.
		};

		uint bitmapRenderID;

		//update values to reflect that there's a new render
		{
			lock_guard<mutex> guard(genericMutex);
			bitmapRenderQueueSize++;
			bitmapRenderID = ++lastBitmapRenderID;
		}

		//actually start the render
		if (new_thread)
			thread(action, bitmapRenderID).detach();
		else
			action(bitmapRenderID);
	}
};

void saveImage(FractalCanvas* canvas, string filename, bool cleanup = true) {
	uint width_resolution = canvas->P().width_resolution();
	uint height_resolution = canvas->P().height_resolution();
	uint bitmap_zoom = canvas->P().get_bitmap_zoom();

	//
	//	These loops performs conversion.
	//
	//	PNG requires RGBA-values in big-endian order. The colors in this program are ARGB stored in little-endian order. Lodepng interprets the data correctly when delivered as ABGR (the reserve order of RGBA) because of the endianness difference. This comes down to swapping red and blue.
	//	 
	//	 I convert the colors to ABGR in the original array to reduce memory usage. That means that after saving the PNG, the colors may need to be reverted to their original values.
	//
	if (bitmap_zoom > 1)
	{
		//I don't want to save the zoomed bitmap. This loop uses the existing array (which is large enough) to create an unzoomed bitmap.
		for (uint y=0; y< height_resolution; y++)
		for (uint x=0; x< width_resolution; x++)
		{
			uint pixelIndex = canvas->pixelIndex_of_pixelXY(x, y);
			const ARGB pixel = canvas->ptPixels[pixelIndex];
			canvas->ptPixels[width_resolution * y + x] = rgb(pixel.B, pixel.G, pixel.R);
		}
	}
	else
	{
		for (uint y=0; y< height_resolution; y++)
		for (uint x=0; x< width_resolution; x++)
		{
			uint pixelIndex = canvas->pixelIndex_of_pixelXY(x, y);
			ARGB& pixel = canvas->ptPixels[pixelIndex];
			pixel = rgb(pixel.B, pixel.G, pixel.R);
		}
	}

	
	uint8* out;
	size_t outsize;
	uint errorcode = lodepng_encode32(
		&out, &outsize				//will contain the PNG data
		,(uint8*)canvas->ptPixels	//image data to encode
		, width_resolution, height_resolution	//width and height
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
		canvas->createNewBitmapRender(false);
	}
}


#endif