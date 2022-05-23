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

//standard library
#include <fstream>

//this program
#include "common.cpp"

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

class SimpleBitmapManager : public BitmapManager {
public:
	ARGB* ptPixels{ nullptr };

	ARGB* realloc(uint width, uint height) {
		ptPixels = (ARGB*)malloc(width * height * sizeof(ARGB));
		return ptPixels;
	}

	~SimpleBitmapManager() {
		free(ptPixels);
	}
};

template <typename T>
struct nullable {
	T v;
	bool isnull = true;

	void set(T v) {
		this->v = v;
		isnull = false;
	}
	void null() { isnull = true; }
};

template <typename T>
void printvector(vector<T> v)
{
	for (int i=0; i<v.size(); i++) {
		cout << v[i] << ", ";
	}
	cout << endl;
}

//used for debugging. Using a mutex makes text printed from multiple threads more readable because it's printed one line at a time.
template<typename... Ts>
void mtxprint(Ts... args) {
	static int use;
	static mutex m;
	if constexpr(debug) {
		lock_guard<mutex> guard(m);
		use++;
		((cout << use << ": ") << ... << args) << endl;
	}
}

//
// The most obvious way to reinterpret memory is this:
// *(To*)(&From)
// It interprets the memory address of a From as a To, and takes the value at that address,
// but that's undefined behavior because of the strict aliasing rule:
// https://stackoverflow.com/questions/98650/what-is-the-strict-aliasing-rule
//
// This also exists in c++20 as bit_cast
template <typename To, typename From>
inline To bitcast(From from) {
    static_assert(sizeof(From) == sizeof(To), "types are not the same size");
    To to;
    memcpy(&to, &from, sizeof(From));
    return to;
}

#endif