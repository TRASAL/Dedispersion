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
#include <ctime>

#include <configuration.hpp>

#include <utils.hpp>
#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <ReadData.hpp>
#include <InitializeOpenCL.hpp>
#include <Kernel.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
#include <Timer.hpp>
#include <Stats.hpp>

void initializeDeviceMemory(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shifts, cl::Buffer * shifts_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, std::vector< uint8_t > & beamDriver, cl::Buffer * beamDriver_d, cl::Buffer * dispersedData_d, const unsigned int dispersedData_size, cl::Buffer * dedispersedData_d, const unsigned int dedispersedData_size);

int main(int argc, char * argv[]) {
  bool reInit = false;
  unsigned int padding = 0;
  uint8_t inputBits = 0;
  unsigned int nrSBeams = 0;
	unsigned int nrIterations = 0;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	unsigned int minThreads = 0;
  unsigned int maxThreads = 0;
	unsigned int maxRows = 0;
	unsigned int maxColumns = 0;
  unsigned int vectorWidth = 0;
  unsigned int maxItems = 0;
  unsigned int maxUnroll = 0;
  unsigned int maxLoopBodySize = 0;
  std::string channelsFile;
  AstroData::Observation observation;
  PulsarSearch::DedispersionConf conf;
  cl::Event event;

	try {
    isa::utils::ArgumentList args(argc, argv);

		nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
    conf.setSplitSeconds(args.getSwitch("-split_seconds"));
    conf.setLocalMem(args.getSwitch("-local"));
		padding = args.getSwitchArgument< unsigned int >("-padding");
    inputBits = args.getSwitchArgument< unsigned int >("-input_bits");
    channelsFile = args.getSwitchArgument< std::string >("-zapped_channels");
    vectorWidth = args.getSwitchArgument< unsigned int >("-vector");
		minThreads = args.getSwitchArgument< unsigned int >("-min_threads");
		maxThreads = args.getSwitchArgument< unsigned int >("-max_threads");
		maxRows = args.getSwitchArgument< unsigned int >("-max_rows");
		maxColumns = args.getSwitchArgument< unsigned int >("-max_columns");
		maxItems = args.getSwitchArgument< unsigned int >("-max_items");
    maxUnroll = args.getSwitchArgument< unsigned int >("-max_unroll");
    maxLoopBodySize = args.getSwitchArgument< unsigned int >("-max_loopsize");
    observation.setNrBeams(args.getSwitchArgument< unsigned int >("-beams"));
    nrSBeams = args.getSwitchArgument< unsigned int >("-synthetic_beams");
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerBatch(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch ( isa::utils::EmptyCommandLine & err ) {
		std::cerr << argv[0] << " -iterations ... -opencl_platform ... -opencl_device ... [-split_seconds] [-local] -input_bits ... -padding ... -zapped_channels ... -vector ... -min_threads ... -max_threads ... -max_items ... -max_unroll ... -max_loopsize ... -max_columns ... -max_rows ... -synthetic_beams ... -beams ... -min_freq ... -channel_bandwidth ... -samples ... -subbands ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	} catch ( std::exception & err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}

  // Allocate host memory
  std::vector< float > * shifts = PulsarSearch::getShifts(observation, padding);
  std::vector< uint8_t > zappedChannels(observation.getNrPaddedChannels(padding / sizeof(uint8_t)));
  std::vector< uint8_t > beamDriver(nrSBeams * observation.getNrPaddedChannels(padding / sizeof(uint8_t)));

  AstroData::readZappedChannels(observation, channelsFile, zappedChannels);
  if ( conf.getSplitSeconds() ) {
    if ( (observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) % observation.getNrSamplesPerBatch() == 0 ) {
      observation.setNrDelaySeconds((observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerBatch());
    } else {
      observation.setNrDelaySeconds(((observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerBatch()) + 1);
    }
  } else {
    observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
  }
  srand(time(0));
  for ( unsigned int sBeam = 0; sBeam < nrSBeams; sBeam++ ) {
    for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
      beamDriver[(sBeam * observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + channel] = rand() % observation.getNrBeams();
    }
  }

	// Initialize OpenCL
	cl::Context clContext;
	std::vector< cl::Platform > * clPlatforms = new std::vector< cl::Platform >();
	std::vector< cl::Device > * clDevices = new std::vector< cl::Device >();
	std::vector< std::vector< cl::CommandQueue > > * clQueues = new std::vector< std::vector < cl::CommandQueue > >();
  isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, &clContext, clDevices, clQueues);

	// Allocate device memory
  cl::Buffer shifts_d;
  cl::Buffer zappedChannels_d;
  cl::Buffer beamDriver_d;
  cl::Buffer dispersedData_d;
  cl::Buffer dedispersedData_d;

  try {
    if ( inputBits >= 8 ) {
      if ( conf.getSplitSeconds() ) {
        //initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
      } else {
        initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, beamDriver, &beamDriver_d, &dispersedData_d, observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType)), &dedispersedData_d, nrSBeams * observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
      }
    } else {
      if ( conf.getSplitSeconds() ) {
        //initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerBatch() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
      } else {
        //initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
      }
    }
  } catch ( cl::Error & err ) {
    std::cerr << err.what() << std::endl;
    return -1;
  }

	std::cout << std::fixed << std::endl;
	std::cout << "# nrBeams nrSBeams nrDMs nrChannels nrZappedChannels nrSamples splitSeconds local unroll threadsD0 threadsD1 itemsD0 itemsD1 GFLOP/s time stdDeviation COV" << std::endl << std::endl;

	for ( unsigned int threads = minThreads; threads <= maxColumns; threads++) {
    conf.setNrThreadsD0(threads);

		for ( unsigned int threads = 1; threads <= maxRows; threads++ ) {
      conf.setNrThreadsD1(threads);
			if ( conf.getNrThreadsD0() * conf.getNrThreadsD1() > maxThreads ) {
				break;
			} else if ( (conf.getNrThreadsD0() * conf.getNrThreadsD1()) % vectorWidth != 0 ) {
        continue;
      }

			for ( unsigned int items = 1; items <= maxItems; items++ ) {
        conf.setNrItemsD0(items);
				if ( (observation.getNrSamplesPerBatch() % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
					continue;
				}

				for ( unsigned int items = 1; items <= maxItems; items++ ) {
          conf.setNrItemsD1(items);
					if ( (observation.getNrDMs() % (conf.getNrThreadsD1() * conf.getNrItemsD1())) != 0 ) {
						continue;
					} else if ( (conf.getNrItemsD0() * conf.getNrItemsD1()) + conf.getNrItemsD1() > maxItems ) {
						break;
					}

          for ( unsigned int unroll = 1; unroll <= maxUnroll; unroll++ ) {
            conf.setUnroll(unroll);
            if ( (observation.getNrChannels() - 1) % conf.getUnroll() != 0 ) {
              continue;
            } else if ( (conf.getNrItemsD0() * conf.getNrItemsD1() * conf.getUnroll()) > maxLoopBodySize ) {
              break;
            }
            // Generate kernel
            double gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrDMs()) * nrSBeams * (observation.getNrChannels() - observation.getNrZappedChannels()) * observation.getNrSamplesPerBatch());
            isa::utils::Timer timer;
            cl::Kernel * kernel;
            std::string * code = PulsarSearch::getDedispersionOpenCL< inputDataType, outputDataType >(conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shifts, zappedChannels);

            if ( reInit ) {
              delete clQueues;
              clQueues = new std::vector< std::vector < cl::CommandQueue > >();
              isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, &clContext, clDevices, clQueues);
              try {
                if ( inputBits >= 8 ) {
                  if ( conf.getSplitSeconds() ) {
                    //initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
                  } else {
                    initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, beamDriver, &beamDriver_d, &dispersedData_d, observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType)), &dedispersedData_d, nrSBeams * observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
                  }
                } else {
                  if ( conf.getSplitSeconds() ) {
                    //initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerBatch() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
                  } else {
                    //initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
                  }
                }
              } catch ( cl::Error & err ) {
                std::cerr << "Error in memory allocation: ";
                std::cerr << isa::utils::toString(err.err()) << "." << std::endl;
                return -1;
              }
              reInit = false;
            }
            try {
              kernel = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", clContext, clDevices->at(clDeviceID));
            } catch ( isa::OpenCL::OpenCLError & err ) {
              std::cerr << err.what() << std::endl;
              delete code;
              break;
            }
            delete code;

            cl::NDRange global(observation.getNrSamplesPerBatch() / conf.getNrItemsD0(), observation.getNrDMs() / conf.getNrItemsD1(), nrSBeams);
            cl::NDRange local(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);

            if ( conf.getSplitSeconds() ) {
              kernel->setArg(0, 0);
              kernel->setArg(1, dispersedData_d);
              kernel->setArg(2, dedispersedData_d);
              kernel->setArg(3, zappedChannels_d);
              kernel->setArg(4, shifts_d);
            } else {
              kernel->setArg(0, dispersedData_d);
              kernel->setArg(1, dedispersedData_d);
              kernel->setArg(2, beamDriver_d);
              kernel->setArg(3, zappedChannels_d);
              kernel->setArg(4, shifts_d);
            }

            try {
              // Warm-up run
              clQueues->at(clDeviceID)[0].finish();
              clQueues->at(clDeviceID)[0].enqueueNDRangeKernel(*kernel, cl::NullRange, global, local, 0, &event);
              event.wait();
              // Tuning runs
              for ( unsigned int iteration = 0; iteration < nrIterations; iteration++ ) {
                timer.start();
                clQueues->at(clDeviceID)[0].enqueueNDRangeKernel(*kernel, cl::NullRange, global, local, 0, &event);
                event.wait();
                timer.stop();
              }
            } catch ( cl::Error & err ) {
              std::cerr << "OpenCL error kernel execution (";
              std::cerr << conf.print() << "): ";
              std::cerr << isa::utils::toString(err.err()) << "." << std::endl;
              delete kernel;
              if ( err.err() == -4 || err.err() == -61 ) {
                return -1;
              }
              reInit = true;
              break;
            }
            delete kernel;

            std::cout << observation.getNrBeams() << " " << nrSBeams << " ";
            std::cout << observation.getNrDMs() << " " << observation.getNrChannels() << " " << observation.getNrZappedChannels() << " " << observation.getNrSamplesPerBatch() << " ";
            std::cout << conf.print() << " ";
            std::cout << std::setprecision(3);
            std::cout << gflops / timer.getAverageTime() << " ";
            std::cout << std::setprecision(6);
            std::cout << timer.getAverageTime() << " " << timer.getStandardDeviation() << " ";
            std::cout << timer.getCoefficientOfVariation() <<  std::endl;
          }
				}
			}
		}
	}

	std::cout << std::endl;

	return 0;
}

void initializeDeviceMemory(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shifts, cl::Buffer * shifts_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, std::vector< uint8_t > & beamDriver, cl::Buffer * beamDriver_d, cl::Buffer * dispersedData_d, const unsigned int dispersedData_size, cl::Buffer * dedispersedData_d, const unsigned int dedispersedData_size) {
  try {
    *shifts_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(float), 0, 0);
    *zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(uint8_t), 0, 0);
    *beamDriver_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamDriver.size() * sizeof(uint8_t), 0, 0);
    *dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
    *dedispersedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, dedispersedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shifts_d, CL_FALSE, 0, shifts->size() * sizeof(float), reinterpret_cast< void * >(shifts->data()));
    clQueue->enqueueWriteBuffer(*zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(uint8_t), reinterpret_cast< void * >(zappedChannels.data()));
    clQueue->enqueueWriteBuffer(*beamDriver_d, CL_FALSE, 0, beamDriver.size() * sizeof(uint8_t), reinterpret_cast< void * >(beamDriver.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << isa::utils::toString(err.err()) << "." << std::endl;
    throw;
  }
}

