# Dedispersion

Many-core incoherent dedispersion algorithm in OpenCL, with classes to use them in C++.

## Publications

* Alessio Sclocco, Henri E. Bal, Rob V. van Nieuwpoort. _Finding Pulsars in Real-Time_. **IEEE International Conference on eScience**, 31 August - 4 September, 2015, Munich, Germany. ([print](http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=7304280)) ([preprint](http://alessio.sclocco.eu/pubs/sclocco2015.pdf)) ([slides](http://alessio.sclocco.eu/pubs/Presentation_eScience2015.pdf))
* Alessio Sclocco, Henri E. Bal, Jason Hessels, Joeri van Leeuwen, Rob V. van Nieuwpoort. _Auto-Tuning Dedispersion for Many-Core Accelerators_. **28th IEEE International Parallel & Distributed Processing Symposium (IPDPS)**, May 19-23, 2014, Phoenix (Arizona), USA. ([print](http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=6877325)) ([preprint](http://alessio.sclocco.eu/pubs/sclocco2014.pdf))

# Installation

Set the `APERTIF_ROOT` environment variable to the location of the pipeline sourcode.
If this package is installed in `$HOME/Code/APERTIF/Dedispersion` this would be:

```bash
 $ export APERTIF_ROOT=$HOME/Code/APERTIF
```

Then build and test as follows:

```bash
 $ make
 $ make test # test run for a single configuration
 $ make tune # example tuning output
```

## Dependencies

* [AstroData](https://github.com/isazi/AstroData) - master branch
* [OpenCL](https://github.com/isazi/OpenCL) - master branch
* [utils](https://github.com/isazi/utils) - master branch

# Included programs

The dedispersion step is typically compiled as part of a larger pipeline, but this repo contains two example programs in the `bin/` directory to test and autotune a dedispersion kernel.

## DedispersionTest

Checks if the output of the CPU is the same for the GPU.
The CPU is assumed to be always correct.

Platform specific arguments:

 * *opencl_platform*     OpenCL platform
 * *opencl_device*       OpenCL device number
 * *input_bits*          number of bits of the input
 * *padding*             number of elements in the cacheline of the platform
 * *vector*              vector size in number of elements
 * *zapped_channels*     file containing tainted channels, or empty file

Data input arguments:

 * *channels*                Number of channels
 * *min_freq*                Frequency of first channel
 * *channel_bandwidth*       Mhz
 * *samples*                 Number of samples in a batch, ie. length of time dimension; should be divisible by *threads0*, *items0*, and *threads0 x items0*
 * *dms*                     Number of dispersion measures, ie. length of dm dimension; should be divisible by *threads1*, *items1*, and *threads1 x items1*
 * *dm_first*                Dispersion measure [m3/parsec?]
 * *dm_step*                 Dispersion measure step size [m3/parsec?]

Kernel Configuration arguments:

 *  *split-seconds*         Optional. Sets a different way of treating the input: (not implemented in subband, unclear if it will be useful). Reduces data transfers but slows down computation.

    * standard mode data is continuous in memmory
    * split-seconds mode: data is blocked in bunches of 1 second 

 *  *local*                 Defines OpenCL memmory space to use; ie. automatic or manual caching.

    * global [default]
    * local, local is often faster

 *  *threads0*              Number of threads in dimension 0 (time)
 *  *threads1*              Number of threads in dimension 1 (dm)
 *  *items0*                Tiling factor in dimension 0: ie. the number of items per thread
 *  *items1*                Tiling factor in dimension 1: ie. the number of items per thread
 *  *unroll*                How far to unroll loops


## DedispersionTune

Tune the dedispersion kernel's parameters by doing a complete sampling of the parameter space.
Kernel configuration and runtime statistics are written to stdout.
The commandline arguments are as above, except for the kernel configuration arguments.
These are replaced by the folling options to define the parameter space:

 * *iterations*          Number of samples for a given configuration. 
 * *min_threads*         Minimum number of threads to use. Use this to reduce the parameter space.
 * *max_threads*         Limits on total number of threads
 * *max_items*           Maximum value on *item0 + item1*
 * *max_unroll*          Maximum value unroll parameter
 * *max_loopsize*        Some cards have problems with (too) large codes, this limits total kernel size.
 * *max_columns*         Limit on length of dimension 0
 * *max_rows*            Limit on length of dimension 1

## analysis.py (TODO)

Python program to store, analyse, and plot the output of the tuning.

# Included classes

## configuration.hpp
The code is based on templates, for running the test pipeline we need to define some actual types.
This file contains the datatypes used by this package.

## Shifts.hpp
Contains `getShifts()` that returns for each frequently channel the shift part without the dispersion measure (dm).

## Dedispersion.hpp
Classses holding the implementation of the kernels for CPU and GPU.


# License

Licensed under the Apache License, Version 2.0.

