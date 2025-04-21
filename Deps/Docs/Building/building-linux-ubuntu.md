# Compiling Xenon with Ubuntu
## Prerequisites
A build environment suited for SDL3. Please view the document [here](https://github.com/libsdl-org/SDL/blob/b8c2bc143e697a49b0c3924cde465950b96320d5/docs/README-linux.md)
## Dependencies
#### Currently, we depend on CMake, make, gcc, build-essential, and mesa-utils
##### I shouldn't have to mention this, but PLEASE make sure your Ubuntu install is up to date! `sudo apt update && sudo apt upgrade`
```bash
  sudo apt-get install build-essential cmake make git gcc mesa-utils
```
## Starting a build
Starting a build for Ubuntu or any standard distro is fairly straightforward.
```bash
  cmake -B "build" -G "Unix Makefiles"
  cd build
  make
```