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
#include <Exceptions.hpp>
#include <Observation.hpp>
#include <InitializeOpenCL.hpp>
#include <Kernel.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>

typedef float dataType;
std::string typeName("float");


int main(int argc, char *argv[]) {
  bool localMem = false;
  bool print = false;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	unsigned int nrSamplesPerBlock = 0;
	unsigned int nrDMsPerBlock = 0;
	unsigned int nrSamplesPerThread = 0;
	unsigned int nrDMsPerThread = 0;
	long long unsigned int wrongSamples = 0;
	AstroData::Observation< dataType > observation("DedispersionTest", typeName);

	try {
    isa::utils::ArgumentList args(argc, argv);
    print = args.getSwitch("-print");
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
    observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
    localMem = args.getSwitch("-local");
		nrSamplesPerBlock = args.getSwitchArgument< unsigned int >("-sb");
		nrDMsPerBlock = args.getSwitchArgument< unsigned int >("-db");
		nrSamplesPerThread = args.getSwitchArgument< unsigned int >("-st");
		nrDMsPerThread = args.getSwitchArgument< unsigned int >("-dt");
		observation.setMinFreq(args.getSwitchArgument< float >("-min_freq"));
		observation.setChannelBandwidth(args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
		observation.setNrChannels(args.getSwitchArgument< unsigned int >("-channels"));
		observation.setNrDMs(args.getSwitchArgument< unsigned int >("-dms"));
		observation.setFirstDM(args.getSwitchArgument< float >("-dm_first"));
		observation.setDMStep(args.getSwitchArgument< float >("-dm_step"));
		observation.setMaxFreq(observation.getMinFreq() + (observation.getChannelBandwidth() * (observation.getNrChannels() - 1)));
	} catch  ( isa::utils::SwitchNotFound &err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception &err ) {
    std::cerr << "Usage: " << argv[0] << " [-print] -opencl_platform ... -opencl_device ... -padding ... [-local] -sb ... -db ... -st ... -dt ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
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
  std::vector< dataType > controlData = std::vector< dataType >(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
  try {
    shifts_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, shifts->size() * sizeof(unsigned int), NULL, NULL);
    dispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, dispersedData.size() * sizeof(dataType), NULL, NULL);
    dedispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_WRITE, dedispersedData.size() * sizeof(dataType), NULL, NULL);
  } catch ( cl::Error &err ) {
    std::cerr << "OpenCL error allocating memory: " << isa::utils::toString< cl_int >(err.err()) << "." << std::endl;
    return 1;
  }

	srand(time(NULL));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerDispersedChannel(); sample++ ) {
      dispersedData[(channel * observation.getNrSamplesPerDispersedChannel()) + sample] = static_cast< dataType >(rand() % 10);
		}
	}

  // Copy data structures to device
  try {
    clQueues->at(clDeviceID)[0].enqueueWriteBuffer(shifts_d, CL_FALSE, 0, shifts->size() * sizeof(unsigned int), reinterpret_cast< void * >(shifts->data()), NULL, NULL);
    clQueues->at(clDeviceID)[0].enqueueWriteBuffer(dispersedData_d, CL_FALSE, 0, dispersedData.size() * sizeof(dataType), reinterpret_cast< void * >(dispersedData.data()), NULL, NULL);
  } catch ( cl::Error &err ) {
    std::cerr << "OpenCL error H2D transfer: " << isa::utils::toString< cl_int >(err.err()) << "." << std::endl;
    return 1;
  }

	// Generate kernel
  std::string * code = PulsarSearch::getDedispersionOpenCL< dataType >(localMem, nrSamplesPerBlock, nrDMsPerBlock, nrSamplesPerThread, nrDMsPerThread, typeName, observation, *shifts);
  cl::Kernel * kernel;
  if ( print ) {
    std::cout << *code << std::endl;
  }
	try {
    kernel = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
	} catch ( isa::OpenCL::OpenCLError &err ) {
    std::cerr << err.what() << std::endl;
		return 1;
	}

  // Run OpenCL kernel and CPU control
  try {
    cl::NDRange global(observation.getNrSamplesPerPaddedSecond() / nrSamplesPerThread, observation.getNrDMs() / nrDMsPerThread);
    cl::NDRange local(nrSamplesPerBlock, nrDMsPerBlock);

    kernel->setArg(0, dispersedData_d);
    kernel->setArg(1, dedispersedData_d);
    kernel->setArg(2, shifts_d);
    clQueues->at(clDeviceID)[0].enqueueNDRangeKernel(*kernel, cl::NullRange, global, local);
    PulsarSearch::dedispersion< dataType >(observation, dispersedData, controlData, *shifts);
    clQueues->at(clDeviceID)[0].enqueueReadBuffer(dedispersedData_d, CL_TRUE, 0, dedispersedData.size() * sizeof(dataType), reinterpret_cast< void * >(dedispersedData.data()));
  } catch ( cl::Error &err ) {
    std::cerr << "OpenCL error kernel execution: " << isa::utils::toString< cl_int >(err.err()) << "." << std::endl;
    return 1;
  }

	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
      if ( !isa::utils::same(controlData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample], dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]) ) {
        wrongSamples++;
			}
		}
	}

  if ( wrongSamples > 0 ) {
    std::cout << "Wrong samples: " << wrongSamples << " (" << (wrongSamples * 100.0) / (static_cast< long long unsigned int >(observation.getNrDMs()) * observation.getNrSamplesPerSecond()) << "%)." << std::endl;
  } else {
    std::cout << "TEST PASSED." << std::endl;
  }

	return 0;
}

