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

#include <utils.hpp>
#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <ReadData.hpp>
#include <SynthesizedBeams.hpp>
#include <InitializeOpenCL.hpp>
#include <Kernel.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
#include <Timer.hpp>
#include <Stats.hpp>

void initializeDeviceMemorySingleStep(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shifts, cl::Buffer * shifts_d, std::vector<unsigned int> & zappedChannels, cl::Buffer * zappedChannels_d, std::vector<unsigned int> & beamMapping, cl::Buffer * beamMapping_d, const unsigned int dispersedData_size, cl::Buffer * dispersedData_d, const unsigned int dedispersedData_size, cl::Buffer * dedispersedData_d);
void initializeDeviceMemoryStepOne(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepOne, cl::Buffer * shiftsStepOne_d, std::vector<unsigned int> & zappedChannels, cl::Buffer * zappedChannels_d, const unsigned int dispersedData_size, cl::Buffer * dispersedData_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d);
void initializeDeviceMemoryStepTwo(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepTwo, cl::Buffer * shiftsStepTwo_d, std::vector<unsigned int> & beamMapping, cl::Buffer * beamMapping_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d, const unsigned int dedispersedData_size, cl::Buffer * dedispersedData_d);

int main(int argc, char * argv[]) {
  // TODO: implement split_batches mode
  bool singleStep = false;
  bool stepOne = false;
  bool initializeDeviceMemory = false;
  bool bestMode = false;
  unsigned int padding = 0;
  unsigned int nrIterations = 0;
  unsigned int clPlatformID = 0;
  unsigned int clDeviceID = 0;
  unsigned int minThreads = 0;
  unsigned int maxThreads = 0;
  unsigned int maxRows = 0;
  unsigned int maxColumns = 0;
  unsigned int vectorWidth = 0;
  unsigned int maxItems = 0;
  unsigned int maxSampleItems = 0;
  unsigned int maxDMItems = 0;
  unsigned int maxUnroll = 0;
  double bestGFLOPs = 0.0;
  std::string channelsFile;
  AstroData::Observation observation;
  std::vector<Dedispersion::DedispersionConf> confs;
  Dedispersion::DedispersionConf bestConf;
  cl::Event event;

  try {
    isa::utils::ArgumentList args(argc, argv);

    nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
    clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
    clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
    bestMode = args.getSwitch("-best");
    singleStep = args.getSwitch("-single_step");
    stepOne = args.getSwitch("-step_one");
    bool stepTwo = args.getSwitch("-step_two");
    if ( (static_cast< unsigned int >(singleStep) + static_cast< unsigned int >(stepOne) + static_cast< unsigned int >(stepTwo)) != 1 ) {
      std::cerr << "Mutually exclusive modes, select one: -single_step -step_one -step_two" << std::endl;
      return 1;
    }
    padding = args.getSwitchArgument< unsigned int >("-padding");
    vectorWidth = args.getSwitchArgument< unsigned int >("-vector");
    if ( singleStep || stepOne ) {
      channelsFile = args.getSwitchArgument< std::string >("-zapped_channels");
    }
    // Tuning constraints
    minThreads = args.getSwitchArgument< unsigned int >("-min_threads");
    maxThreads = args.getSwitchArgument< unsigned int >("-max_threads");
    maxRows = args.getSwitchArgument< unsigned int >("-max_rows");
    maxColumns = args.getSwitchArgument< unsigned int >("-max_columns");
    maxItems = args.getSwitchArgument< unsigned int >("-max_items");
    maxSampleItems = args.getSwitchArgument< unsigned int >("-max_sample_items");
    maxDMItems = args.getSwitchArgument< unsigned int >("-max_dm_items");
    maxUnroll = args.getSwitchArgument< unsigned int >("-max_unroll");
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
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
    } else if ( stepTwo ) {
      observation.setNrSynthesizedBeams(args.getSwitchArgument< unsigned int >("-synthesized_beams"));
      observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-subbanding_dms"), 0.0f, 0.0f, true);
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
    }
  } catch ( isa::utils::EmptyCommandLine & err ) {
    std::cerr << argv[0] << " -iterations ... -opencl_platform ... -opencl_device ... [-best] [-single_step | -step_one | -step_two] -padding ... -vector ... -min_threads ... -max_threads ... -max_columns ... -max_rows ... -max_items ... -max_sample_items ... -max_dm_items ... -max_unroll ... -beams ... -samples ...-min_freq ... -channel_bandwidth ... -channels ... " << std::endl;
    std::cerr << "\t-single_step -input_bits ... -zapped_channels ... -synthesized_beams ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    std::cerr << "\t-step_one -input_bits ... -zapped_channels ... -subbands ... -subbanding_dms ... -subbanding_dm_first ... -subbanding_dm_step ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    std::cerr << "\t-step_two -synthesized_beams ... -subbands ... -subbanding_dms ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    return 1;
  } catch ( std::exception & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }

  // Allocate host memory
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
  } else if ( stepOne ) {
    observation.setNrSamplesPerBatch(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsStepTwo->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))), true);
    observation.setNrSamplesPerDispersedBatch(observation.getNrSamplesPerBatch(true) + static_cast< unsigned int >(shiftsStepOne->at(0) * (observation.getFirstDM(true) + ((observation.getNrDMs(true) - 1) * observation.getDMStep(true)))), true);
  } else {
    observation.setNrSamplesPerBatch(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsStepTwo->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))), true);
  }

  // Generate test data
  if ( singleStep ) {
    AstroData::generateBeamMapping(observation, beamMappingSingleStep, padding);
  } else if ( !stepOne ) {
    AstroData::generateBeamMapping(observation, beamMappingStepTwo, padding, true);
  }

  unsigned int dispersedData_size;
  unsigned int subbandedData_size;
  unsigned int dedispersedData_size;
  cl::Context clContext;
  std::vector< cl::Platform > * clPlatforms = new std::vector< cl::Platform >();
  std::vector< cl::Device > * clDevices = new std::vector< cl::Device >();
  std::vector< std::vector< cl::CommandQueue > > * clQueues = new std::vector< std::vector < cl::CommandQueue > >();
  cl::Buffer shiftsSingleStep_d;
  cl::Buffer shiftsStepOne_d;
  cl::Buffer shiftsStepTwo_d;
  cl::Buffer zappedChannels_d;
  cl::Buffer beamMappingSingleStep_d;
  cl::Buffer beamMappingStepTwo_d;
  cl::Buffer dispersedData_d;
  cl::Buffer subbandedData_d;
  cl::Buffer dedispersedData_d;

  if ( singleStep ) {
    if ( inputBits >= 8 ) {
      dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerDispersedBatch(false, padding / sizeof(inputDataType));
    } else {
      dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(inputDataType));
    }
    dedispersedData_size = observation.getNrSynthesizedBeams() * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType));
  } else if ( stepOne ) {
    if ( inputBits >= 8 ) {
      dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerBatch(true, padding / sizeof(inputDataType));
    } else {
      dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(inputDataType));
    }
    subbandedData_size = observation.getNrBeams() * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType));
  } else {
    subbandedData_size = observation.getNrBeams() * observation.getNrDMs(true) * observation.getNrSubbands() * observation.getNrSamplesPerBatch(true, padding / sizeof(outputDataType));
    dedispersedData_size = observation.getNrSynthesizedBeams() * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSamplesPerBatch(false, padding / sizeof(outputDataType));
  }

  for ( unsigned int threadsD0 = minThreads; threadsD0 <= maxColumns; threadsD0 *= 2 ) {
    for ( unsigned int threadsD1 = 1; threadsD1 <= maxRows; threadsD1++ ) {
      if ( threadsD0 * threadsD1 > maxThreads ) {
        break;
      } else if ( (threadsD0 * threadsD1) % vectorWidth != 0 ) {
        continue;
      }
      for ( unsigned int itemsD0 = 1; itemsD0 <= maxSampleItems; itemsD0++ ) {
        if ( singleStep ) {
          if ( (observation.getNrSamplesPerBatch() % itemsD0) != 0 ) {
            continue;
          }
        } else if ( stepOne ) {
          if ( (observation.getNrSamplesPerBatch(true) % itemsD0) != 0 ) {
            continue;
          }
        } else {
          if ( (observation.getNrSamplesPerBatch() % itemsD0) != 0 ) {
            continue;
          }
        }
        for ( unsigned int itemsD1 = 1; itemsD1 <= maxDMItems; itemsD1++ ) {
          if ( singleStep ) {
            if ( (observation.getNrDMs() % (threadsD1 * itemsD1)) != 0 ) {
              continue;
            }
          } else if ( stepOne ) {
            if ( (observation.getNrDMs(true) % (threadsD1 * itemsD1)) != 0 ) {
              continue;
            }
          } else {
            if ( (observation.getNrDMs() % (threadsD1 * itemsD1)) != 0 ) {
              continue;
            }
          }
          for ( unsigned int unroll = 1; unroll <= maxUnroll; unroll++ ) {
            if ( singleStep ) {
              if ( observation.getNrChannels() % unroll != 0 ) {
                continue;
              }
            } else if ( stepOne ) {
              if ( observation.getNrChannelsPerSubband() % unroll != 0 ) {
                continue;
              }
            } else {
              if ( observation.getNrSubbands() % unroll != 0 ) {
                continue;
              }
            }

            // Generate configurations
            unsigned int nrItems, localNrItems;
            Dedispersion::DedispersionConf conf, localConf;

            conf.setNrThreadsD0(threadsD0);
            localConf.setNrThreadsD0(threadsD0);
            conf.setNrThreadsD1(threadsD1);
            localConf.setNrThreadsD1(threadsD1);
            conf.setNrItemsD0(itemsD0);
            localConf.setNrItemsD0(itemsD0);
            conf.setNrItemsD1(itemsD1);
            localConf.setNrItemsD1(itemsD1);
            conf.setUnroll(unroll);
            localConf.setUnroll(unroll);
            localConf.setLocalMem(true);

            nrItems = conf.getNrItemsD1() + (conf.getNrItemsD0() * conf.getNrItemsD1());
            localNrItems = nrItems;
            if ( singleStep ) {
              nrItems += 4;
              localNrItems += 9;
            } else if ( stepOne ) {
              nrItems += 5;
              localNrItems += 10;
            } else {
              nrItems += 5;
              localNrItems += 10;
            }
            if ( inputBits < 8 ) {
              nrItems += 4;
              localNrItems += 4;
            }

            if ( nrItems <= maxItems ) {
              confs.push_back(conf);
            }
            if ( localNrItems <= maxItems ) {
              confs.push_back(localConf);
            }
          }
        }
      }
    }
  }

  if ( !bestMode ) {
    std::cout << std::fixed << std::endl;
    std::cout << "# nrBeams nrSynthesizedBeams nrSubbandingDMs nrDMs nrSubbands nrChannels nrZappedChannels nrSamplesSubbanding nrSamples *configuration* GFLOP/s time stdDeviation COV" << std::endl << std::endl;
  }

  for ( auto conf = confs.begin(); conf != confs.end(); ++conf  ) {
    // Generate kernel
    double gflops = 0.0;
    isa::utils::Timer timer;
    cl::Kernel * kernel;
    std::string * code = 0;

    if ( initializeDeviceMemory ) {
      delete clQueues;
      clQueues = new std::vector< std::vector < cl::CommandQueue > >();
      isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, &clContext, clDevices, clQueues);
      try {
        if ( singleStep ) {
          initializeDeviceMemorySingleStep(clContext, &(clQueues->at(clDeviceID)[0]), shiftsSingleStep, &shiftsSingleStep_d, zappedChannels, &zappedChannels_d, beamMappingSingleStep, &beamMappingSingleStep_d, dispersedData_size, &dispersedData_d, dedispersedData_size, &dedispersedData_d);
        } else if ( stepOne ) {
          initializeDeviceMemoryStepOne(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepOne, &shiftsStepOne_d, zappedChannels, &zappedChannels_d, dispersedData_size, &dispersedData_d, subbandedData_size, &subbandedData_d);
        } else {
          initializeDeviceMemoryStepTwo(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepTwo, &shiftsStepTwo_d, beamMappingStepTwo, &beamMappingStepTwo_d, subbandedData_size, &subbandedData_d, dedispersedData_size, &dedispersedData_d);
        }
      } catch ( cl::Error & err ) {
        std::cerr << "Error in memory allocation: ";
        std::cerr << std::to_string(err.err()) << "." << std::endl;
        return -1;
      }
      initializeDeviceMemory = false;
    }
    if ( singleStep ) {
      code = Dedispersion::getDedispersionOpenCL< inputDataType, outputDataType >(*conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shiftsSingleStep);
      gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrSynthesizedBeams()) * observation.getNrDMs() * (observation.getNrChannels() - observation.getNrZappedChannels()) * observation.getNrSamplesPerBatch());
    } else if ( stepOne ) {
      code = Dedispersion::getSubbandDedispersionStepOneOpenCL< inputDataType, outputDataType >(*conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shiftsStepOne);
      gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrBeams()) * observation.getNrDMs(true) * (observation.getNrChannels() - observation.getNrZappedChannels()) * observation.getNrSamplesPerBatch(true));
    } else {
      code = Dedispersion::getSubbandDedispersionStepTwoOpenCL< outputDataType >(*conf, padding, outputDataName, observation, *shiftsStepTwo);
      gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrSynthesizedBeams()) * observation.getNrDMs(true) * observation.getNrDMs() * observation.getNrSubbands() * observation.getNrSamplesPerBatch());
    }
    try {
      if ( singleStep ) {
        kernel = isa::OpenCL::compile("dedispersion", *code, "-cl-mad-enable -Werror", clContext, clDevices->at(clDeviceID));
      } else if ( stepOne ) {
        kernel = isa::OpenCL::compile("dedispersionStepOne", *code, "-cl-mad-enable -Werror", clContext, clDevices->at(clDeviceID));
      } else {
        kernel = isa::OpenCL::compile("dedispersionStepTwo", *code, "-cl-mad-enable -Werror", clContext, clDevices->at(clDeviceID));
      }
    } catch ( isa::OpenCL::OpenCLError & err ) {
      std::cerr << err.what() << std::endl;
      delete code;
      continue;
    }
    delete code;

    cl::NDRange global;
    cl::NDRange local;

    if ( singleStep ) {
      global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch() / (*conf).getNrItemsD0(), (*conf).getNrThreadsD0()), observation.getNrDMs() / (*conf).getNrItemsD1(), observation.getNrSynthesizedBeams());
      local = cl::NDRange((*conf).getNrThreadsD0(), (*conf).getNrThreadsD1(), 1);
    } else if ( stepOne ) {
      global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch(true) / (*conf).getNrItemsD0(), (*conf).getNrThreadsD0()), observation.getNrDMs(true) / (*conf).getNrItemsD1(), observation.getNrBeams() * observation.getNrSubbands());
      local = cl::NDRange((*conf).getNrThreadsD0(), (*conf).getNrThreadsD1(), 1);
    } else {
      global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch() / (*conf).getNrItemsD0(), (*conf).getNrThreadsD0()), observation.getNrDMs() / (*conf).getNrItemsD1(), observation.getNrSynthesizedBeams() * observation.getNrDMs(true));
      local = cl::NDRange((*conf).getNrThreadsD0(), (*conf).getNrThreadsD1(), 1);
    }

    if ( singleStep ) {
      kernel->setArg(0, dispersedData_d);
      kernel->setArg(1, dedispersedData_d);
      kernel->setArg(2, beamMappingSingleStep_d);
      kernel->setArg(3, zappedChannels_d);
      kernel->setArg(4, shiftsSingleStep_d);
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
      std::cerr << (*conf).print() << "): ";
      std::cerr << std::to_string(err.err()) << "." << std::endl;
      delete kernel;
      if ( err.err() == -4 || err.err() == -61 ) {
        return -1;
      } else if ( err.err() == -5 ) {
        // No need to reallocate the memory in this case
        continue;
      }
      initializeDeviceMemory = true;
      continue;
    }
    delete kernel;

    if ( (gflops / timer.getAverageTime()) > bestGFLOPs ) {
      bestGFLOPs = gflops / timer.getAverageTime();
      bestConf = *conf;
    }
    if ( !bestMode ) {
      std::cout << observation.getNrBeams() << " " << observation.getNrSynthesizedBeams() << " ";
      std::cout << observation.getNrDMs(true) << " " << observation.getNrDMs() << " ";
      std::cout << observation.getNrSubbands() << " " << observation.getNrChannels() << " " << observation.getNrZappedChannels() << " ";
      std::cout << observation.getNrSamplesPerBatch(true) << " " << observation.getNrSamplesPerBatch() << " ";
      std::cout << (*conf).print() << " ";
      std::cout << std::setprecision(3);
      std::cout << gflops / timer.getAverageTime() << " ";
      std::cout << std::setprecision(6);
      std::cout << timer.getAverageTime() << " " << timer.getStandardDeviation() << " ";
      std::cout << timer.getCoefficientOfVariation() <<  std::endl;
    }
  }

  if ( bestMode ) {
    if ( stepOne ) {
      std::cout << observation.getNrDMs(true) << " ";
    } else {
      std::cout << observation.getNrDMs() << " ";
    }
    std::cout << bestConf.print() << std::endl;
  } else {
    std::cout << std::endl;
  }

  return 0;
}

void initializeDeviceMemorySingleStep(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsSingleStep, cl::Buffer * shiftsSingleStep_d, std::vector<unsigned int> & zappedChannels, cl::Buffer * zappedChannels_d, std::vector<unsigned int> & beamMappingSingleStep, cl::Buffer * beamMappingSingleStep_d, const unsigned int dispersedData_size, cl::Buffer * dispersedData_d, const unsigned int dedispersedData_size, cl::Buffer * dedispersedData_d) {
  try {
    *shiftsSingleStep_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsSingleStep->size() * sizeof(float), 0, 0);
    *zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(unsigned int), 0, 0);
    *beamMappingSingleStep_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamMappingSingleStep.size() * sizeof(unsigned int), 0, 0);
    *dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
    *dedispersedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, dedispersedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shiftsSingleStep_d, CL_FALSE, 0, shiftsSingleStep->size() * sizeof(float), reinterpret_cast< void * >(shiftsSingleStep->data()));
    clQueue->enqueueWriteBuffer(*zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(unsigned int), reinterpret_cast< void * >(zappedChannels.data()));
    clQueue->enqueueWriteBuffer(*beamMappingSingleStep_d, CL_FALSE, 0, beamMappingSingleStep.size() * sizeof(unsigned int), reinterpret_cast< void * >(beamMappingSingleStep.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << std::to_string(err.err()) << "." << std::endl;
    throw;
  }
}

void initializeDeviceMemoryStepOne(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepOne, cl::Buffer * shiftsStepOne_d, std::vector<unsigned int> & zappedChannels, cl::Buffer * zappedChannels_d, const unsigned int dispersedData_size, cl::Buffer * dispersedData_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d) {
  try {
    *shiftsStepOne_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsStepOne->size() * sizeof(float), 0, 0);
    *zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(unsigned int), 0, 0);
    *dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
    *subbandedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, subbandedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shiftsStepOne_d, CL_FALSE, 0, shiftsStepOne->size() * sizeof(float), reinterpret_cast< void * >(shiftsStepOne->data()));
    clQueue->enqueueWriteBuffer(*zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(unsigned int), reinterpret_cast< void * >(zappedChannels.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << std::to_string(err.err()) << "." << std::endl;
    throw;
  }
}

void initializeDeviceMemoryStepTwo(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepTwo, cl::Buffer * shiftsStepTwo_d, std::vector<unsigned int> & beamMappingStepTwo, cl::Buffer * beamMappingStepTwo_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d, const unsigned int dedispersedData_size, cl::Buffer * dedispersedData_d) {
  try {
    *shiftsStepTwo_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsStepTwo->size() * sizeof(float), 0, 0);
    *beamMappingStepTwo_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamMappingStepTwo.size() * sizeof(unsigned int), 0, 0);
    *subbandedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, subbandedData_size * sizeof(outputDataType), 0, 0);
    *dedispersedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, dedispersedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shiftsStepTwo_d, CL_FALSE, 0, shiftsStepTwo->size() * sizeof(float), reinterpret_cast< void * >(shiftsStepTwo->data()));
    clQueue->enqueueWriteBuffer(*beamMappingStepTwo_d, CL_FALSE, 0, beamMappingStepTwo.size() * sizeof(unsigned int), reinterpret_cast< void * >(beamMappingStepTwo.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << std::to_string(err.err()) << "." << std::endl;
    throw;
  }
}

