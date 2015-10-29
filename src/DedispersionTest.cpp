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

#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <InitializeOpenCL.hpp>
#include <Kernel.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>

// Define the data types
typedef float inputDataType;
std::string inputTypeName("float");
typedef float intermediateDataType;
std::string intermediateTypeName("float");
typedef float outputDataType;
std::string outputTypeName("float");


int main(int argc, char *argv[]) {
  uint8_t inputBits = 0;
  bool printCode = false;
  bool printResults = false;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
  long long unsigned int wrongSamples = 0;
  PulsarSearch::DedispersionConf conf;
  AstroData::Observation observation;

  try {
    isa::utils::ArgumentList args(argc, argv);
    printCode = args.getSwitch("-print_code");
    printResults = args.getSwitch("-print_results");
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
    inputBits = args.getSwitchArgument< unsigned int >("-input_bits");
    observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
    conf.setLocalMem(args.getSwitch("-local"));
    conf.setSplitSeconds(args.getSwitch("-split_seconds"));
    conf.setNrSamplesPerBlock(args.getSwitchArgument< unsigned int >("-sb"));
		conf.setNrDMsPerBlock(args.getSwitchArgument< unsigned int >("-db"));
		conf.setNrSamplesPerThread(args.getSwitchArgument< unsigned int >("-st"));
		conf.setNrDMsPerThread(args.getSwitchArgument< unsigned int >("-dt"));
    conf.setUnroll(args.getSwitchArgument< unsigned int >("-unroll"));
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch  ( isa::utils::SwitchNotFound & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception & err ) {
    std::cerr << "Usage: " << argv[0] << " [-print_code] [-print_results] -opencl_platform ... -opencl_device ... -input_bits ... -padding ... [-split_seconds] [-local] -sb ... -db ... -st ... -dt ... -unroll ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	}

	// Initialize OpenCL
	cl::Context * clContext = new cl::Context();
	std::vector< cl::Platform > * clPlatforms = new std::vector< cl::Platform >();
	std::vector< cl::Device > * clDevices = new std::vector< cl::Device >();
	std::vector< std::vector< cl::CommandQueue > > * clQueues = new std::vector< std::vector < cl::CommandQueue > >();

  isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, clContext, clDevices, clQueues);
  std::vector< float > * shifts = PulsarSearch::getShifts(observation);

  if ( conf.getSplitSeconds() ) {
    if ( (observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) % observation.getNrSamplesPerSecond() == 0 ) {
      observation.setNrDelaySeconds((observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerSecond());
    } else {
      observation.setNrDelaySeconds(((observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerSecond()) + 1);
    }
  }
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));

	// Allocate memory
  cl::Buffer shifts_d;
  std::vector< inputDataType > dispersedData;
  std::vector< inputDataType > dispersedData_control;
  if ( inputBits >= 8 ) {
    if ( conf.getSplitSeconds() ) {
      dispersedData = std::vector< inputDataType >(observation.getNrDelaySeconds() * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond());
      dispersedData_control = std::vector< inputDataType >(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel());
    } else {
      dispersedData = std::vector< inputDataType >(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel());
    }
  } else {
    if ( conf.getSplitSeconds() ) {
      dispersedData = std::vector< inputDataType >(observation.getNrDelaySeconds() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding()));
      dispersedData_control = std::vector< inputDataType >(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding()));
    } else {
      dispersedData = std::vector< inputDataType >(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding()));
    }
  }
  cl::Buffer dispersedData_d;
  std::vector< outputDataType > dedispersedData = std::vector< outputDataType >(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
  cl::Buffer dedispersedData_d;
  std::vector< outputDataType > dedispersedData_control = std::vector< outputDataType >(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
  try {
    shifts_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(float), 0, 0);
    dispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, dispersedData.size() * sizeof(inputDataType), 0, 0);
    dedispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_WRITE, dedispersedData.size() * sizeof(outputDataType), 0, 0);
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error allocating memory: " << isa::utils::toString< cl_int >(err.err()) << "." << std::endl;
    return 1;
  }

	srand(time(0));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerDispersedChannel(); sample++ ) {
      if ( inputBits >= 8 ) {
        if ( conf.getSplitSeconds() ) {
          dispersedData_control[(channel * observation.getNrSamplesPerPaddedDispersedChannel()) + sample] = static_cast< inputDataType >(rand() % 10);
          dispersedData[((sample / observation.getNrSamplesPerSecond()) * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond()) + (channel * observation.getNrSamplesPerPaddedSecond()) + (sample % observation.getNrSamplesPerSecond())] = dispersedData_control[(channel * observation.getNrSamplesPerPaddedDispersedChannel()) + sample];
        } else {
          dispersedData[(channel * observation.getNrSamplesPerPaddedDispersedChannel()) + sample] = static_cast< inputDataType >(rand() % 10);
        }
      } else {
        unsigned int byte = 0;
        unsigned int byte_control = 0;
        uint8_t firstBit = 0;
        uint8_t firstBit_control = 0;
        uint8_t value = rand() % inputBits;
        inputDataType buffer = 0;
        inputDataType buffer_control = 0;

        if ( conf.getSplitSeconds() ) {
          byte = (sample % observation.getNrSamplesPerSecond()) / (8 / inputBits);
          firstBit = ((sample % observation.getNrSamplesPerDispersedChannel()) % (8 / inputBits)) * inputBits;
          buffer = dispersedData[((sample / observation.getNrSamplesPerSecond()) * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + (channel * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + byte];
          byte_control = sample / (8 / inputBits);
          firstBit_control = (sample % (8 / inputBits)) * inputBits;
          buffer_control = dispersedData_control[(channel * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + byte_control];
        } else {
          byte = sample / (8 / inputBits);
          firstBit = (sample % (8 / inputBits)) * inputBits;
          buffer = dispersedData[(channel * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + byte];
        }

        for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
          if ( conf.getSplitSeconds() ) {
            isa::utils::setBit(buffer, isa::utils::getBit(value, bit), firstBit + bit);
            isa::utils::setBit(buffer_control, isa::utils::getBit(value, bit), firstBit_control + bit);
          } else {
            isa::utils::setBit(buffer, isa::utils::getBit(value, bit), firstBit + bit);
          }
        }

        if ( conf.getSplitSeconds() ) {
          dispersedData[((sample / observation.getNrSamplesPerSecond()) * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + (channel * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + byte] = buffer;
          dispersedData_control[(channel * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + byte_control] = buffer_control;
        } else {
          dispersedData[(channel * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + byte] = buffer;
        }
      }
		}
	}

  // Copy data structures to device
  try {
    clQueues->at(clDeviceID)[0].enqueueWriteBuffer(shifts_d, CL_FALSE, 0, shifts->size() * sizeof(float), reinterpret_cast< void * >(shifts->data()), 0, 0);
    clQueues->at(clDeviceID)[0].enqueueWriteBuffer(dispersedData_d, CL_FALSE, 0, dispersedData.size() * sizeof(inputDataType), reinterpret_cast< void * >(dispersedData.data()), 0, 0);
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error H2D transfer: " << isa::utils::toString< cl_int >(err.err()) << "." << std::endl;
    return 1;
  }

	// Generate kernel
  std::string * code = PulsarSearch::getDedispersionOpenCL(conf, inputBits, inputTypeName, intermediateTypeName, outputTypeName, observation, *shifts);
  cl::Kernel * kernel;
  if ( printCode ) {
    std::cout << *code << std::endl;
  }
	try {
    kernel = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
	} catch ( isa::OpenCL::OpenCLError & err ) {
    std::cerr << err.what() << std::endl;
		return 1;
	}

  // Run OpenCL kernel and CPU control
  try {
    cl::NDRange global(observation.getNrSamplesPerPaddedSecond() / conf.getNrSamplesPerThread(), observation.getNrDMs() / conf.getNrDMsPerThread());
    cl::NDRange local(conf.getNrSamplesPerBlock(), conf.getNrDMsPerBlock());

    if ( conf.getSplitSeconds() ) {
      kernel->setArg(0, 0);
      kernel->setArg(1, dispersedData_d);
      kernel->setArg(2, dedispersedData_d);
      kernel->setArg(3, shifts_d);
    } else {
      kernel->setArg(0, dispersedData_d);
      kernel->setArg(1, dedispersedData_d);
      kernel->setArg(2, shifts_d);
    }
    clQueues->at(clDeviceID)[0].enqueueNDRangeKernel(*kernel, cl::NullRange, global, local);
    if ( conf.getSplitSeconds() ) {
      PulsarSearch::dedispersion< inputDataType, intermediateDataType, outputDataType >(observation, dispersedData_control, dedispersedData_control, *shifts, inputBits);
    } else {
      PulsarSearch::dedispersion< inputDataType, intermediateDataType, outputDataType >(observation, dispersedData, dedispersedData_control, *shifts, inputBits);
    }
    clQueues->at(clDeviceID)[0].enqueueReadBuffer(dedispersedData_d, CL_TRUE, 0, dedispersedData.size() * sizeof(outputDataType), reinterpret_cast< void * >(dedispersedData.data()));
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error kernel execution: " << isa::utils::toString< cl_int >(err.err()) << "." << std::endl;
    return 1;
  }

	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
      if ( !isa::utils::same(dedispersedData_control[(dm * observation.getNrSamplesPerPaddedSecond()) + sample], dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]) ) {
        wrongSamples++;
			}
		}
	}
  if ( printResults ) {
    for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
      std::cout << dm << ": ";
      for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
        std::cout << dedispersedData_control[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " ";
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
    for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
      std::cout << dm << ": ";
      for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
        std::cout << dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " ";
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }

  if ( wrongSamples > 0 ) {
    std::cout << "Wrong samples: " << wrongSamples << " (" << (wrongSamples * 100.0) / (static_cast< long long unsigned int >(observation.getNrDMs()) * observation.getNrSamplesPerSecond()) << "%)." << std::endl;
  } else {
    std::cout << "TEST PASSED." << std::endl;
  }

	return 0;
}

