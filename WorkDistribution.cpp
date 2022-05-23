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

#ifndef WORKDISTRIBUTION_H
#define WORKDISTRIBUTION_H

//standard library
#include <queue>
#include <stack>

//this program
#include "common.cpp"
#include "utilities.cpp"
#include "FractalCanvas.cpp"

#include <cstdio>  

// The advance function of this class iterates through all points on the canvas in a spiral-shaped order, starting in the center, spiraling outwards.
class spiraler {
public:
	enum direction {
		left, right, up, down
	};

	//These values keep track of when the direction should turn to the right.
	uint max_up;
	uint max_down;
	uint max_left;
	uint max_right;
	
	point pos;
	direction d;

	spiraler(uint xmax, uint ymax)
	{
		//odd height, not taller than wide
		//(ymax % 2 == 0 means odd height because point indices start at 0)
		if (ymax % 2 == 0 && xmax >= ymax)
		{
			//first assume square with width and height ymax
			pos.x = ymax / 2;
			pos.y = ymax / 2;

			max_up = pos.y - 1;
			max_down = pos.y + 1;
			max_left = pos.x - 1;
			max_right = pos.x + 1;

			d = direction::up;

			//if it's not a square after all, make a correction
			if (xmax > ymax) {
				uint diff = xmax - ymax;
				pos.x += diff;
				max_right += diff;
				max_left += 1; //causes the direction to turn to up where it would start in case xmax == ymax
				d = direction::left;
			}
		}
		//even height, not taller than wide
		else if (ymax % 2 == 1 && xmax >= ymax)
		{
			//first assume square with width and height ymax
			pos.x = ymax / 2 + 1;
			pos.y = ymax / 2;

			max_up = pos.y - 1;
			max_down = pos.y + 1;
			max_left = pos.x - 1;
			max_right = pos.x + 1;

			d = direction::down;

			//if it's not a square after all, make a correction
			if (xmax > ymax) {
				uint diff = xmax - ymax;
				max_right += diff - 1;
				d = direction::right;
			}
		}
		//taller than wide, odd width
		else if (ymax > xmax && xmax % 2 == 0)
		{
			//first treat it as a square with width and height xmax
			pos.x = xmax / 2;
			pos.y = xmax / 2;

			max_up = pos.y - 1;
			max_down = pos.y + 1;
			max_left = pos.x - 1;
			max_right = pos.x + 1;

			d = direction::up;

			//correction (direction remains the same)
			uint diff = ymax - xmax;
			pos.y += diff;
			max_down += diff;
		}
		//taller than wide, even width
		else if (ymax > xmax && xmax % 2 == 1)
		{
			//first treat it as a square with width and height xmax
			pos.x = xmax / 2 + 1;
			pos.y = xmax / 2;

			max_up = pos.y - 1;
			max_down = pos.y + 1;
			max_left = pos.x - 1;
			max_right = pos.x + 1;

			d = direction::down;

			//correction
			uint diff = ymax - xmax;
			max_down += diff;
		}
		else {
			assert(false);
		}

		//move the starting location one backwards, so that the first call of advance results in pos being the starting location
		switch (d) {
			break; case direction::up:    pos.y += 1;
			break; case direction::down:  pos.y -= 1;
			break; case direction::left:  pos.x += 1;
			break; case direction::right: pos.x -= 1;
		}
	}

	//The user is responsible not to call this when the end is reached.
	void advance() {
		assert(pos.x != 0 || pos.y != 0); //check that the end is not reached
		switch (d)
		{
			break; case direction::up: {
				pos.y--;
				if (pos.y == max_up) {
					d = direction::right;
					max_up--;
				}
			}
			break; case direction::right: {
				pos.x++;
				if (pos.x == max_right) {
					d = direction::down;
					max_right++;
				}
			}
			break; case direction::down: {
				pos.y++;
				if (pos.y == max_down) {
					d = direction::left;
					max_down++;
				}
			}
			break; case direction::left: {
				pos.x--;
				if (pos.x == max_left) {
					d = direction::up;
					max_left--;
				}
			}
		}
	}

	//Finds which of pos's neighbors (above, below, left and right) is farthest in the spiral and returns the number of steps (advances) to get there.
	//This is the numbers of steps for one full rotation, starting from the current position.
	int farthest_neighbor_steps()
	{
		return 2*(max_right - max_left + max_down - max_up) - 1;
	}
};

//todo: this is unfinished
class WorkDistribution
{
public:
	mutex work_distribution;
	spiraler spiral;

	vector<IterData> phase16;
	vector<IterData> phase8;
	vector<IterData> phase4;
	vector<IterData> phase2;
	vector<IterData> phase1;

	WorkDistribution(uint width, uint height, bool guessing)
		: spiral(width, height)
	{
		uint xmax = width-1;
		uint ymax = height-1;
		//spiral = spiraler(
	}

	bool getWork()
	{
		lock_guard<mutex> guard(work_distribution);
	}
};


#endif