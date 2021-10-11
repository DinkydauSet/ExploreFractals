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

#ifndef UTILITIES_H
#define UTILITIES_H

//this program
#include "common.cpp"
#include "FractalParameters.cpp"
#include "FractalCanvas.cpp"

//standard library
#include <fstream>

bool writeParameters(const FractalParameters& P, string fileName) {
	bool succes = true;
	cout << "Writing parameters" << endl;
	ofstream outfile(fileName);
	if (outfile.is_open()) {
		outfile << P.toJson(); //converts parameter struct to JSON
		outfile.close();
	}
	else {
		cout << "opening outfile failed" << endl;
		succes = false;
	}
	return succes;
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

void saveImage(FractalCanvas* canvas, string filename, bool cleanup = true) {
	uint screenWidth = canvas->P().get_screenWidth();
	uint screenHeight = canvas->P().get_screenHeight();

	/*
		This loop performs conversion.

		PNG requires RGBA-values in big-endian order. The colors in this program are ARGB stored in little-endian order. Lodepng interprets the data correctly when delivered as ABGR (the reserve order of RGBA) because of the endianness difference. This comes down to swapping red and blue.
		 I convert the colors to ABGR in the original array to reduce memory usage. That means that after saving the PNG, the colors may need to be reverted to their original values.
	*/
	for (uint x=0; x<screenWidth; x++) {
		for (uint y=0; y<screenHeight; y++)
		{
			uint pixelIndex = canvas->pixelIndex_of_pixelXY(x, y);
			ARGB& pixel = canvas->ptPixels[pixelIndex];
			
			ARGB r,g,b;
			r = (pixel & 0x00ff0000);
			g = (pixel & 0x0000ff00);
			b = (pixel & 0x000000ff);

			pixel = (
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
		,(uint8*)canvas->ptPixels	//image data to encode
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
		canvas->createNewBitmapRender(false);
	}
}

enum class ReadResult {
	succes,
	parseError,
	fileError
};

/*
	Stores the values in the json string in P. It first tries to change values in a copy of P so that when parsing fails, P is not left in an inconsistent state.
*/
ReadResult readParametersJson(FractalParameters& P, string& json) {
	FractalParameters newS = P;
	if ( ! newS.fromJson(json)) {
		return ReadResult::parseError;
	}
	P = newS;
	return ReadResult::succes;
}

/*
	Reads JSON from the file and stores the values in P.
*/
ReadResult readParametersFile(FractalParameters& P, string fileName) {
	ReadResult ret;
	{
		ifstream infile;
		infile.open(fileName);
		if (infile.is_open()) {
			stringstream strStream; strStream << infile.rdbuf();
			string json = strStream.str();
			ret = readParametersJson(P, json); //this changes P if succesful
		}
		else {
			ret = ReadResult::fileError;
		}
		infile.close();
	}
	return ret;
}

#endif