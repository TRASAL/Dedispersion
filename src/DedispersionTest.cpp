// Copyright 2017 Netherlands Institute for Radio Astronomy (ASTRON)
// Copyright 2017 Netherlands eScience Center
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

#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <ReadData.hpp>
#include <SynthesizedBeams.hpp>
#include <InitializeOpenCL.hpp>
#include <Kernel.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>


int main(int argc, char *argv[]) {
  // TODO: implement split_batches mode
  // TODO: implement a way to test external beam drivers
  unsigned int padding = 0;
  uint8_t inputBits = 0;
  bool printCode = false;
  bool printResults = false;
  bool random = false;
  bool singleStep = false;
  bool stepOne = false;
  unsigned int clPlatformID = 0;
  unsigned int clDeviceID = 0;
  uint64_t wrongSamples = 0;
  std::string channelsFile;
  Dedispersion::DedispersionConf conf;
  AstroData::Observation observation;

  try {
    isa::utils::ArgumentList args(argc, argv);
    printCode = args.getSwitch("-print_code");
    printResults = args.getSwitch("-print_results");
    random = args.getSwitch("-random");
    singleStep = args.getSwitch("-single_step");
    stepOne = args.getSwitch("-step_one");
    bool stepTwo = args.getSwitch("-step_two");
    if ( (static_cast< unsigned int >(singleStep) + static_cast< unsigned int >(stepOne) + static_cast< unsigned int >(stepTwo)) != 1 ) {
      std::cerr << "Mutually exclusive modes, select one: -single_step -step_one -step_two" << std::endl;
      return 1;
    }
    clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
    clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
    if ( singleStep || stepOne ) {
      inputBits = args.getSwitchArgument< unsigned int >("-input_bits");
      channelsFile = args.getSwitchArgument< std::string >("-zapped_channels");
    }
    padding = args.getSwitchArgument< unsigned int >("-padding");
    // Kernel configuration
    conf.setLocalMem(args.getSwitch("-local"));
    conf.setNrThreadsD0(args.getSwitchArgument< unsigned int >("-threadsD0"));
    conf.setNrThreadsD1(args.getSwitchArgument< unsigned int >("-threadsD1"));
    conf.setNrItemsD0(args.getSwitchArgument< unsigned int >("-itemsD0"));
    conf.setNrItemsD1(args.getSwitchArgument< unsigned int >("-itemsD1"));
    conf.setUnroll(args.getSwitchArgument< unsigned int >("-unroll"));
    // Observation configuration
    observation.setNrBeams(args.getSwitchArgument< unsigned int >("-beams"));
    observation.setNrSamplesPerBatch(args.getSwitchArgument< unsigned int >("-samples"));
    if ( singleStep ) {
      observation.setNrSynthesizedBeams(args.getSwitchArgument< unsigned int >("-synthesized_beams"));
      observation.setFrequencyRange(1, args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
    } else if ( stepOne ) {
      observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-subbanding_dms"), args.getSwitchArgument< float >("-subbanding_dm_first"), args.getSwitchArgument< float >("-subbanding_dm_step"), true);
    } else if ( stepTwo ) {
      observation.setNrSynthesizedBeams(args.getSwitchArgument< unsigned int >("-synthesized_beams"));
      observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-subbanding_dms"), 0.0f, 0.0f, true);
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
    }
  } catch  ( isa::utils::SwitchNotFound & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception & err ) {
    std::cerr << "Usage: " << argv[0] << " [-print_code] [-print_results] [-random] [-single_step | -step_one | -step_two] -opencl_platform ... -opencl_device ... -padding ... -vector ... [-local] -threadsD0 ... -threadsD1 ... -itemsD0 ... -itemsD1 ... -unroll ... -beams ... -channels ... -min_freq ... -channel_bandwidth ... -samples ..." << std::endl;
    std::cerr << "\t-single_step -input_bits ... -zapped_channels ... -synthesized_beams ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    std::cerr << "\t-step_one -input_bits ... -zapped_channels ... -subbands ... -subbanding_dms ... -subbanding_dm_first ... -subbanding_dm_step ..." << std::endl;
    std::cerr << "\t-step_two -synthesized_beams ... -subbands ... -subbanding_dms ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    return 1;
  }

  // Initialize OpenCL
  cl::Context * clContext = new cl::Context();
  std::vector< cl::Platform > * clPlatforms = new std::vector< cl::Platform >();
  std::vector< cl::Device > * clDevices = new std::vector< cl::Device >();
  std::vector< std::vector< cl::CommandQueue > > * clQueues = new std::vector< std::vector < cl::CommandQueue > >();
  isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, clContext, clDevices, clQueues);

  // Allocate host memory
  std::vector< inputDataType > dispersedData;
  std::vector< outputDataType > subbandedData;
  std::vector< outputDataType > subbandedData_c;
  std::vector< outputDataType > dedispersedData;
  std::vector< outputDataType > dedispersedData_c;
  std::vector< float > * shiftsSingleStep = Dedispersion::getShifts(observation, padding);
  std::vector< float > * shiftsStepOne = Dedispersion::getShifts(observation, padding);
  std::vector< float > * shiftsStepTwo = Dedispersion::getShiftsStepTwo(observation, padding);
  std::vector<unsigned int> zappedChannels(observation.getNrChannels(padding / sizeof(unsigned int)));
  std::vector<unsigned int> beamMappingSingleStep(observation.getNrSynthesizedBeams() * observation.getNrChannels(padding / sizeof(unsigned int)));
  std::vector<unsigned int> beamMappingStepTwo(observation.getNrSynthesizedBeams() * observation.getNrSubbands(padding / sizeof(unsigned int)));

  if ( singleStep || stepOne ) {
    AstroData::readZappedChannels(observation, channelsFile, zappedChannels);
  }
  if ( singleStep ) {
    observation.setNrSamplesPerDispersedBatch(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsSingleStep->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
    if ( inputBits >= 8 ) {
      dispersedData.resize(observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerDispersedBatch(padding / sizeof(inputDataType)));
      dedispersedData.resize(observation.getNrSynthesizedBeams() * observation.getNrDMs() * observation.getNrSamplesPerBatch(padding / sizeof(outputDataType)));
      dedispersedData_c.resize(observation.getNrSynthesizedBeams() * observation.getNrDMs() * observation.getNrSamplesPerBatch(padding / sizeof(outputDataType)));
    } else {
      dispersedData.resize(observation.getNrBeams() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(inputDataType)));
      dedispersedData.resize(observation.getNrSynthesizedBeams() * observation.getNrDMs() * isa::utils::pad(observation.getNrSamplesPerBatch() / (8 / inputBits), padding / sizeof(outputDataType)));
      dedispersedData_c.resize(observation.getNrSynthesizedBeams() * observation.getNrDMs() * isa::utils::pad(observation.getNrSamplesPerBatch() / (8 / inputBits), padding / sizeof(outputDataType)));
    }
  } else if ( stepOne ) {
    observation.setNrSamplesPerBatch(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsStepTwo->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))), true);
    observation.setNrSamplesPerDispersedBatch(observation.getNrSamplesPerBatch(true) + static_cast< unsigned int >(shiftsStepOne->at(0) * (observation.getFirstDM(true) + ((observation.getNrDMs(true) - 1) * observation.getDMStep(true)))), true);
    if ( inputBits >= 8 ) {
      dispersedData.resize(observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerDispersedBatch(true, padding / sizeof(inputDataType)));
      subbandedData.resize(observation.getNrBeams() * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType)));
      subbandedData_c.resize(observation.getNrBeams() * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType)));
    } else {
      dispersedData.resize(observation.getNrBeams() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(inputDataType)));
      subbandedData.resize(observation.getNrBeams() * observation.getNrDMs(true) * observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / (8 / inputBits), padding / sizeof(outputDataType)));
      subbandedData_c.resize(observation.getNrBeams() * observation.getNrDMs(true) * observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / (8 / inputBits), padding / sizeof(outputDataType)));
    }
  } else {
    observation.setNrSamplesPerBatch(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsStepTwo->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))), true);
    subbandedData.resize(observation.getNrBeams() * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType)));
    dedispersedData.resize(observation.getNrSynthesizedBeams() * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType)));
    dedispersedData_c.resize(observation.getNrSynthesizedBeams() * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType)));
  }

  // Allocate device memory
  cl::Buffer shiftsSingleStep_d;
  cl::Buffer shiftsStepOne_d;
  cl::Buffer shiftsStepTwo_d;
  cl::Buffer zappedChannels_d;
  cl::Buffer dispersedData_d;
  cl::Buffer subbandedData_d;
  cl::Buffer dedispersedData_d;
  cl::Buffer beamMappingSingleStep_d;
  cl::Buffer beamMappingStepTwo_d;
  try {
    if ( singleStep ) {
      shiftsSingleStep_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, shiftsSingleStep->size() * sizeof(float), 0, 0);
      zappedChannels_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(unsigned int), 0, 0);
      dispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, dispersedData.size() * sizeof(inputDataType), 0, 0);
      dedispersedData_d = cl::Buffer(*clContext, CL_MEM_WRITE_ONLY, dedispersedData.size() * sizeof(outputDataType), 0, 0);
      beamMappingSingleStep_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, beamMappingSingleStep.size() * sizeof(unsigned int), 0, 0);
    } else if ( stepOne ) {
      shiftsStepOne_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, shiftsStepOne->size() * sizeof(float), 0, 0);
      zappedChannels_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(unsigned int), 0, 0);
      dispersedData_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, dispersedData.size() * sizeof(inputDataType), 0, 0);
      subbandedData_d = cl::Buffer(*clContext, CL_MEM_WRITE_ONLY, subbandedData.size() * sizeof(outputDataType), 0, 0);
    } else {
      shiftsStepTwo_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, shiftsStepTwo->size() * sizeof(float), 0, 0);
      subbandedData_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, subbandedData.size() * sizeof(outputDataType), 0, 0);
      dedispersedData_d = cl::Buffer(*clContext, CL_MEM_WRITE_ONLY, dedispersedData.size() * sizeof(outputDataType), 0, 0);
      beamMappingStepTwo_d = cl::Buffer(*clContext, CL_MEM_READ_ONLY, beamMappingStepTwo.size() * sizeof(unsigned int), 0, 0);
    }
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error allocating memory: " << std::to_string(err.err()) << "." << std::endl;
    return 1;
  }

  // Generate test data
  srand(time(0));
  if ( singleStep ) {
    for ( unsigned int beam = 0; beam < observation.getNrBeams(); beam++ ) {
      for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
        for ( unsigned int sample = 0; sample < observation.getNrSamplesPerDispersedBatch(); sample++ ) {
          if ( inputBits >= 8 ) {
            if ( conf.getSplitBatches() ) {
            } else {
              if ( random ) {
                dispersedData[(beam * observation.getNrChannels() * observation.getNrSamplesPerDispersedBatch(false, padding / sizeof(inputDataType))) + (channel * observation.getNrSamplesPerDispersedBatch(false, padding / sizeof(inputDataType))) + sample] = static_cast< inputDataType >(rand() % 10);
              } else {
                dispersedData[(beam * observation.getNrChannels() * observation.getNrSamplesPerDispersedBatch(false, padding / sizeof(inputDataType))) + (channel * observation.getNrSamplesPerDispersedBatch(false, padding / sizeof(inputDataType))) + sample] = static_cast< inputDataType >(10);
              }
            }
          } else {
            unsigned int byte = 0;
            uint8_t firstBit = 0;
            uint8_t value = 0;
            uint8_t buffer = 0;

            if ( random ) {
              value = rand() % inputBits;
            } else {
              value = inputBits - 1;
            }
            if ( conf.getSplitBatches() ) {
            } else {
              byte = sample / (8 / inputBits);
              firstBit = (sample % (8 / inputBits)) * inputBits;
              buffer = dispersedData[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(inputDataType))) + byte];
            }

            for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
              if ( conf.getSplitBatches() ) {
              } else {
                isa::utils::setBit(buffer, isa::utils::getBit(value, bit), firstBit + bit);
              }
            }

            if ( conf.getSplitBatches() ) {
            } else {
              dispersedData[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(inputDataType))) + byte] = buffer;
            }
          }
        }
      }
    }
    AstroData::generateBeamMapping(observation, beamMappingSingleStep, padding);
  } else if ( stepOne ) {
    for ( unsigned int beam = 0; beam < observation.getNrBeams(); beam++ ) {
      for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
        for ( unsigned int sample = 0; sample < observation.getNrSamplesPerDispersedBatch(true); sample++ ) {
          if ( inputBits >= 8 ) {
            if ( conf.getSplitBatches() ) {
            } else {
              if ( random ) {
                dispersedData[(beam * observation.getNrChannels() * observation.getNrSamplesPerDispersedBatch(true, padding / sizeof(inputDataType))) + (channel * observation.getNrSamplesPerDispersedBatch(true, padding / sizeof(inputDataType))) + sample] = static_cast< inputDataType >(rand() % 10);
              } else {
                dispersedData[(beam * observation.getNrChannels() * observation.getNrSamplesPerDispersedBatch(true, padding / sizeof(inputDataType))) + (channel * observation.getNrSamplesPerDispersedBatch(true, padding / sizeof(inputDataType))) + sample] = static_cast< inputDataType >(10);
              }
            }
          } else {
            unsigned int byte = 0;
            uint8_t firstBit = 0;
            uint8_t value = 0;
            uint8_t buffer = 0;

            if ( random ) {
              value = rand() % inputBits;
            } else {
              value = inputBits - 1;
            }
            if ( conf.getSplitBatches() ) {
            } else {
              byte = sample / (8 / inputBits);
              firstBit = (sample % (8 / inputBits)) * inputBits;
              buffer = dispersedData[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(inputDataType))) + byte];
            }

            for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
              if ( conf.getSplitBatches() ) {
              } else {
                isa::utils::setBit(buffer, isa::utils::getBit(value, bit), firstBit + bit);
              }
            }

            if ( conf.getSplitBatches() ) {
            } else {
              dispersedData[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(inputDataType))) + byte] = buffer;
            }
          }
        }
      }
    }
  } else {
    for ( unsigned int beam = 0; beam < observation.getNrBeams(); beam++ ) {
      for ( unsigned int dm = 0; dm < observation.getNrDMs(true); dm++ ) {
        for ( unsigned int subband = 0; subband < observation.getNrSubbands(); subband++ ) {
          for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch(true); sample++ ) {
            if ( random ) {
              subbandedData[(beam * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (dm * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (subband * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + sample] = static_cast< outputDataType >(rand() % 10);
            } else {
              subbandedData[(beam * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (dm * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (subband * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + sample] = static_cast< outputDataType >(10);
            }
          }
        }
      }
    }
    AstroData::generateBeamMapping(observation, beamMappingStepTwo, padding, true);
  }

  // Copy data from host to device H2D
  try {
    if ( singleStep ) {
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(shiftsSingleStep_d, CL_FALSE, 0, shiftsSingleStep->size() * sizeof(float), reinterpret_cast< void * >(shiftsSingleStep->data()), 0, 0);
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(unsigned int), reinterpret_cast< void * >(zappedChannels.data()), 0, 0);
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(dispersedData_d, CL_FALSE, 0, dispersedData.size() * sizeof(inputDataType), reinterpret_cast< void * >(dispersedData.data()), 0, 0);
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(beamMappingSingleStep_d, CL_FALSE, 0, beamMappingSingleStep.size() * sizeof(unsigned int), reinterpret_cast< void * >(beamMappingSingleStep.data()), 0, 0);
    } else if ( stepOne ) {
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(shiftsStepOne_d, CL_FALSE, 0, shiftsStepOne->size() * sizeof(float), reinterpret_cast< void * >(shiftsStepOne->data()), 0, 0);
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(unsigned int), reinterpret_cast< void * >(zappedChannels.data()), 0, 0);
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(dispersedData_d, CL_FALSE, 0, dispersedData.size() * sizeof(inputDataType), reinterpret_cast< void * >(dispersedData.data()), 0, 0);
    } else {
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(shiftsStepTwo_d, CL_FALSE, 0, shiftsStepTwo->size() * sizeof(float), reinterpret_cast< void * >(shiftsStepTwo->data()), 0, 0);
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(subbandedData_d, CL_FALSE, 0, subbandedData.size() * sizeof(outputDataType), reinterpret_cast< void * >(subbandedData.data()), 0, 0);
      clQueues->at(clDeviceID)[0].enqueueWriteBuffer(beamMappingStepTwo_d, CL_FALSE, 0, beamMappingStepTwo.size() * sizeof(unsigned int), reinterpret_cast< void * >(beamMappingStepTwo.data()), 0, 0);
    }
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error H2D transfer: " << std::to_string(err.err()) << "." << std::endl;
    return 1;
  }

  // Generate kernel
  std::string * code = 0;
  cl::Kernel * kernel;

  if ( singleStep ) {
    code = Dedispersion::getDedispersionOpenCL< inputDataType, outputDataType >(conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shiftsSingleStep);
  } else if ( stepOne ) {
    code = Dedispersion::getSubbandDedispersionStepOneOpenCL< inputDataType, outputDataType >(conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shiftsStepOne);
  } else {
    code = Dedispersion::getSubbandDedispersionStepTwoOpenCL< outputDataType >(conf, padding, outputDataName, observation, *shiftsStepTwo);
  }
  if ( printCode ) {
    std::cout << *code << std::endl;
  }
  try {
    if ( singleStep ) {
      kernel = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
    } else if ( stepOne ) {
      kernel = isa::OpenCL::compile("dedispersionStepOne", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
    } else {
      kernel = isa::OpenCL::compile("dedispersionStepTwo", *code, "-cl-mad-enable -Werror", *clContext, clDevices->at(clDeviceID));
    }
  } catch ( isa::OpenCL::OpenCLError & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }

  // Run OpenCL kernel and CPU control
  try {
    cl::NDRange global;
    cl::NDRange local;

    if ( singleStep ) {
      global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch() / conf.getNrItemsD0(), conf.getNrThreadsD0()), observation.getNrDMs() / conf.getNrItemsD1(), observation.getNrSynthesizedBeams());
      local = cl::NDRange(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
    } else if ( stepOne ) {
      global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch(true) / conf.getNrItemsD0(), conf.getNrThreadsD0()), observation.getNrDMs(true) / conf.getNrItemsD1(), observation.getNrBeams() * observation.getNrSubbands());
      local = cl::NDRange(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
    } else {
      global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch() / conf.getNrItemsD0(), conf.getNrThreadsD0()), observation.getNrDMs() / conf.getNrItemsD1(), observation.getNrSynthesizedBeams() * observation.getNrDMs(true));
      local = cl::NDRange(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
    }

    if ( singleStep ) {
      if ( conf.getSplitBatches() ) {
      } else {
        kernel->setArg(0, dispersedData_d);
        kernel->setArg(1, dedispersedData_d);
        kernel->setArg(2, beamMappingSingleStep_d);
        kernel->setArg(3, zappedChannels_d);
        kernel->setArg(4, shiftsSingleStep_d);
      }
    } else if ( stepOne ) {
      kernel->setArg(0, dispersedData_d);
      kernel->setArg(1, subbandedData_d);
      kernel->setArg(2, zappedChannels_d);
      kernel->setArg(3, shiftsStepOne_d);
    } else {
      kernel->setArg(0, subbandedData_d);
      kernel->setArg(1, dedispersedData_d);
      kernel->setArg(2, beamMappingStepTwo_d);
      kernel->setArg(3, shiftsStepTwo_d);
    }
    clQueues->at(clDeviceID)[0].enqueueNDRangeKernel(*kernel, cl::NullRange, global, local);
    if ( singleStep ) {
      if ( conf.getSplitBatches() ) {
      } else {
        Dedispersion::dedispersion< inputDataType, intermediateDataType, outputDataType >(observation, zappedChannels, beamMappingSingleStep, dispersedData, dedispersedData_c, *shiftsSingleStep, padding, inputBits);
      }
      clQueues->at(clDeviceID)[0].enqueueReadBuffer(dedispersedData_d, CL_TRUE, 0, dedispersedData.size() * sizeof(outputDataType), reinterpret_cast< void * >(dedispersedData.data()));
    } else if ( stepOne ) {
      Dedispersion::subbandDedispersionStepOne< inputDataType, intermediateDataType, outputDataType >(observation, zappedChannels, dispersedData, subbandedData_c, *shiftsStepOne, padding, inputBits);
      clQueues->at(clDeviceID)[0].enqueueReadBuffer(subbandedData_d, CL_TRUE, 0, subbandedData.size() * sizeof(outputDataType), reinterpret_cast< void * >(subbandedData.data()));
    } else {
      Dedispersion::subbandDedispersionStepTwo< outputDataType, intermediateDataType, outputDataType >(observation, beamMappingStepTwo, subbandedData, dedispersedData_c, *shiftsStepTwo, padding);
      clQueues->at(clDeviceID)[0].enqueueReadBuffer(dedispersedData_d, CL_TRUE, 0, dedispersedData.size() * sizeof(outputDataType), reinterpret_cast< void * >(dedispersedData.data()));
    }
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error kernel execution: " << std::to_string(err.err()) << "." << std::endl;
    return 1;
  }

  // Compare results
  if ( singleStep ) {
    for ( unsigned int syntBeam = 0; syntBeam < observation.getNrSynthesizedBeams(); syntBeam++ ) {
      if ( printResults ) {
        std::cout << "Synthesized Beam: " << syntBeam << std::endl;
      }
      for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
        if ( printResults ) {
          std::cout << "DM: " << dm << " = ";
        }
        for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch(); sample++ ) {
          if ( !isa::utils::same(dedispersedData[(syntBeam * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample], dedispersedData_c[(syntBeam * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample]) ) {
            wrongSamples++;
          }
          if ( printResults ) {
            std::cout << dedispersedData[(syntBeam * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample] << "," << dedispersedData_c[(syntBeam * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample] << " ";
          }
        }
        if ( printResults ) {
          std::cout << std::endl;
        }
      }
      if ( printResults ) {
        std::cout << std::endl;
      }
    }
  } else if ( stepOne ) {
    for ( unsigned int beam = 0; beam < observation.getNrBeams(); beam++ ) {
      if ( printResults ) {
        std::cout << "Beam: " << beam << std::endl;
      }
      for ( unsigned int dm = 0; dm < observation.getNrDMs(true); dm++ ) {
        if ( printResults ) {
          std::cout << "DM: " << dm << std::endl;
        }
        for ( unsigned int subband = 0; subband < observation.getNrSubbands(); subband++ ) {
          for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch(true); sample++ ) {
            if ( !isa::utils::same(subbandedData[(beam * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (dm * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (subband * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + sample], subbandedData_c[(beam * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (dm * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (subband * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + sample]) ) {
            }
            if ( printResults) {
              std::cout << subbandedData[(beam * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (dm * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (subband * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + sample] << "," << subbandedData_c[(beam * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (dm * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + (subband * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType))) + sample] << " ";
            }
          }
          if ( printResults) {
            std::cout << std::endl;
          }
        }
        if ( printResults) {
          std::cout << std::endl;
        }
      }
      if ( printResults) {
        std::cout << std::endl;
      }
    }
  } else {
    for ( unsigned int syntBeam = 0; syntBeam < observation.getNrSynthesizedBeams(); syntBeam++ ) {
      if ( printResults ) {
        std::cout << "Synthesized Beam: " << syntBeam << std::endl;
      }
      for ( unsigned int dm = 0; dm < observation.getNrDMs(true) * observation.getNrDMs(); dm++ ) {
        if ( printResults ) {
          std::cout << "DM: " << dm << " = ";
        }
        for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch(); sample++ ) {
          if ( !isa::utils::same(dedispersedData[(syntBeam * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample], dedispersedData_c[(syntBeam * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample]) ) {
            wrongSamples++;
          }
          if ( printResults ) {
            std::cout << dedispersedData[(syntBeam * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample] << "," << dedispersedData_c[(syntBeam * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + (dm * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType))) + sample] << " ";
          }
        }
        if ( printResults ) {
          std::cout << std::endl;
        }
      }
      if ( printResults ) {
        std::cout << std::endl;
      }
    }
  }

  if ( wrongSamples > 0 ) {
    if ( singleStep ) {
      std::cout << "Wrong samples: " << wrongSamples << " (" << (wrongSamples * 100.0) / (static_cast< uint64_t >(observation.getNrSynthesizedBeams()) * observation.getNrDMs() * observation.getNrSamplesPerBatch()) << "%)." << std::endl;
    } else if ( stepOne ) {
      std::cout << "Wrong samples: " << wrongSamples << " (" << (wrongSamples * 100.0) / (static_cast< uint64_t >(observation.getNrBeams()) * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true)) << "%)." << std::endl;
    } else {
      std::cout << "Wrong samples: " << wrongSamples << " (" << (wrongSamples * 100.0) / (static_cast< uint64_t >(observation.getNrSynthesizedBeams()) * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch()) << "%)." << std::endl;
    }
  } else {
    std::cout << "TEST PASSED." << std::endl;
  }

  return 0;
}

