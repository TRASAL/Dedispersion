# Dedispersion

Many-core incoherent dedispersion algorithm in OpenCL, with classes to use them in C++.

## Publications

* Alessio Sclocco, Joeri van Leeuwen, Henri E. Bal, Rob V. van Nieuwpoort. _Real-time dedispersion for fast radio transient surveys, using auto tuning on many-core accelerators_. **Astronomy and Computing**, 2016, 14, 1-7. ([print](http://www.sciencedirect.com/science/article/pii/S2213133716000020)) ([preprint](http://alessio.sclocco.eu/pubs/sclocco2016.pdf)) ([arxiv](http://arxiv.org/abs/1601.01165))
* Alessio Sclocco, Joeri van Leeuwen, Henri E. Bal, Rob V. van Nieuwpoort. _A Real-Time Radio Transient Pipeline for ARTS_. **3rd IEEE Global Conference on Signal & Information Processing**, December 14-16, 2015, Orlando (Florida), USA. ([print](http://ieeexplore.ieee.org/xpl/freeabs_all.jsp?arnumber=7418239&abstractAccess=no&userType=inst)) ([preprint](http://alessio.sclocco.eu/pubs/sclocco2015a.pdf)) ([slides](http://alessio.sclocco.eu/pubs/Presentation_GlobalSIP2015.pdf))
* Alessio Sclocco, Henri E. Bal, Rob V. van Nieuwpoort. _Finding Pulsars in Real-Time_. **IEEE International Conference on eScience**, 31 August - 4 September, 2015, Munich, Germany. ([print](http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=7304280)) ([preprint](http://alessio.sclocco.eu/pubs/sclocco2015.pdf)) ([slides](http://alessio.sclocco.eu/pubs/Presentation_eScience2015.pdf))
* Alessio Sclocco, Henri E. Bal, Jason Hessels, Joeri van Leeuwen, Rob V. van Nieuwpoort. _Auto-Tuning Dedispersion for Many-Core Accelerators_. **28th IEEE International Parallel & Distributed Processing Symposium (IPDPS)**, May 19-23, 2014, Phoenix (Arizona), USA. ([print](http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=6877325)) ([preprint](http://alessio.sclocco.eu/pubs/sclocco2014.pdf))

# Installation

Set the `SOURCE_ROOT` environment variable to the location of the pipeline sourcode.
If this package is installed in `$HOME/Code/APERTIF/Dedispersion` this would be:

```bash
 $ export SOURCE_ROOT=$HOME/Code/APERTIF
```

Then build and test as follows:

```bash
 $ make
 $ make test # test run for a single configuration
 $ make tune # example tuning output
```

## Dependencies

* [utils](https://github.com/isazi/utils) - master branch
* [OpenCL](https://github.com/isazi/OpenCL) - master branch
* [AstroData](https://github.com/isazi/AstroData) - master branch

# Included programs

The dedispersion step is typically compiled as part of a larger pipeline, but this repo contains two example programs in the `bin/` directory to test and autotune a dedispersion kernel.

## DedispersionTest

Checks if the output of the CPU is the same for the GPU.
The CPU is assumed to be always correct.
Needs platform, data layout, and kernel configuration parameters (see below).

## DedispersionTune

Tune the dedispersion kernel's parameters by doing a complete sampling of the parameter space.
Kernel configuration and runtime statistics are written to stdout.
The commandline parameters are as above, except for the kernel configuration parameters.
Needs platform, data layout, and tuning parameters (see below).

The output can be analyzed using the python scripts in in the *analysis* directory.

## Commandline arguments

Description of common commandline arguments for the separate binaries.

### Compute platform specific arguments

 * *opencl_platform*     OpenCL platform
 * *opencl_device*       OpenCL device number
 * *input_bits*          number of bits used to represent a single input item
 * *padding*             cacheline size, in bytes, of the OpenCL device
 * *vector*              vector size, in number of input items, of the OpenCL device

### Data layout arguments

 * *channels*                Number of channels
 * *min_freq*                Frequency of first channel
 * *channel_bandwidth*       Mhz
 * *samples*                 Number of samples in a batch, ie. length of time dimension; should be divisible by *threads0*, *items0*, and *threads0 x items0*
 * *dms*                     Number of dispersion measures, ie. length of dm dimension; should be divisible by *threads1*, *items1*, and *threads1 x items1*
 * *dm_first*                Dispersion measure [m3/parsec?]
 * *dm_step*                 Dispersion measure step size [m3/parsec?]
 * *zapped_channels*         File containing tainted channels, or empty file
 * *split-seconds*           Optional. Sets a different way of treating the input: (not implemented in subband, unclear if it will be useful). Reduces data transfers but slows down computation.

    * default mode: data is continuous in memmory
    * split-seconds mode: data is blocked in bunches of 1 second
 *  *local*                  Defines OpenCL memmory space to use; ie. automatic or manual caching.

    * global [default]
    * local, local is often faster

### Kernel Configuration arguments

 *  *threads0*              Number of threads in dimension 0 (time)
 *  *threads1*              Number of threads in dimension 1 (dm)
 *  *items0*                Tiling factor in dimension 0: ie. the number of items per thread
 *  *items1*                Tiling factor in dimension 1: ie. the number of items per thread
 *  *unroll*                How far to unroll loops

### Tuning parameters

 * *iterations*          Number of samples for a given configuration.
 * *min_threads*         Minimum number of threads to use. Use this to reduce the parameter space.
 * *max_threads*         Limits on total number of threads
 * *max_items*           Maximum value on *item0 + item1*
 * *max_unroll*          Maximum value unroll parameter
 * *max_loopsize*        Some cards have problems with (too) large codes, this limits total kernel size.
 * *max_columns*         Limit on length of dimension 0
 * *max_rows*            Limit on length of dimension 1


# Analyzing tuning output

Kernel statistics can be saved to a database, and analyzed to find the optimal configuration.

## Setup

### MariaDB

Install mariadb, fi. via your package manager. Then:

0. log in to the database: ` $ mysql`
1. create a database to hold our tuning data: `create database AAALERT`
2. make sure we can use it (replace USER with your username): `grant all privileges on AAALERT.* to 'USER'@'localhost';`
3. copy the template configuration file: `cp analysis/config.py.orig analysis/config.py` and enter your configuration.

### Python

The analysis scripts use some python3 packages. An easy way to set this up is using `virtualenv`:

```bash
$ cd $SOURCE_ROOT/Dedispersion/analysis`
$ virtualenv --system-site-packages --python=python3 env`
$ . env/bin/activate`
```

And then install the missing packages:

```bash
$ pip install pymysql
```

## Run Analysis

The analysis is controlled by the `analysis/dedispersin.py` script.
It prints data as space-separated data to stdout, where you can plot it with fi. gnuplot, or copy-paste it in your favorite spreadsheet.
You can also write it to a file, that can then be read by the dedispersion code.

1. List current tables: `./dedispersion.py list`
2. Create a table: `./dedispersion.py create <table name>`
3. Enter a file create with DedispersionTuning into the database: `./dedispersion load <table name> <file name>`
4. Find optimal kernel configuration: `./dedispersion.py tune <table name> max <channels> <samples>`

The tune subcommand also takes a number of different parameters: `./dedispersion.py tune <table> <operator> <channels> <samples> [local|cache] [split|cont]`

 * operator: max, min, avg, std  (SQL aggergation commands)
 * channels: number of channels
 * samples: number of samples
 * local|cache  When specified, only consider local or cache kernels. See tuning document.
 * split|cont   When specified, only consider with or without the split_second option. See tuning document.

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

