// Copyright 2014 Alessio Sclocco <a.sclocco@vu.nl>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>

#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <InitializeOpenCL.hpp>
#include <Kernel.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
#include <utils.hpp>
#include <Timer.hpp>
#include <Stats.hpp>

typedef float dataType;
string typeName("float");


int main(int argc, char * argv[]) {
  bool localMem = false;
	unsigned int nrIterations = 0;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	unsigned int minThreads = 0;
	unsigned int maxThreadsPerBlock = 0;
	unsigned int maxItemsPerThread = 0;
	unsigned int maxColumns = 0;
	unsigned int maxRows = 0;
  AstroData::Observation< dataType > observation("DedispersionTuning", typeName);

	try {
    isa::utils::ArgumentList args(argc, argv);

		nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
    localMem = args.getSwitch("-local");
		observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
		minThreads = args.getSwitchArgument< unsigned int >("-min_threads");
		maxThreadsPerBlock = args.getSwitchArgument< unsigned int >("-max_threads");
		maxItemsPerThread = args.getSwitchArgument< unsigned int >("-max_items");
		maxColumns = args.getSwitchArgument< unsigned int >("-max_columns");
		maxRows = args.getSwitchArgument< unsigned int >("-max_rows");
		observation.setMinFreq(args.getSwitchArgument< float >("-min_freq"));
		observation.setChannelBandwidth(args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
		observation.setNrChannels(args.getSwitchArgument< unsigned int >("-channels"));
		observation.setNrDMs(args.getSwitchArgument< unsigned int >("-dms"));
		observation.setFirstDM(args.getSwitchArgument< float >("-dm_first"));
		observation.setDMStep(args.getSwitchArgument< float >("-dm_step"));
		observation.setMaxFreq(observation.getMinFreq() + (observation.getChannelBandwidth() * (observation.getNrChannels() - 1)));
	} catch ( isa::utils::EmptyCommandLine & err ) {
		std::cerr << argv[0] << " -iterations ... -opencl_platform ... -opencl_device ... [-local] -padding ... -min_threads ... -max_threads ... -max_items ... -max_columns ... -max_rows ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	} catch ( std::exception & err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}

	// Initialize OpenCL
	cl::Context * clContext = new cl::Context();
	std::vector< cl::Platform > * clPlatforms = new std::vector< cl::Platform >();
	std::vector< cl::Device > * clDevices = new std::vector< cl::Device >();
	std::vector< std::vector< cl::CommandQueue > > * clQueues = new std::vector< std::vector < cl::CommandQueue > >();

  isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, clContext, clDevices, clQueues);
  std::vector< unsigned int > * shifts = PulsarSearch::getShifts(observation);

  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);

	// Allocate memory
  cl::Buffer shifts_d;
  std::vector< dataType > dispersedData = std::vector< dataType >(observation.getNrChannels() * observation.getNrSamplesPerDispersedChannel());
  cl::Buffer dispersedData_d;
  std::vector< dataType > dedispersedData = std::vector< dataType >(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
  cl::Buffer dedispersedData_d;
  try {
    shifts_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(unsigned int), 0, 0);
    dispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, dispersedData.size() * sizeof(dataType), 0, 0);
    dedispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_WRITE, dedispersedData.size() * sizeof(dataType), 0, 0);
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error allocating memory: " << isa::utils::toString(err.err()) << "." << std::endl;
    return 1;
  }

	srand(time(0));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerDispersedChannel(); sample++ ) {
      dispersedData[(channel * observation.getNrSamplesPerDispersedChannel()) + sample] = static_cast< dataType >(rand() % 10);
		}
	}

  // Copy data structures to device
  try {
    clQueues->at(clDeviceID)[0].enqueueWriteBuffer(shifts_d, CL_FALSE, 0, shifts->size() * sizeof(unsigned int), reinterpret_cast< void * >(shifts->data()));
    clQueues->at(clDeviceID)[0].enqueueWriteBuffer(dispersedData_d, CL_FALSE, 0, dispersedData.size() * sizeof(dataType), reinterpret_cast< void * >(dispersedData.data()));
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error H2D transfer: " << isa::utils::toString(err.err()) << "." << std::endl;
    return 1;
  }

	// Find the parameters
	std::vector< unsigned int > samplesPerBlock;
	for ( unsigned int samples = minThreads; samples <= maxColumns; samples += minThreads ) {
		if ( (observation.getNrSamplesPerPaddedSecond() % samples) == 0 ) {
			samplesPerBlock.push_back(samples);
		}
	}
	std::vector< unsigned int > DMsPerBlock;
	for ( unsigned int DMs = 1; DMs <= maxRows; DMs++ ) {
		if ( (observation.getNrDMs() % DMs) == 0 ) {
			DMsPerBlock.push_back(DMs);
		}
	}

	std::cout << std::fixed << std::endl;
	std::cout << "# nrDMs nrChannels nrSamples local samplesPerBlock DMsPerBlock samplesPerThread DMsPerThread GFLOP/s err time err" << std::endl << std::endl;

	for ( std::vector< unsigned int >::iterator samples = samplesPerBlock.begin(); samples != samplesPerBlock.end(); ++samples ) {
		for ( std::vector< unsigned int >::iterator DMs = DMsPerBlock.begin(); DMs != DMsPerBlock.end(); ++DMs ) {
			if ( ((*samples) * (*DMs)) > maxThreadsPerBlock ) {
				break;
			}

			for ( unsigned int samplesPerThread = 1; samplesPerThread <= maxItemsPerThread; samplesPerThread++ ) {
				if ( (observation.getNrSamplesPerPaddedSecond() % ((*samples) * samplesPerThread)) != 0 ) {
					continue;
				}

				for ( unsigned int DMsPerThread = 1; DMsPerThread <= maxItemsPerThread; DMsPerThread++ ) {
					if ( (observation.getNrDMs() % ((*DMs) * DMsPerThread)) != 0 ) {
						continue;
					}
					if ( ( samplesPerThread * DMsPerThread ) > maxItemsPerThread ) {
						break;
					}

          // Generate kernel
          double flops = isa::utils::giga(static_cast< long long unsigned int >(observation.getNrDMs()) * observation.getNrChannels() * observation.getNrSamplesPerSecond());
          isa::utils::Timer timer("Kernel Timer");
          isa::utils::Stats< double > stats;
          cl::Event event;
          cl::Kernel * kernel;
          std::string * code = PulsarSearch::getDedispersionOpenCL(localMem, *samples, *DMs, samplesPerThread, DMsPerThread, typeName, observation, *shifts);

          try {
            kernel = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
          } catch ( isa::OpenCL::OpenCLError & err ) {
            std::cerr << err.what() << std::endl;
            continue;
          }

          cl::NDRange global(observation.getNrSamplesPerPaddedSecond() / samplesPerThread, observation.getNrDMs() / DMsPerThread);
          cl::NDRange local(*samples, *DMs);

          kernel->setArg(0, dispersedData_d);
          kernel->setArg(1, dedispersedData_d);
          kernel->setArg(2, shifts_d);

          // Warm-up run
          try {
            clQueues->at(clDeviceID)[0].enqueueNDRangeKernel(*kernel, cl::NullRange, global, local, 0, &event);
            event.wait();
          } catch ( cl::Error & err ) {
            std::cerr << "OpenCL error kernel execution: " << isa::utils::toString(err.err()) << "." << std::endl;
            continue;
          }
          // Tuning runs
          try {
            for ( unsigned int iteration = 0; iteration < nrIterations; iteration++ ) {
              timer.start();
              clQueues->at(clDeviceID)[0].enqueueNDRangeKernel(*kernel, cl::NullRange, global, local, 0, &event);
              event.wait();
              timer.stop();
              stats.addElement(flops / timer.getLastRunTime());
            }
          } catch ( cl::Error & err ) {
            std::cerr << "OpenCL error kernel execution: " << isa::utils::toString(err.err()) << "." << std::endl;
            continue;
          }

          std::cout << observation.getNrDMs() << " " << observation.getNrChannels() << " " << observation.getNrSamplesPerSecond() << " " << localMem << " " << *samples << " " << *DMs << " " << samplesPerThread << " " << DMsPerThread << " " << std::setprecision(3) << stats.getAverage() << " " << stats.getStdDev() << " " << std::setprecision(6) << timer.getAverageTime() << " " << timer.getStdDev() << std::endl;
				}
			}
		}
	}

	std::cout << std::endl;

	return 0;
}

