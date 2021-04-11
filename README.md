# ExploreFractals
A tool for testing the effect of Mandelbrot set Julia morphings

![image](https://user-images.githubusercontent.com/29734312/112564228-2d29cb00-8ddb-11eb-99ab-17f434847902.png)

With this program you can test the effect of inflections / Julia morphings on the Mandelbrot set with powers 2, 3, 4 and 5. This works by transforming the plane and then actually rendering the fractal. Each click adds an inflection at the location of the cursor. You can also work with Julia sets. You can go somewhere in the M-set, then use "Toggle Julia" which uses the center of the screen as the seed for the Julia set.

### Download

See this forum thread at fractalforums: https://fractalforums.org/other/55/explore-fractals-inflection-tool/777

You can also find more information about the program there.

### Characteristics

1. add a Julia morphing with just one mouse click
2. fast and responsive
3. not for deep zooming and perturbation
4. can save and load parameter files in a human-readable JSON format
5. can render and save images with oversampling
6. can be used to render images and animations as a headless commandline tool
7. works in Wine
8. can also be used to explore unmorphed fractals
9. limited to double precision floating point (I wish to improve that some day.)


### Commandline parameters

all available parameters:

```
    -p name.efp     use the file name.efp as initial parameters. default: default.efp
    --width         override the width parameter
    --height        override the height parameter
    --oversampling  override the oversampling parameter
    --image         render the initial parameter file to an image
    --animation     render an animation of the initial parameters
    --fps number    the number of frames per second (integer)
    --spi number    the number of seconds per inflection (floating point)
    --spz number    the number of seconds per zoom (floating point)
    -o directory    use the directory as output directory for animation frames (example: C:\folder)
    -i              do not close the program after rendering an image or animation to continue interactive use
    --help or -h    show this text
```

examples:

```
    ExploreFractals -p file.efp --animation --fps 60 --spi 3 --spz 0.6666 -o C:\folder -i
    ExploreFractals -p name.efp --width 1920 --height 1080 --oversampling 2
```

### Compiling the code

The program was compiled with visual studio 2017. The only file that needs to be compiled is ExploreFractals.cpp. Other resources such as intrin.h for usage of AVX instructions are available on my computer but I don't know where they come from. Therefore I don't know what is needed exactly to compile this code. I don't have enough time to investigate that. One of my plans is to compile the program with GCC in order to find out what exactly is required to compile. I also suspect there may be some visual studio-specific code, as I've read that visual studio has some slight deviations from the c++ standard. I want to get rid of any incompatibilities eventually when I have enough time.

### Changelog

#### Version 8 (2021-04-12)

Changes:
1. Images are saved as PNG. I used the library lodePNG for that.
2. New fractal type "Pure Julia morphings" under "More", which starts with a monocolored plane. Colors are based only on applied Julia morphings.
3. Memory usage has been reduced by 1/3 (with no oversampling) up to 1/2 of the previous amount (with high oversampling), at the cost of the maximum iterationcount now being 2^30 = 1,073,741,824 instead of 2^32.
4. Animations are a little different (smoother).
5. Extra parameters:
    --skipframes    number of frames to skip (for example to continue an unfinished animation render)
    --efp           save the parameters instead of rendering to an image (can be used to convert old parameter files or to store parameter files for every frame in an animation)
6. Option "View Regular colors" to undo the effect of "View guessed pixels"

Fixed problems:
1. maxIters was not read from the parameter file
2. maxIters was used for triple matchmaker causing a black region, which is not correct. Triple matchmaker has no notion of maximum iterations.
3. crash in some cases when loading a parameter file with many inflections
4. blank first frame in animations
5. crash when requesting too much memory. This has been replaced by an error message.
6. crash when using a low resolution such as 2×2
7. removed an unnecessary limitation of 2^31 points
8. better handling of some other edge cases
9. opening a new options window now closes the previous one


#### Version 7.1 (2021-03-26)

Zooming didn't work with oversampling. That's fixed in this version.

#### Version 7 (2021-03-26)

Changes
   1. Rotation is available. There are 2 menu buttons to rotate left and right. You can set a finer rotation by editing the JSON. There are also shotcuts for the buttons: alt+R to rotate right and alt+L to rotate left.
   2. A negative gradient speed can be set. The slider in the options window has a range that includes negative values. I found this useful when using gradients with many colors (4096 in my case). Increasing the gradient speed stretches the gradient. A negative gradient speed shrinks the gradient.

Fixed problems:
   1. increasing oversampling in the JSON window crashes the program

#### Version 6 (2021-03-05)

Changes
   1. Oversampling has better performance and works in wine (by not using the windows api StretchBlt function)
   2. All formulas other than Mandelbrot power 2 are faster (by using templates)
   3. There are command line parameters for rendering animations of Julia morphings and images.
   4. The gradient (colors) is now included in the parameters. You can change the gradient by editing the JSON. There is no graphical way to change the colors.
   5. Multithreaded coloring (when changing the gradient) which makes changing the gradient a lot faster when using a CPU with many cores.
   6. You can place a parameter file named default.efp in the directory from which you start the program to start with custom parameters.
   7. Extra option to save both parameters and an image, and while saving a file the dialog suggests a filename based on the time and formula.

Solved problems:
   1. The complex number in the status bar was calculated without inflections applied.
   2. Resizing during a render is now possible. The active render will be cancelled first.

Added problems:
   1. parameter files from version 4.0 and lower can't be loaded anymore. To use them you can load and re-save with version 5.0.

#### Version 5 (2019-12-09)

Changes
1. Every fractal type uses multithreading, and every fractal type that allows pixel guessing uses it.
2. Parameters are saved in JSON format so that they can be easily read and changed by humans and other programs. Old parameter files can still be loaded.
3. There's a new JSON window where you can view and edit the current parameters of the program.
4. There's a status bar that shows progress, elapsed time and the complex number and iteration count of the pixel below the cursor.
5. More efficient inflection calculation for Mandelbrot power 2
6. The options window is more compact.
7. In the options window you can set the current zoom level as "inflection zoom". Normally when an inflection is set the zoom level gets reset to 0. This sets that "reset zoom level" to the current zoom level. However, a correction factor of 1/2 is used for every inflection set, to keep the pattern the same size. The reason is: An inflection halves the distance (the power of the magnification factor) to stuff lying deeper below.

Fixes for problems:
1. The width and height were not saved correctly when oversampling was used.
2. The program doesn't crash anymore when it's instructed to change the resolution during a render. Instead of crashing, it shows an error.
3. No more delay between the render being finished and being shown on screen. It makes the experience a little smoother.

#### Version 4 (2019-10-05)

Changes:
1. AVX instructions are used for the Mandelbrot power 2 formula and its Julia sets. It's almost 2 times as fast.
2. The reset button also removes all inflections.
3. When anti-aliasing is used, images are saved with anti-aliasing applied.
4. There is an option to cancel the render in the menu called "More".
5. The options window is no longer always on top.
6. experimental fractal type "Triple Matchmaker" under the menu "More"

Fixes for problems:
1. The location of added inflections is correct when using anti-aliasing.
2. The program is faster because an accidental inefficiency has been removed that lead to some pixels being calculated multiple times.

#### Version 3 (2019-07-27)

Changes:
1. You can save and load parameter files.
2. You can save the image as BMP.
3. The program uses the maximum number of threads more often by
3.1. keeping track of the number of active threads and creating a new thread with work that's left to do when a thread is done, and
3.2. calculating the boundaries of the first tiles with the maximum number of threads as well, instead of just 1.

#### Version 2 (2019-04-05)

Changes:
1. options window with various settings among which: image size, oversampling, gradient speed and offset
2. more efficiency (many changes there), especially Mandelbrot with power 2
3. checkerboard "formula" to simulate tilings and an extremely high power Mandelbrot set (power is 33554432 = 2^25) (slow)
4. changes with the intention of improving the reliability, responsiveness and flexibility of the program for later
5. some small changes such as a different gradient

#### Version 1 (2018-01-30)

initial release

There are some features that don't work. The program is in an unfinished state, but Julia morphing works. The precision is limited to that of the double datatype (a little less than 64 bits). It's also slow, compared to fractal extreme, for example. I don't know how to improve the speed.
