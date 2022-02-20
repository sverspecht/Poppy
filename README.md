# Poppy
An non-domain-specific morph algorithm that doesn't require manual keypoints

![Face morphing demo](https://github.com/kallaballa/Poppy/blob/main/demo/faces.gif?raw=true)

This is very much a work in progress. And works best on similar objects from a similar perspective.
Please note that it isn't (yet) rotation or scale invariant, so you have to scale and rotate the source images to match each other.
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

## Run

```bash
    cd Poppy
    src/poppy img1.png img2.png imgN.png
```
hf