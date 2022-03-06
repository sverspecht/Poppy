# Poppy
An automatic and non-domain-specific image morphing tool that doesn't require manual keypoints.

This is very much a work in progress. And works best on similar objects from a similar perspective.
Please note that it isn't (yet) rotation or scale invariant, **so you have to scale and rotate the source images to match each other**.

## Demos
The following demos are very low quality renderings of what Poppy can do. If you are truly curious, you should really try it yourself! It is quick and easy to do. Check out [this script](https://github.com/kallaballa/Poppy/blob/main/make_demos.sh) to see how the demos are generated.

![Car morphing](https://github.com/kallaballa/Poppy/blob/main/demo/cars.gif?raw=true)
![Morphing face features](https://github.com/kallaballa/Poppy/blob/main/demo/browns.gif?raw=true)
![Morphing faces](https://github.com/kallaballa/Poppy/blob/main/demo/faces.gif?raw=true)
![Yale faces morphing](https://github.com/kallaballa/Poppy/blob/main/demo/yalefaces.gif?raw=true)
![Flower morphing to another flower](https://github.com/kallaballa/Poppy/blob/main/demo/flowers.gif?raw=true)
![Square morphing to a circle](https://github.com/kallaballa/Poppy/blob/main/demo/squarecircle.gif?raw=true)
![Morphing from one number to the next](https://github.com/kallaballa/Poppy/blob/main/demo/numbers.gif?raw=true)
![Morphing from a cat to a dog](https://github.com/kallaballa/Poppy/blob/main/demo/catdog.gif?raw=true)

(Yale face images taken from http://vision.ucsd.edu/content/yale-face-database)

## Dependencies
* boost
* libboost-program-options
* opencv4

## Build

```bash
    git clone https://github.com/kallaballa/Poppy.git
    cd Poppy
    make
```

## Usage
```
Usage: poppy [options] <imageFiles>...
Default options will work fine on good source material. Please,
always make sure images are scaled and rotated to match each
other. Anyway, there are a couple of options you can specifiy.
But usually you would only want to do this if you either have
bad source material, feel like experimenting or are trying to
do something funny.

Options:
  -g [ --gui ]                        Show analysis windows
  -m [ --maxkey ] arg (=-1)           Manual override for the number of 
                                      keypoints to retain during detection. The
                                      default is automatic determination of 
                                      that number.
  -f [ --frames ] arg (=60)           The number of frames to generate.
  -t [ --tolerance ] arg (=1)         How tolerant poppy is when matching 
                                      keypoints.
  -a [ --angloss ] arg (=0)           The target loss, in percent, for the 
                                      angle test.
  -l [ --lenloss ] arg (=0)           The target loss, in percent, for the 
                                      length test.
  -c [ --contour ] arg (=2)           How sensitive poppy is to contours.
  -o [ --outfile ] arg (=output.mkv)  The name of the video file to write to.
  -h [ --help ]                       Print the help message.
```

## Run

```bash
    cd Poppy
    src/poppy img1.png img2.png imgN.png
```

## Have fun!
