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

//Visual Studio requires this for no clear reason
#include "stdafx.h"

//C++ standard library
#include <iomanip>	//for setfill and setw, to make framenumbers with leading zeros such as frame000001.png
#include <chrono>
#include <thread>
//#include <random>
#include <fstream>
#include <regex>
#include <codecvt>	//to convert utf16 to utf8
#include <locale>

//WinApi
#include <windows.h>
#include <shellapi.h>

//lodepng
#include "lodepng/lodepng.cpp"

//this program
#include "common.cpp"
#include "GUI.cpp"
#include "FractalParameters.cpp"
#include "FractalCanvas.cpp"
#include "Render.cpp"
#include "windows_util.cpp"
#include "utilities.cpp"


//macro to easily time pieces of code
#define timerstart { auto start = chrono::high_resolution_clock::now();
#define timerend(name) \
	auto end = chrono::high_resolution_clock::now(); \
	chrono::duration<double> elapsed = end - start; \
	cout << #name << " took " << elapsed.count() << " seconds" << endl; }


//I used this to prevent moire by adding some noise to the coordinates but it's too slow:
/*
std::mt19937_64 generator;
uniform_real_distribution<double> distribution(0.0, 1.0);

double random() {
	return distribution(generator);
}
*/


template <int procedure_identifier, bool use_avx, bool julia>
void FractalCanvas::createNewRenderTemplated(uint renderID)
{
	assert( mP.get_procedure().id == procedure_identifier );

	auto render = make_shared<Render<procedure_identifier, use_avx, julia>>(*this, renderID);
	auto& R = *render.get();

	uint width = mP.get_width();
	uint height = mP.get_height();
	{
		//Printing information before the render
		double_c center = mP.get_center();
		double_c juliaSeed = mP.get_juliaSeed();
		cout << "-----New Render-----" << endl;
		cout << "renderID: " << renderID << endl;
		cout << "Procedure: " << mP.get_procedure().name() << (julia ? " julia" : "") << (use_avx ? " (avx)" : "") << endl;
		cout << "width: " << width << endl;
		cout << "height: " << height << endl;
		cout << "center: " << real(center) << " + " << imag(center) << " * I" << endl;
		cout << "xrange: " << mP.get_x_range() << endl;
		cout << "yrange: " << mP.get_y_range() << endl;
		cout << "zoom: " << mP.get_zoomLevel() << endl;
		cout << "Julia: ";
		if (mP.get_julia()) cout << "Yes, with seed " << real(juliaSeed) << " + " << imag(juliaSeed) << " * I" << endl;
		else cout << "no" << endl;
	}

	renderStartedEvent(static_pointer_cast<RenderInterface>(render), renderID);
	R.execute(); //after this the render is done
	bool cancelled = lastRenderID != renderID;

	if(debug) if(cancelled) cout << "createNewRender found that the render was cancelled" << endl;
	if ( ! cancelled)
		renderFinishedEvent(static_pointer_cast<RenderInterface>(render), renderID);

	{
		//Printing information after the render

		string elapsedString = to_string(R.getElapsedTime()) + " s";
		cout << "Elapsed time: " << elapsedString << endl;
		cout << "computed iterations: " << R.computedIterations << endl;
		cout << "iterations per second: " << ((uint64)(R.computedIterations / R.getElapsedTime()) / 1000000.0) << " M" << endl;
		cout << "used threads: " << R.usedThreads / 2 << endl; // divide by 2 because each thread is counted when created and when destroyed by addToThreadcount
		cout << "guessedPixelCount: " << R.guessedPixelCount << " / " << width * height << " = " << (double)R.guessedPixelCount / (width*height) << endl;
		cout << "calculatedPixelCount" << R.calculatedPixelCount << " / " << width * height << " = " << (double)R.calculatedPixelCount / (width*height) << endl;
		cout << "pixelGroupings: " << R.pixelGroupings << endl;
	}
}



class SimpleBitmapManager : public BitmapManager {
public:
	ARGB* ptPixels{ nullptr };

	ARGB* realloc(uint newScreenWidth, uint newScreenHeight) {
		ptPixels = (ARGB*)malloc(newScreenHeight * newScreenWidth * sizeof(ARGB));
		return ptPixels;
	}

	~SimpleBitmapManager() {
		free(ptPixels);
	}
};



void animation(
	string path
	,bool save_only_parameters
	,int skipframes
	,int framesPerInflection
	,int framesPerZoom
	,FractalCanvas& canvas
) {
	FractalParameters& P = canvas.Pmutable();

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
			writeParameters(canvas.P(), path + filename);
			return;
		}

		if (recalculate)
			canvas.createNewRender(); //the whole render takes place in this thread so after this the render is done
		else
			canvas.createNewBitmapRender(false);

		string filename = "frame" + num.str() + ".png";
		cout << "saving image " << filename << endl;
		saveImage(&canvas, path + filename, false);
	};


	P.setPreTransformation(7);
	vector<double_c> inflections = P.get_inflectionCoords();
	int inflectionCount = P.get_inflectionCount();
	while(P.removeInflection()); //reset to 0 inflections
	double_c originalCenter = P.get_center();
	P.setCenterAndZoomAbsolute(0, 0); //start the animation unzoomed and centered
	P.setPartialInflectionPower(1);
	P.setPartialInflectionCoord(0);

	double inflectionPowerStepsize = 1.0 / (framesPerInflection - 1);
	int frame = 1;

	makeFrame(frame++, true);		

	double_c centerTarget = inflectionCount > 0 ? inflections[0] : originalCenter;
	double_c currentCenter = P.get_center();
	double_c diff = centerTarget - currentCenter;

	//gradually move the center towards the next inflection location
	if (centerTarget != 0.0 + 0.0*I) {
		for (int i=1; i<=framesPerInflection; i++) {

			P.setCenter( currentCenter + diff * ((1.0 / framesPerInflection) * i) );

			makeFrame(frame++, true);
		}
	}
	P.setCenter(centerTarget);

	double currentZoom = P.get_zoomLevel();
	double targetZoom = P.get_inflectionZoomLevel() * (1 / pow(2, P.get_inflectionCount()));
	double zoomDiff = targetZoom - currentZoom;
	double zoomStepsize = 1.0 / framesPerZoom;

	//zoom to the inflection zoom level
	if (zoomStepsize > 0.001) {
		for (int i=1; i <= framesPerZoom * zoomDiff; i++) {

			P.setZoomLevel( currentZoom + zoomStepsize * i );

			makeFrame(frame++, true);
		}
	}

	for (int inflection=0; inflection < inflectionCount; inflection++)
	{
		P.setPartialInflectionPower(1);
		P.setPartialInflectionCoord(0);

		double_c thisInflectionCoord = inflections[inflection];
		
		double_c currentCenter = P.get_center();
		double_c diff = thisInflectionCoord - currentCenter;

		P.setPartialInflectionCoord(thisInflectionCoord);

		//gradually apply the next inflection by letting the inflection power go from 1 to 2, and meanwhile also gradually move to the next inflection location.
		for (int i=0; i<framesPerInflection; i++)
		{
			double zoom = (
				P.get_inflectionZoomLevel()
				* (1 / pow(2, P.get_inflectionCount()))
				* (1 / P.get_partialInflectionPower())
			);	
			P.setCenterAndZoomAbsolute(0, zoom);
			P.setCenter( currentCenter + diff * ((1.0 / (framesPerInflection-1)) * i) );

			makeFrame(frame++, true);

			P.setPartialInflectionPower( P.get_partialInflectionPower() + inflectionPowerStepsize);
		}
		P.addInflection(P.get_partialInflectionCoord());
	}

	//repeat the last frame
	for (int i=0; i<framesPerInflection; i++)
		makeFrame(frame++, false);

	P.setPreTransformation(0);
}



//variables related to command line options
bool render_animation = false;
bool render_image = false;
bool interactive = true;
string write_directory = "";
string parameterfile = "default.efp";
int fps = 60;
double secondsPerInflection = 3;
double secondsPerZoom = 0.6666666666666;
int skipframes = 0;
bool save_as_efp = false;


//utf16 to utf8 converter from https://stackoverflow.com/a/35103224/10336025
#if _MSC_VER >= 1900
std::string utf16_to_utf8(std::u16string utf16_string)
{
    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> convert;
    auto p = reinterpret_cast<const int16_t *>(utf16_string.data());
    return convert.to_bytes(p, p + utf16_string.size());
}
#else
std::string utf16_to_utf8(std::u16string utf16_string)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(utf16_string);
}
#endif

[[gnu::target("avx")]]
uint64 getFeatureMask() {
	return _xgetbv(0);
}

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR	 lpCmdLine,
	_In_ int	   nCmdShow
)
{
	//Hide the CMD window that windows always opens along with a console application. The fact that this has to be done is because of a restriction in windows.
	ShowWindow( GetConsoleWindow(), SW_HIDE );

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
		uint64 xcrFeatureMask = getFeatureMask();
		using_avx = (xcrFeatureMask & 0x6) == 0x6;
	}
	cout << "using AVX: " << (using_avx ? "Yes" : "No") << endl;


	FractalParameters defaultParameters;

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

	FractalParameters initialParameters = defaultParameters;

	if (save_as_efp && ! render_animation) {
		writeParameters(defaultParameters, write_directory + parameterfile);
	}
	else if (render_image || render_animation)
	{
		FractalCanvas canvas{ defaultParameters, NUMBER_OF_THREADS, make_shared<SimpleBitmapManager>(), {} };

		if (render_image)
		{		
			cout << "rendering image" << endl;
			canvas.createNewRender();
			saveImage(&canvas, write_directory + parameterfile + ".png");
		}
		if (render_animation)
		{
			int framesPerInflection = (int)(fps * secondsPerInflection);
			int framesPerZoom = (int)(fps * secondsPerZoom);
			cout << "rendering animation with" << endl;
			cout << fps << " fps" << endl;
			cout << framesPerInflection << " frames per inflection" << endl;
			cout << framesPerZoom << " frames per zoom" << endl;
			animation(write_directory, save_as_efp, skipframes, framesPerInflection, framesPerZoom, canvas);

			//this causes the parameters of the final frame of the animation to be used in the first tab if interactive is true
			initialParameters.fromParameters(canvas.P());
		}
	}

	if (!interactive) {
		return 0;
	}

	return GUI::GUI_main(defaultParameters, NUMBER_OF_THREADS, initialParameters);
}
