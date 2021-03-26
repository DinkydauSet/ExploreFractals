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
4. can save and load parameter files in a human-readable JSON format
5. can render and save images with oversampling
6. works in Wine
7. can be used to render images and animations as a headless commandline tool
8. can also be used to explore unmorphed fractals
9. not for deep zooming and perturbation
10. limited to double precision floating point (I wish to improve that some day.)


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
