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

void initializeDeviceMemory(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shifts, cl::Buffer * shifts_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, cl::Buffer * dispersedData_d, const unsigned int dispersedData_size, cl::Buffer * dedispersedData_d, const unsigned int dedispersedData_size);

int main(int argc, char * argv[]) {
  bool reInit = false;
  unsigned int padding = 0;
  uint8_t inputBits = 0;
	unsigned int nrIterations = 0;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	unsigned int minThreads = 0;
  unsigned int maxThreads = 0;
	unsigned int maxRows = 0;
	unsigned int maxColumns = 0;
  unsigned int vectorWidth = 0;
  unsigned int threadIncrement = 0;
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
    threadIncrement = args.getSwitchArgument< unsigned int >("-thread_increment");
		maxItems = args.getSwitchArgument< unsigned int >("-max_items");
    maxUnroll = args.getSwitchArgument< unsigned int >("-max_unroll");
    maxLoopBodySize = args.getSwitchArgument< unsigned int >("-max_loopsize");
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch ( isa::utils::EmptyCommandLine & err ) {
		std::cerr << argv[0] << " -iterations ... -opencl_platform ... -opencl_device ... [-split_seconds] [-local] -input_bits ... -padding ... -zapped_channels ... -thread_unit ... -min_threads ... -max_threads ... -max_items ... -max_unroll ... -max_loopsize ... -max_columns ... -max_rows ... -thread_increment ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	} catch ( std::exception & err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}

  // Allocate host memory
  std::vector< float > * shifts = PulsarSearch::getShifts(observation, padding);
  std::vector< uint8_t > zappedChannels(observation.getNrPaddedChannels(padding / sizeof(uint8_t)));

  AstroData::readZappedChannels(observation, channelsFile, zappedChannels);
  if ( conf.getSplitSeconds() ) {
    if ( (observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) % observation.getNrSamplesPerSecond() == 0 ) {
      observation.setNrDelaySeconds((observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerSecond());
    } else {
      observation.setNrDelaySeconds(((observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerSecond()) + 1);
    }
  } else {
    observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
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
  cl::Buffer dispersedData_d;
  cl::Buffer dedispersedData_d;

  try {
    if ( inputBits >= 8 ) {
      if ( conf.getSplitSeconds() ) {
        initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
      } else {
        initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
      }
    } else {
      if ( conf.getSplitSeconds() ) {
        initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
      } else {
        initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
      }
    }
  } catch ( cl::Error & err ) {
    std::cerr << err.what() << std::endl;
    return -1;
  }

	// Find the parameters
	std::vector< unsigned int > samplesPerBlock;
	for ( unsigned int samples = minThreads; samples <= maxColumns; samples += threadIncrement ) {
		if ( (observation.getNrSamplesPerSecond() % samples) == 0 ) {
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
	std::cout << "# nrDMs nrChannels nrZappedChannels nrSamples splitSeconds local unroll samplesPerBlock DMsPerBlock samplesPerThread DMsPerThread GFLOP/s time stdDeviation COV" << std::endl << std::endl;

	for ( std::vector< unsigned int >::iterator samples = samplesPerBlock.begin(); samples != samplesPerBlock.end(); ++samples ) {
    conf.setNrSamplesPerBlock(*samples);

		for ( std::vector< unsigned int >::iterator DMs = DMsPerBlock.begin(); DMs != DMsPerBlock.end(); ++DMs ) {
      conf.setNrDMsPerBlock(*DMs);
			if ( conf.getNrSamplesPerBlock() * conf.getNrDMsPerBlock() > maxThreads ) {
				break;
			} else if ( (conf.getNrSamplesPerBlock() * conf.getNrDMsPerBlock()) % vectorWidth != 0 ) {
        continue;
      }

			for ( unsigned int samplesPerThread = 1; samplesPerThread <= maxItems; samplesPerThread++ ) {
        conf.setNrSamplesPerThread(samplesPerThread);
				if ( (observation.getNrSamplesPerSecond() % (conf.getNrSamplesPerBlock() * conf.getNrSamplesPerThread())) != 0 ) {
					continue;
				}

				for ( unsigned int DMsPerThread = 1; DMsPerThread <= maxItems; DMsPerThread++ ) {
          conf.setNrDMsPerThread(DMsPerThread);
					if ( (observation.getNrDMs() % (conf.getNrDMsPerBlock() * conf.getNrDMsPerThread())) != 0 ) {
						continue;
					} else if ( (conf.getNrSamplesPerThread() * conf.getNrDMsPerThread()) + conf.getNrDMsPerThread() > maxItems ) {
						break;
					}

          for ( unsigned int unroll = 1; unroll <= maxUnroll; unroll++ ) {
            conf.setUnroll(unroll);
            if ( (observation.getNrChannels() - 1) % conf.getUnroll() != 0 ) {
              continue;
            } else if ( (conf.getNrSamplesPerThread() * conf.getNrDMsPerThread() * conf.getUnroll()) > maxLoopBodySize ) {
              break;
            }
            // Generate kernel
            double gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrDMs()) * (observation.getNrChannels() - observation.getNrZappedChannels()) * observation.getNrSamplesPerSecond());
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
                    initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
                  } else {
                    initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
                  }
                } else {
                  if ( conf.getSplitSeconds() ) {
                    initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrDelaySeconds() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
                  } else {
                    initializeDeviceMemory(clContext, &(clQueues->at(clDeviceID)[0]), shifts, &shifts_d, zappedChannels, &zappedChannels_d, &dispersedData_d, observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType)), &dedispersedData_d, observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(outputDataType)));
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

            cl::NDRange global(observation.getNrSamplesPerSecond() / conf.getNrSamplesPerThread(), observation.getNrDMs() / conf.getNrDMsPerThread());
            cl::NDRange local(conf.getNrSamplesPerBlock(), conf.getNrDMsPerBlock());

            if ( conf.getSplitSeconds() ) {
              kernel->setArg(0, 0);
              kernel->setArg(1, dispersedData_d);
              kernel->setArg(2, dedispersedData_d);
              kernel->setArg(3, shifts_d);
              kernel->setArg(4, zappedChannels_d);
            } else {
              kernel->setArg(0, dispersedData_d);
              kernel->setArg(1, dedispersedData_d);
              kernel->setArg(2, shifts_d);
              kernel->setArg(3, zappedChannels_d);
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

            std::cout << observation.getNrDMs() << " " << observation.getNrChannels() << " " << observation.getNrZappedChannels() << " " << observation.getNrSamplesPerSecond() << " ";
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

void initializeDeviceMemory(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shifts, cl::Buffer * shifts_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, cl::Buffer * dispersedData_d, const unsigned int dispersedData_size, cl::Buffer * dedispersedData_d, const unsigned int dedispersedData_size) {
  try {
    *shifts_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(float), 0, 0);
    *zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(uint8_t), 0, 0);
    *dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
    *dedispersedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, dedispersedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shifts_d, CL_FALSE, 0, shifts->size() * sizeof(float), reinterpret_cast< void * >(shifts->data()));
    clQueue->enqueueWriteBuffer(*zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(uint8_t), reinterpret_cast< void * >(zappedChannels.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << isa::utils::toString(err.err()) << "." << std::endl;
    throw;
  }
}

