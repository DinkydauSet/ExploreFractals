# ExploreFractals
A tool for testing the effect of Mandelbrot set Julia morphings

![ExploreFractals](https://user-images.githubusercontent.com/29734312/71610778-2db1c280-2b94-11ea-97b3-6f66829097bb.png)

With this program you can test the effect of inflections / Julia morphings on the Mandelbrot set with powers 2, 3, 4 and 5. This works by transforming the plane and then actually rendering the fractal. Each click adds an inflection at the location of the cursor. You can also work with Julia sets. You can go somewhere in the M-set, then use "Toggle Julia" which uses the center of the screen as the seed for the Julia set.

Forum thread at fractalforums for this program with more information: https://fractalforums.org/other/55/explore-fractals-inflection-tool/777

All the code that really does something is in ExploreFractals.cpp. I intentionally used only one file to make it easy to copy and paste the code, and the size is still manageable. In the beginning I compiled it with GCC and then moved to visual studio, which introduced the other files, but if I delete them it doesn't work anymore.

![Environment screenshot](https://user-images.githubusercontent.com/29734312/71610752-ea575400-2b93-11ea-99fb-e6a266c5e90d.png)
