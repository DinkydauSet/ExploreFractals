# ExploreFractals
A tool for testing the effect of Mandelbrot set Julia morphings

![ExploreFractals](https://user-images.githubusercontent.com/29734312/71610778-2db1c280-2b94-11ea-97b3-6f66829097bb.png)

With this program you can test the effect of inflections / Julia morphings on the Mandelbrot set with powers 2, 3, 4 and 5. This works by transforming the plane and then actually rendering the fractal. Each click adds an inflection at the location of the cursor. You can also work with Julia sets. You can go somewhere in the M-set, then use "Toggle Julia" which uses the center of the screen as the seed for the Julia set.

### Download

See this forum thread at fractalforums: https://fractalforums.org/other/55/explore-fractals-inflection-tool/777

You can also find more information about the program there.

### Compiling the code

The program was compiled with visual studio 2017. The only file that needs to be compiled is ExploreFractals.cpp. I have included the rapidjson library with the code. Other resources such as intrin.h for usage of AVX instructions are available on my computer but I don't know where they come from. Therefore I don't know what is needed exactly to compile this code. I don't have enough time to investigate that. One of my plans is to compile the program with GCC in order to find out what exactly is required to compile. I also suspect there may be some visual studio-specific code, as I've read that visual studio has some slight deviations from the c++ standard. I want to get rid of any incompatibilities eventually when I have enough time.
