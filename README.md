# ExploreFractals
A tool for testing the effect of Mandelbrot set Julia morphings

![image](https://user-images.githubusercontent.com/29734312/133895838-a5ac832e-eaa6-408e-92f4-947a28e0f351.png)

With this program you can test the effect of inflections / Julia morphings on the Mandelbrot set and some other fractal types. This works by transforming the plane and then actually rendering the fractal. Each click adds an inflection at the location of the cursor. You can also work with Julia sets. You can go somewhere in the M-set, then use "Toggle Julia" which uses the center of the screen as the seed for the Julia set.

### Download

See the releases: https://github.com/DinkydauSet/ExploreFractals/releases

### Characteristics

1. add a Julia morphing with just one mouse click
2. fast and responsive
3. not for deep zooming and perturbation
4. works in Wine
5. commandline options to render images and animations
6. can save and load parameter files in a human-readable JSON format
7. can render and save images with oversampling
10. limited to double precision floating point (I wish to improve that some day.)


### Commandline parameters

all available parameters in version 9:

```
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
```

examples:

```
ExploreFractals -p file.efp --animation --fps 60 --spi 3 --spz 0.6666 -o C:\folder -i
ExploreFractals -p name.efp --width 1920 --height 1080 --oversampling 2
```

### Explanation of some features

This text is also available in the program through menu option Other -> Help.

This program is made to test the effect of Julia morphings / inflections. A click adds an inflection at the clicked location in the fractal. An inflection is a transformation of the complex plane, which corresponds to how shapes in the Mandelbrot set are related to each other. Deeper shapes are Julia transformed versions of lesser deep shapes at the same zoom path.

##### Set inflection zoom

Sets the base zoom level to which the program resets after applying an inflection. Without using this setting, the zoomlevel goes back to 0 after creating an inflection. The zoom level is automatically corrected for the number of inflections, because an inflection halves the distance (the exponent in the magnification factor) to deeper shapes.

##### Coarse and fine

Both control gradient speed. Fine is just finer. The gradient speed controls the number of interpolated colors in the color gradient. Every pixel of the fractal is independently colored based on its iteration count. The color index in the gradient is calculated by (iterationcount of the pixel) mod (number of colors in the gradient).

##### Procedures

Checkers - A checkerboard pattern to test inflections on a tiling pattern, which can also be found in the Mandelbrot set. The circles are a crude simulation of "details" like in the Mandelbrot set.

Pure Julia morphings - starts as a completely empty plane. Add Julia morphings to transform the plane. The iteration count - which determines the color - in this procedure is the number of Julia morphings until the pixel escapes. The procedure iterates over the list of Julia morphings, instead of iterating the same formula all the time (as with the normal Mandelbrot set).

Triple matchmaker - A formula by Pauldelbrot at fractalforums.org. This fractal doesn't have a notion of escaping. The number of iterations is constant for every pixel. Therefore, changing the max iterations changes the result.

##### Keyboard shortcuts

CTRL + Z and CTRL + Y - move back and forward in the history  
CTRL + T - create a new tab  
CTRL + D - duplicate the current tab  
CTRL + W - close the current tab  
CTRL + TAB and CTRL + SHIFT + TAB - go to the next or previous tab

##### Using a default parameter file

Use the program to save a parameter file called default.efp to the directory from where you start the program. The program will use the parameters as default for new tabs.

##### Using commandline parameters

The program also has commandline parameters to render images and Julia morphing animations. Start the program from a commandline like this to get help:
ExploreFractals.exe --help

### Compiling the code

The only file that needs to be compiled is ExploreFractals.cpp. As of version 9, the program needs to be linked to the Nana GUI library. More information: https://github.com/DinkydauSet/ExploreFractals/releases/tag/9.0

To compile with GCC:

> g++ ExploreFractals.cpp -std=c++17 -s -static -m64 -O2 -ffast-math -I. -L. -lnana -lgdi32 -lcomdlg32 -D NDEBUG -o ExploreFractals.exe

More information about compiling with GCC: https://github.com/DinkydauSet/ExploreFractals/wiki/Compiling-with-GCC

I use visual studio 2017 for development. The program can be compiled with visual studio. I don't remember how I got the project set up so I can't help with that.

The code here on github includes some libraries:

| name | use |
| -- | -- |
| rapidjson | to save and load parameter files in JSON format |
| lodepng | to save PNG images |
| gcem | for constexpr math functions |

All of the libraries mention that they allow to be redistributed. I do so to make it easier to compile the code.
