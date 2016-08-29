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

void initializeDeviceMemoryStepOne(cl::Context & clContext, cl::CommandQueue * clQueue, const std::vector< float > * shifts, cl::Buffer * shifts_d, const std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, const std::vector< inputDataType > & dispersedData, cl::Buffer * dispersedData_d, const std::vector< outputDataType > & subbandedData, cl::Buffer * subbandedData_d);
void initializeDeviceMemoryStepTwo(cl::Context & clContext, cl::CommandQueue * clQueue, const std::vector< float > * shifts, cl::Buffer * shifts_d, const std::vector< uint8_t > & beamDriver, cl::Buffer * beamDriver_d, const std::vector< outputDataType > & subbandedData, cl::Buffer * subbandedData_d, const std::vector< outputDataType > & dedispersedData, cl::Buffer * dedispersedData_d);

int main(int argc, char * argv[]) {
  bool reInit = false;
  bool stepOne = false;
  bool stepTwo = false;
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
    stepOne = args.getSwitch("-step_one");
    stepTwo = args.getSwitch("-step_two");
    if ( (stepOne && stepTwo) || (!stepOne && !stepTwo) ) {
      std::cerr << "Only one option between \"-step_one\" and \"-step_two\" can be set." << std::endl;
      return -1;
    }
    conf.setSplitSeconds(args.getSwitch("-split_seconds"));
    conf.setLocalMem(args.getSwitch("-local"));
		padding = args.getSwitchArgument< unsigned int >("-padding");
    inputBits = args.getSwitchArgument< unsigned int >("-input_bits");
    if ( stepOne ) {
      channelsFile = args.getSwitchArgument< std::string >("-zapped_channels");
    }
    vectorWidth = args.getSwitchArgument< unsigned int >("-vector");
		minThreads = args.getSwitchArgument< unsigned int >("-min_threads");
		maxThreads = args.getSwitchArgument< unsigned int >("-max_threads");
		maxRows = args.getSwitchArgument< unsigned int >("-max_rows");
		maxColumns = args.getSwitchArgument< unsigned int >("-max_columns");
		maxItems = args.getSwitchArgument< unsigned int >("-max_items");
    maxUnroll = args.getSwitchArgument< unsigned int >("-max_unroll");
    maxLoopBodySize = args.getSwitchArgument< unsigned int >("-max_loopsize");
    observation.setNrBeams(args.getSwitchArgument< unsigned int >("-beams"));
    if ( stepTwo ) {
      observation.setNrSyntheticBeams(args.getSwitchArgument< unsigned int >("-synthetic_beams"));
    }
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerBatch(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMSubbandingRange(args.getSwitchArgument< unsigned int >("-subbanding_dms"), args.getSwitchArgument< float >("-subbanding_dm_first"), args.getSwitchArgument< float >("-subbanding_dm_step"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch ( isa::utils::EmptyCommandLine & err ) {
		std::cerr << argv[0] << " -iterations ... -opencl_platform ... -opencl_device ... [-step_one | -step_two] [-split_seconds] [-local] -input_bits ... -padding ... -vector ... -min_threads ... -max_threads ... -max_items ... -max_unroll ... -max_loopsize ... -max_columns ... -max_rows ... -beams ... -min_freq ... -channel_bandwidth ... -samples ... -subbands ... -channels ... -subbanding_dms ... -subbanding_dm_first ... -subbanding_dm_step ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    std::cerr << " -step_one: -zapped_channels ..." << std::endl;
    std::cerr << " -step_two: -synthetic_beams ..." << std::endl;
		return 1;
	} catch ( std::exception & err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}

  // Allocate host memory
  std::vector< float > * shiftsStepOne = PulsarSearch::getShifts(observation, padding);
  std::vector< float > * shiftsStepTwo = PulsarSearch::getSubbandStepTwoShifts(observation, padding);
  std::vector< uint8_t > zappedChannels;
  std::vector< uint8_t > beamDriver;
  std::vector< inputDataType > dispersedData;
  std::vector< outputDataType > subbandedData;
  std::vector< outputDataType > dedispersedData;

  if ( stepOne ) {
    zappedChannels.resize(observation.getNrPaddedChannels(padding / sizeof(uint8_t)));
    AstroData::readZappedChannels(observation, channelsFile, zappedChannels);
  } else if ( stepTwo ) {
    beamDriver.resize(observation.getNrSyntheticBeams() * observation.getNrPaddedSubbands(padding / sizeof(uint8_t)));
  }
  if ( conf.getSplitSeconds() ) {
    // TODO: implement this mode
  } else {
    observation.setNrSamplesPerBatchSubbanding(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsStepTwo->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
    observation.setNrSamplesPerSubbandingDispersedChannel(observation.getNrSamplesPerBatchSubbanding() + static_cast< unsigned int >(shiftsStepOne->at(0) * (observation.getFirstDMSubbanding() + ((observation.getNrDMsSubbanding() - 1) * observation.getDMSubbandingStep()))));
    if ( inputBits >= 8 ) {
      dispersedData.resize(observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerPaddedSubbandingDispersedChannel(padding / sizeof(inputDataType)));
    } else {
      dispersedData.resize(observation.getNrBeams() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSubbandingDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType)));
    }
    subbandedData.resize(observation.getNrBeams() * observation.getNrSubbands() * observation.getNrDMsSubbanding() * observation.getNrSamplesPerPaddedBatchSubbanding(padding / sizeof(outputDataType)));
    if ( stepTwo ) {
      dedispersedData.resize(observation.getNrSyntheticBeams() * observation.getNrDMsSubbanding() * observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType)));
    }
  }
  // Generate data
  srand(time(0));
  if ( stepOne ) {
    for ( unsigned int beam = 0; beam < observation.getNrBeams(); beam++ ) {
      for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
        for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSubbandingDispersedChannel(); sample++ ) {
          if ( inputBits >= 8 ) {
            dispersedData[(beam * observation.getNrChannels() * observation.getNrSamplesPerPaddedSubbandingDispersedChannel(padding / sizeof(inputDataType))) + (channel * observation.getNrSamplesPerPaddedSubbandingDispersedChannel(padding / sizeof(inputDataType))) + sample] = rand() % observation.getNrChannels();
          } else {
            unsigned int byte = 0;
            uint8_t firstBit = 0;
            uint8_t value = rand() % inputBits;
            uint8_t buffer = 0;

            byte = sample / (8 / inputBits);
            firstBit = (sample % (8 / inputBits)) * inputBits;
            buffer = dispersedData[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSubbandingDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation.getNrSamplesPerSubbandingDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + byte];
            for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
              isa::utils::setBit(buffer, isa::utils::getBit(value, bit), firstBit + bit);
            }
            dispersedData[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSubbandingDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation.getNrSamplesPerSubbandingDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + byte] = buffer;
          }
        }
      }
    }
  }
  if ( stepTwo ) {
    for ( unsigned int beam = 0; beam < observation.getNrBeams(); beam++ ) {
      for ( unsigned int subband = 0; subband < observation.getNrSubbands(); subband++ ) {
        for ( unsigned int dm = 0; dm < observation.getNrDMsSubbanding(); dm++ ) {
          for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatchSubbanding(); sample++ ) {
            subbandedData[(beam * observation.getNrSubbands() * observation.getNrDMsSubbanding() * observation.getNrSamplesPerPaddedBatchSubbanding(padding / sizeof(outputDataType))) + (subband * observation.getNrDMsSubbanding() * observation.getNrSamplesPerPaddedBatchSubbanding(padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerPaddedBatchSubbanding(padding / sizeof(outputDataType))) + sample] = rand() % observation.getNrSubbands();
          }
        }
      }
    }
    for ( unsigned int sBeam = 0; sBeam < observation.getNrSyntheticBeams(); sBeam++ ) {
      for ( unsigned int subband = 0; subband < observation.getNrSubbands(); subband++ ) {
        beamDriver[(sBeam * observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + subband] = rand() % observation.getNrBeams();
      }
    }
  }

	// Initialize OpenCL
	cl::Context clContext;
	std::vector< cl::Platform > * clPlatforms = new std::vector< cl::Platform >();
	std::vector< cl::Device > * clDevices = new std::vector< cl::Device >();
	std::vector< std::vector< cl::CommandQueue > > * clQueues = new std::vector< std::vector < cl::CommandQueue > >();
  isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, &clContext, clDevices, clQueues);

	// Allocate device memory
  cl::Buffer shiftsStepOne_d;
  cl::Buffer shiftsStepTwo_d;
  cl::Buffer zappedChannels_d;
  cl::Buffer beamDriver_d;
  cl::Buffer dispersedData_d;
  cl::Buffer subbandedData_d;
  cl::Buffer dedispersedData_d;

  try {
    if ( conf.getSplitSeconds() ) {
      // TODO: implement this mode
    } else {
      if ( stepOne ) {
        initializeDeviceMemoryStepOne(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepOne, &shiftsStepOne_d, zappedChannels, &zappedChannels_d, dispersedData, &dispersedData_d, subbandedData, &subbandedData_d);
      } else {
        initializeDeviceMemoryStepTwo(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepTwo, &shiftsStepTwo_d, beamDriver, &beamDriver_d, subbandedData, &subbandedData_d, dedispersedData, &dedispersedData_d);
      }
    }
  } catch ( cl::Error & err ) {
    std::cerr << err.what() << std::endl;
    return -1;
  }

	std::cout << std::fixed << std::endl;
	std::cout << "# nrBeams nrSBeams nrSubbandingDMs nrDMs nrSubbands nrChannels nrZappedChannels nrSamples *configuration* GFLOP/s time stdDeviation COV" << std::endl << std::endl;

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
            double gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrDMs()) * observation.getNrSyntheticBeams() * (observation.getNrChannels() - observation.getNrZappedChannels()) * observation.getNrSamplesPerBatch());
            isa::utils::Timer timer;
            cl::Kernel * kernel;
            std::string * code = PulsarSearch::getDedispersionOpenCL< inputDataType, outputDataType >(conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shifts, zappedChannels);

            if ( reInit ) {
              delete clQueues;
              clQueues = new std::vector< std::vector < cl::CommandQueue > >();
              isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, &clContext, clDevices, clQueues);
              try {
                if ( conf.getSplitSeconds() ) {
                  // TODO: implement this mode
                } else {
                  if ( stepOne ) {
                  initializeDeviceMemoryStepOne(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepOne, &shiftsStepOne_d, zappedChannels, &zappedChannels_d, dispersedData, &dispersedData_d, subbandedData, &subbandedData_d);
                  } else {
                    initializeDeviceMemoryStepTwo(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepTwo, &shiftsStepTwo_d, beamDriver, &beamDriver_d, subbandedData, &subbandedData_d, dedispersedData, &dedispersedData_d);
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

            cl::NDRange global;
            cl::NDRange local;
            if ( stepOne ) {
              global(observation.getNrSamplesPerBatch() / conf.getNrItemsD0(), observation.getNrDMs() / conf.getNrItemsD1(), observation.getNrBeams() * observation.getNrSubbands());
              local(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
            } else {
              global(observation.getNrSamplesPerBatch() / conf.getNrItemsD0(), observation.getNrDMs() / conf.getNrItemsD1(), observation.getNrBeams() * observation.getNrDMsSubbanding());
              local(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
            }

            if ( conf.getSplitSeconds() ) {
              // TODO: implement this mode
            } else {
              if ( stepOne ) {
                kernel->setArg(0, dispersedData_d);
                kernel->setArg(1, subbandedData_d);
                kernel->setArg(2, shiftsStepOne_d);
                kernel->setArg(3, zappedChannels_d);
              } else {
                kernel->setArg(0, subbandedData_d);
                kernel->setArg(1, dedispersedData_d);
                kernel->setArg(2, shiftsStepTwo_d);
                kernel->setArg(3, beamDriver_d);
              }
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

            std::cout << observation.getNrBeams() << " " << observation.getNrSyntheticBeams() << " ";
            std::cout << observation.getNrDMsSubbanding() << " " << observation.getNrDMs() << " ";
            std::Cout << observation.getNrSubbands() << " " << observation.getNrChannels() << " " << observation.getNrZappedChannels() << " " << observation.getNrSamplesPerBatch() << " ";
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

void initializeDeviceMemoryStepOne(cl::Context & clContext, cl::CommandQueue * clQueue, const std::vector< float > * shifts, cl::Buffer * shifts_d, const std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, const std::vector< inputDataType > & dispersedData, cl::Buffer * dispersedData_d, const std::vector< outputDataType > & subbandedData, cl::Buffer * subbandedData_d) {
  try {
    *shifts_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(float), 0, 0);
    *zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(uint8_t), 0, 0);
    *dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData.size() * sizeof(inputDataType), 0, 0);
    *subbandedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, subbandedData.size() * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shifts_d, CL_FALSE, 0, shifts->size() * sizeof(float), reinterpret_cast< void * >(shifts->data()));
    clQueue->enqueueWriteBuffer(*zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(uint8_t), reinterpret_cast< void * >(zappedChannels.data()));
    clQueue->enqueueWriteBuffer(*dispersedData_d, CL_FALSE, 0, dispersedData.size() * sizeof(inputDataType), reinterpret_cast< void * >(dispersedData.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << isa::utils::toString(err.err()) << "." << std::endl;
    throw;
  }
}

void initializeDeviceMemoryStepTwo(cl::Context & clContext, cl::CommandQueue * clQueue, const std::vector< float > * shifts, cl::Buffer * shifts_d, const std::vector< uint8_t > & beamDriver, cl::Buffer * beamDriver_d, const std::vector< outputDataType > & subbandedData, cl::Buffer * subbandedData_d, const std::vector< outputDataType > & dedispersedData, cl::Buffer * dedispersedData_d) {
  try {
    *shifts_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(float), 0, 0);
    *beamDriver_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamDriver.size() * sizeof(uint8_t), 0, 0);
    *subbandedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, subbandedData.size() * sizeof(outputDataType), 0, 0);
    *dedispersedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, dedispersedData.size() * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shifts_d, CL_FALSE, 0, shifts->size() * sizeof(float), reinterpret_cast< void * >(shifts->data()));
    clQueue->enqueueWriteBuffer(*beamDriver_d, CL_FALSE, 0, beamDriver.size() * sizeof(uint8_t), reinterpret_cast< void * >(beamDriver.data()));
    clQueue->enqueueWriteBuffer(*subbandedData_d, CL_FALSE, 0, subbandedData.size() * sizeof(outputDataType), reinterpret_cast< void * >(subbandedData.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << isa::utils::toString(err.err()) << "." << std::endl;
    throw;
  }
}

