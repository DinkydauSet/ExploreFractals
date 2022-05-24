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

#ifndef TEST_H
#define TEST_H

#include "common.cpp"
#include "WorkDistribution.cpp"

//unit tests:

void dotest(string name, std::function<void(void)> test) {
	auto start = chrono::high_resolution_clock::now();

	test();

	auto end = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed = end - start;
	cout << "Test " << name << " completed in " << elapsed.count() << " seconds" << endl;
}

bool operator==(const point& a, const point& b)
{
	return a.x == b.x && a.y == b.y;
}

void testfunction()
{
	if constexpr(debug) {
		dotest("spiraler farthest neighbors", []
		{
			//With and height are assumed to be at least 3. //todo: check for this when a render starts

			for (uint xmax = 3; xmax<10; xmax++)
			for (uint ymax = 3; ymax<10; ymax++)
			{
				spiraler s(xmax, ymax);
				s.advance();

				//while not at the boundary of the spiral
				while (s.pos.x != 0 && s.pos.y != 0 && s.pos.x != xmax-1 && s.pos.y != ymax-1)
				{
					point neighbor_below = s.pos;
					      neighbor_below.y += 1;
					point neighbor_above = s.pos;
					      neighbor_above.y -= 1;
					point neighbor_right = s.pos;
					      neighbor_right.x += 1;
					point neighbor_left = s.pos;
					      neighbor_left.x -= 1;

					int steps = s.farthest_neighbor_steps();


					//check that after steps steps there is indeed a neigbor
					spiraler copy = s;
					for (int i=0; i<steps; i++)
						copy.advance();
					assert((
						copy.pos == neighbor_above
						|| copy.pos == neighbor_below
						|| copy.pos == neighbor_left
						|| copy.pos == neighbor_right
					));
					point farthest_neighbor = copy.pos;


					//check that all other neighbors come before it
					spiraler check(xmax, ymax);
					check.advance();

					int found_before = 0;
					while(true)
					{
						if (check.pos == farthest_neighbor) {
							assert(found_before == 3);
							break;
						}
						else {
							if (
								check.pos == neighbor_above
								|| check.pos == neighbor_below
								|| check.pos == neighbor_left
								|| check.pos == neighbor_right
							) {
								found_before++;
							}
							check.advance();
						}
					}

					s.advance();
				}
				
			}
		});
	}
}


#endif // TEST_H
