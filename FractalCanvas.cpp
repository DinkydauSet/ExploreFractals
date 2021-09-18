#ifndef FRACTALCANVAS_H
#define FRACTALCANVAS_H

//standard library
#include <iostream>
#include <algorithm>
#include <functional>

//this program
#include "common.cpp"
#include "FractalParameters.cpp"

using namespace std;

constexpr uint64 MAXIMUM_BITMAP_SIZE = 2147483648; // 2^31

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

	The iterationcount limit can be easily raised to 2^62 by using a uint64 instead of a uint, giving a much higher limit than the struct above while using the same amount of memory.
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
	//todo: inconsistent use of int and uint for renderIDs
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
		resize(1,1,1);
		mP.clearModified();
		if(debug) cout << "canvas constructed with dimensions " << mP.get_width() << "x" << mP.get_height() << endl;
	}

	//constructor with initial parameters
	FractalCanvas(FractalParameters& parameters, uint number_of_threads, shared_ptr<BitmapManager> bitmapManager, vector<GUIInterface*> GUIs)
	: FractalCanvas(number_of_threads, bitmapManager, GUIs)
	{
		changeParameters(parameters);
		if(debug) cout << "canvas parameters changed with dimensions " << mP.get_width() << "x" << mP.get_height() << endl;
	}

	~FractalCanvas() {
		if(debug) cout << "deleting FractalCanvas " << this << endl;

		end_all_usage();
		free(iters);

		if(debug) cout << "deleted FractalCanvas " << this << endl;
	}

	inline void* voidPtr() { return reinterpret_cast<void*>(this); }

	void parametersChangedEvent(int source_id = 0) {
		for (GUIInterface* gui : GUIs) {
			gui->parametersChanged(voidPtr(), source_id);
		}
	}

	void sizeChangedEvent() {
		for (GUIInterface* gui : GUIs) {
			gui->canvasSizeChanged(voidPtr());
		}
	}

	void renderStartedEvent(shared_ptr<RenderInterface> render, int renderID) {
		for (GUIInterface* gui : GUIs) {
			gui->renderStarted(move(render));
		}
	}

	void renderFinishedEvent(shared_ptr<RenderInterface> render, int renderID) {
		for (GUIInterface* gui : GUIs) {
			gui->renderFinished(move(render));
		}
	}

	void bitmapRenderStartedEvent(int bitmapRenderID) {
		for (GUIInterface* gui : GUIs) {
			gui->bitmapRenderStarted(voidPtr(), bitmapRenderID);
		}
	}

	void bitmapRenderFinishedEvent(int bitmapRenderID) {
		for (GUIInterface* gui : GUIs) {
			gui->bitmapRenderFinished(voidPtr(), bitmapRenderID);
		}
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

	inline ARGB gradient(int iterationCount) {
		const vector<ARGB>& gradientColors = mP.get_gradientColors();

		uint number_of_colors = gradientColors.size();
		double gradientPosition = (iterationCount + mP.get_gradientOffsetTerm()) * mP.get_gradientSpeedFactor();
		uint asInt = (uint)gradientPosition;
		ARGB previousColor = gradientColors[asInt % number_of_colors];
		ARGB nextColor = gradientColors[(asInt + 1) % number_of_colors];
		double ratio = gradientPosition - asInt;


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

	ResizeResult resize(uint newOversampling, uint newScreenWidth, uint newScreenHeight) {
		uint oldScreenWidth = mP.get_screenWidth();
		uint oldScreenHeight = mP.get_screenHeight();
		uint oldOversampling = mP.get_oversampling();
		return resize(newOversampling, newScreenWidth, newScreenHeight, oldOversampling, oldScreenWidth, oldScreenHeight);
	}
	
	ResizeResult resize(uint newOversampling, uint newScreenWidth, uint newScreenHeight
					 ,uint64 oldOversampling, uint64 oldScreenWidth, uint64 oldScreenHeight)
	{
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

		uint64 oldWidth = oldScreenWidth * oldOversampling;
		uint64 oldHeight = oldScreenHeight * oldOversampling;
		uint64 newWidth = newScreenWidth * newOversampling;
		uint64 newHeight = newScreenHeight * newOversampling;
		uint64 bitmap_size = newScreenWidth * newScreenHeight;
		uint64 fractalcanvas_size = newWidth * newHeight;
		uint64 old_fractalcanvas_size = oldWidth * oldHeight;

		bool realloc_bitmap = newScreenWidth != oldScreenWidth || newScreenHeight != oldScreenHeight;
		bool realloc_fractalcanvas = old_fractalcanvas_size != fractalcanvas_size;

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
			lock_guard<mutex> guard(activeRender);
			cancelBitmapRender();
			lock_guard<mutex> guard2(activeBitmapRender);

			mP.resize(newOversampling, newScreenWidth, newScreenHeight);

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
				mP.resize(oldOversampling, oldScreenWidth, oldScreenHeight);

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
					mP.resize(1,1,1);
					fractalcanvas_realloc(1);
					bitmap_realloc(1,1);
					changed = true;
					cout << "Allocating memory failed. The resolution has changed to 1x1." << endl;
				}
			}
		}
		return {success, changed, res};
	}

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

		uint old_screenWidth = mP.get_screenWidth();
		uint old_screenHeight = mP.get_screenHeight();
		uint old_oversampling = mP.get_oversampling();

		check_modified_memory = check_modified_memory || mP.modifiedMemory; //If the parameters were already changed, overrule
		if(debug) cout << "modified status: " << mP.modifiedSize << mP.modifiedMemory << mP.modifiedCalculations << mP.modifiedColors << endl;

		auto postActions = [this, source_id](ResizeResult res)
		{	
			assert( mP.modifiedSize == res.changed ); //The modification in size has resulted in a changed size of the FractalCanvas
			assert( ! (mP.modifiedMemory && ! mP.modifiedCalculations) ); //If memory is changed, that also changes the calculations.
			assert( ! (mP.modifiedSize && ! mP.modifiedMemory) ); //If the size is changed, that also changes the memory.

			if (res.changed)
				sizeChangedEvent();
			parametersChangedEvent(source_id);
		};

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
			postActions(res);
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
				res = resize(mP.get_oversampling(), mP.get_screenWidth(), mP.get_screenHeight(), old_oversampling, old_screenWidth, old_screenHeight);
				postActions(res);
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
					postActions(res);
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
						postActions(res);
						addToThreadcount(-1);
					}, res).detach();
				}
			}
		}

		return res;
	}

	void addToThreadcount(int amount) {
		lock_guard<mutex> guard(genericMutex);
		assert((int64)otherActiveThreads + amount >= 0); //there should not be less than 0 threads
		otherActiveThreads += amount;
		if(debug) cout << "FractalCanvas other active threads count changed to " << otherActiveThreads << endl;
	}

	ResizeResult changeParameters(const FractalParameters& newP, int source_id = 0)
	{
		return changeParameters([&](FractalParameters& P)
		{
			P.fromParameters(newP);
		}, source_id);
	}

	//	returns the index in ptPixels of (x, y) in the bitmap
	inline uint pixelIndex_of_pixelXY(uint x, uint y) {
		
		assert(x >= 0); assert(x < mP.get_screenWidth());
		assert(y >= 0); assert(y < mP.get_screenHeight());
		return mP.get_screenWidth()*y + x;
	}

	//unused?
	inline uint pixelIndex_of_itersXY(uint x, uint y) {
		//returns the corresponding index in ptPixels of (x, y) in the fractal canvas
		assert(x >= 0); assert(x < mP.get_width());
		assert(y >= 0); assert(y < mP.get_height());
		int oversampling = mP.get_oversampling();
		return pixelIndex_of_pixelXY(x / oversampling, y / oversampling);
	}

	inline uint itersIndex_of_itersXY(uint x, uint y) {
		assert(x >= 0); assert(x < mP.get_width());
		assert(y >= 0); assert(y < mP.get_height());
		//returns the index in iters of (x, y) in the fractalcanvas
		uint screenWidth = mP.get_screenWidth();
		uint screenHeight = mP.get_screenHeight();
		uint oversampling = mP.get_oversampling();
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
		return mP.map(xPos, yPos);
	}

	inline void setPixel(uint i, uint j, uint iterationCount, bool guessed, bool isInMinibrot)
	{
		assert(i >= 0 && j >= 0);
		assert(i < mP.get_width() && j < mP.get_height());

		IterData result(iterationCount, guessed, isInMinibrot);
		uint index = itersIndex_of_itersXY(i, j);
		iters[index] = result;
	}
	
	void renderBitmapRect(bool highlight_guessed, uint xfrom, uint xto, uint yfrom, uint yto, uint bitmapRenderID) {
		uint screenWidth = mP.get_screenWidth();
		uint screenHeight = mP.get_screenHeight();
		uint oversampling = mP.get_oversampling();
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
				
					ptPixels[pixelIndex_of_pixelXY(px, py)] = rgb(
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
				
					ptPixels[pixelIndex_of_pixelXY(px, py)] = rgb(
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
		uint screenWidth = mP.get_screenWidth();
		uint screenHeight = mP.get_screenHeight();

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



	void createNewRender(uint renderID)
	{
		/*
			This macro generates calls of createNewRenderTemplated for every possible combination of julia and using_avx.
			There's a check whether the procedure has a julia or avx version before an attempt is made to use it. For example, if S_().get_julia() is true, procedure.hasJuliaVersion determines whether a julia version is actually used. It overrides the setting.
		*/
		#define procedureRenderCase(procedure_identifier) \
			case procedure_identifier: { \
				constexpr Procedure procedure = getProcedureObject(procedure_identifier); \
				if (using_avx) { \
					if (mP.get_julia()) \
						createNewRenderTemplated<procedure_identifier, procedure.hasAvxVersion, procedure.hasJuliaVersion>(renderID); \
					else \
						createNewRenderTemplated<procedure_identifier, procedure.hasAvxVersion, false>(renderID); \
				} \
				else { \
					if (mP.get_julia()) \
						createNewRenderTemplated<procedure_identifier, false, procedure.hasJuliaVersion>(renderID); \
					else \
						createNewRenderTemplated<procedure_identifier, false, false>(renderID); \
				} \
				break; \
			}

		if(debug) {
			int procedure_identifier = mP.get_procedure_identifier();
			assert(procedure_identifier == mP.get_procedure().id);
			cout << "creating new render with procedure: " << procedure_identifier << " (" << mP.get_procedure().name() << ")" << " and ID " << renderID << endl;
		}

		switch (mP.get_procedure_identifier()) {
			procedureRenderCase(M2.id)
			procedureRenderCase(M3.id)
			procedureRenderCase(M4.id)
			procedureRenderCase(M5.id)
			procedureRenderCase(M512.id)
			procedureRenderCase(BURNING_SHIP.id)
			procedureRenderCase(CHECKERS.id)
			procedureRenderCase(TRIPLE_MATCHMAKER.id)
			procedureRenderCase(HIGH_POWER.id)
			procedureRenderCase(RECURSIVE_FRACTAL.id)
			procedureRenderCase(PURE_MORPHINGS.id)
		}

		#undef procedureRenderCase
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
			renderID	 = ++lastRenderID;
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
		renderBitmapFull(highlight_guessed, true, bitmapRenderID);
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
				lock_guard<mutex> guard(activeBitmapRender);
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


#endif