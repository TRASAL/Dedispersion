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

void initializeDeviceMemorySingleStep(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shifts, cl::Buffer * shifts_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, std::vector< uint8_t > & beamDriver, cl::Buffer * beamDriver_d, cl::Buffer * dispersedData_d, const unsigned int dispersedData_size, cl::Buffer * dedispersedData_d, const unsigned int dedispersedData_size);
void initializeDeviceMemoryStepOne(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepOne, cl::Buffer * shiftsStepOne_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, const unsigned int dispersedData_size, cl::Buffer * dispersedData_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d);
void initializeDeviceMemoryStepTwo(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepTwo, cl::Buffer * shiftsStepTwo_d, std::vector< uint8_t > & beamDriver, cl::Buffer * beamDriver_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d, const unsigned int dedispersedData_size, cl::Buffer * dedispersedData_d);

int main(int argc, char * argv[]) {
  // TODO: implement split_seconds mode
  bool singleStep = false;
  bool stepOne = false;
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
  unsigned int maxItems = 0;
  unsigned int maxSampleItems = 0;
  unsigned int maxDMItems = 0;
  unsigned int maxUnroll = 0;
  std::string channelsFile;
  AstroData::Observation observation;
  PulsarSearch::DedispersionConf conf;
  cl::Event event;

  try {
    isa::utils::ArgumentList args(argc, argv);

    nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
    clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
    clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
    singleStep = args.getSwitch("-single_step");
    stepOne = args.getSwitch("-step_one");
    bool stepTwo = args.getSwitch("-step_two");
    if ( (static_cast< unsigned int >(singleStep) + static_cast< unsigned int >(stepOne) + static_cast< unsigned int >(stepTwo)) > 1 ) {
      std::cerr << "Mutually exclusive modes, select one: -single_step -step_one -step_two" << std::endl;
      return 1;
    }
    conf.setLocalMem(args.getSwitch("-local"));
    padding = args.getSwitchArgument< unsigned int >("-padding");
    vectorWidth = args.getSwitchArgument< unsigned int >("-vector");
    if ( singleStep || stepOne ) {
      inputBits = args.getSwitchArgument< unsigned int >("-input_bits");
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
      observation.setDMSubbandingRange(args.getSwitchArgument< unsigned int >("-subbanding_dms"), args.getSwitchArgument< float >("-subbanding_dm_first"), args.getSwitchArgument< float >("-subbanding_dm_step"));
    } else if ( stepTwo ) {
      observation.setNrSynthesizedBeams(args.getSwitchArgument< unsigned int >("-synthesized_beams"));
      observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
      observation.setDMSubbandingRange(args.getSwitchArgument< unsigned int >("-subbanding_dms"), 0.0f, 0.0f);
      observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
    }
  } catch ( isa::utils::EmptyCommandLine & err ) {
    std::cerr << argv[0] << " -iterations ... -opencl_platform ... -opencl_device ... [-single_step | -step_one | -step_two] [-local] -padding ... -vector ... -min_threads ... -max_threads ... -max_columns ... -max_rows ... -max_items ... -max_sample_items ... -max_dm_items ... -max_unroll ... -beams ... -samples ...-min_freq ... -channel_bandwidth ... -channels ... " << std::endl;
    std::cerr << "\t-single_step -input_bits ... -zapped_channels ... -synthesized_beams ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    std::cerr << "\t-step_one -input_bits ... -zapped_channels ... -subbands ... -subbanding_dms ... -subbanding_dm_first ... -subbanding_dm_step ..." << std::endl;
    std::cerr << "\t-step_two -synthesized_beams ... -subbands ... -subbanding_dms ... -dms ... -dm_first ... -dm_step ..." << std::endl;
    return 1;
  } catch ( std::exception & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }

  // Allocate host memory
  std::vector< float > * shiftsSingleStep = PulsarSearch::getShifts(observation, padding);
  std::vector< float > * shiftsStepOne = PulsarSearch::getShifts(observation, padding);
  std::vector< float > * shiftsStepTwo = PulsarSearch::getShiftsStepTwo(observation, padding);
  std::vector< uint8_t > zappedChannels(observation.getNrPaddedChannels(padding / sizeof(uint8_t)));
  std::vector< uint8_t > beamDriverSingleStep(observation.getNrSynthesizedBeams() * observation.getNrPaddedChannels(padding / sizeof(uint8_t)));
  std::vector< uint8_t > beamDriverStepTwo(observation.getNrSynthesizedBeams() * observation.getNrPaddedSubbands(padding / sizeof(uint8_t)));

  if ( singleStep || stepOne ) {
    AstroData::readZappedChannels(observation, channelsFile, zappedChannels);
  }
  if ( singleStep ) {
    observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsSingleStep->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
  } else if ( stepOne ) {
    observation.setNrSamplesPerBatchSubbanding(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsStepTwo->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
    observation.setNrSamplesPerSubbandingDispersedChannel(observation.getNrSamplesPerBatchSubbanding() + static_cast< unsigned int >(shiftsStepOne->at(0) * (observation.getFirstDMSubbanding() + ((observation.getNrDMsSubbanding() - 1) * observation.getDMSubbandingStep()))));
  } else {
    observation.setNrSamplesPerBatchSubbanding(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shiftsStepTwo->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
  }

  // Generate test data
  if ( singleStep ) {
    for ( unsigned int syntBeam = 0; syntBeam < observation.getNrSynthesizedBeams(); syntBeam++ ) {
      for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
        beamDriverSingleStep[(syntBeam * observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + channel] = syntBeam % observation.getNrBeams();
      }
    }
  } else if ( !stepOne ) {
    for ( unsigned int syntBeam = 0; syntBeam < observation.getNrSynthesizedBeams(); syntBeam++ ) {
      for ( unsigned int subband = 0; subband < observation.getNrSubbands(); subband++ ) {
        beamDriverStepTwo[(syntBeam * observation.getNrPaddedSubbands(padding / sizeof(uint8_t))) + subband] = syntBeam % observation.getNrBeams();
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
  unsigned int dispersedData_size;
  unsigned int subbandedData_size;
  unsigned int dedispersedData_size;
  cl::Buffer shiftsSingleStep_d;
  cl::Buffer shiftsStepOne_d;
  cl::Buffer shiftsStepTwo_d;
  cl::Buffer zappedChannels_d;
  cl::Buffer beamDriverSingleStep_d;
  cl::Buffer beamDriverStepTwo_d;
  cl::Buffer dispersedData_d;
  cl::Buffer subbandedData_d;
  cl::Buffer dedispersedData_d;

  try {
    if ( singleStep ) {
      shiftsSingleStep_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsSingleStep->size() * sizeof(float), 0, 0);
      zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(uint8_t), 0, 0);
      if ( inputBits >= 8 ) {
        dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType));
      } else {
        dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType));
      }
      dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
      dedispersedData_size = observation.getNrSynthesizedBeams() * observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType));
      dedispersedData_d = cl::Buffer(clContext, CL_MEM_WRITE_ONLY, dedispersedData_size * sizeof(outputDataType), 0, 0);
      beamDriverSingleStep_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamDriverSingleStep.size() * sizeof(uint8_t), 0, 0);
    } else if ( stepOne ) {
      shiftsStepOne_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsStepOne->size() * sizeof(float), 0, 0);
      zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(uint8_t), 0, 0);
      if ( inputBits >= 8 ) {
        dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * observation.getNrSamplesPerPaddedSubbandingDispersedChannel(padding / sizeof(inputDataType));
      } else {
        dispersedData_size = observation.getNrBeams() * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType));
      }
      dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
      subbandedData_size = observation.getNrBeams() * observation.getNrDMsSubbanding() * observation.getNrSubbands() * observation.getNrSamplesPerPaddedBatchSubbanding(padding / sizeof(outputDataType));
      subbandedData_d = cl::Buffer(clContext, CL_MEM_WRITE_ONLY, subbandedData_size * sizeof(outputDataType), 0, 0);
    } else {
      shiftsStepTwo_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsStepTwo->size() * sizeof(float), 0, 0);
      subbandedData_size = observation.getNrBeams() * observation.getNrDMsSubbanding() * observation.getNrSubbands() * observation.getNrSamplesPerPaddedBatchSubbanding(padding / sizeof(outputDataType));
      subbandedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, subbandedData_size * sizeof(outputDataType), 0, 0);
      dedispersedData_size = observation.getNrSynthesizedBeams() * observation.getNrDMsSubbanding() * observation.getNrDMs() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(outputDataType));
      dedispersedData_d = cl::Buffer(clContext, CL_MEM_WRITE_ONLY, dedispersedData_size * sizeof(outputDataType), 0, 0);
      beamDriverStepTwo_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamDriverStepTwo.size() * sizeof(uint8_t), 0, 0);
    }
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error allocating memory: " << std::to_string(err.err()) << "." << std::endl;
    return 1;
  }

  std::cout << std::fixed << std::endl;
  std::cout << "# nrBeams nrSynthesizedBeams nrSubbandingDMs nrDMs nrSubbands nrChannels nrZappedChannels nrSamplesSubbanding nrSamples *configuration* GFLOP/s time stdDeviation COV" << std::endl << std::endl;

  for ( unsigned int threads = minThreads; threads <= maxColumns; threads *= 2 ) {
    conf.setNrThreadsD0(threads);
    for ( unsigned int threads = 1; threads <= maxRows; threads++ ) {
      conf.setNrThreadsD1(threads);
      if ( conf.getNrThreadsD0() * conf.getNrThreadsD1() > maxThreads ) {
        break;
      } else if ( (conf.getNrThreadsD0() * conf.getNrThreadsD1()) % vectorWidth != 0 ) {
        continue;
      }
      for ( unsigned int items = 1; items <= maxSampleItems; items++ ) {
        conf.setNrItemsD0(items);
        if ( singleStep ) {
          if ( (observation.getNrSamplesPerBatch() % conf.getNrItemsD0()) != 0 ) {
            continue;
          }
        } else if ( stepOne ) {
          if ( (observation.getNrSamplesPerBatchSubbanding() % conf.getNrItemsD0()) != 0 ) {
            continue;
          }
        } else {
          if ( (observation.getNrSamplesPerBatch() % conf.getNrItemsD0()) != 0 ) {
            continue;
          }
        }
        for ( unsigned int items = 1; items <= maxDMItems; items++ ) {
          conf.setNrItemsD1(items);
          if ( singleStep ) {
            if ( (observation.getNrDMs() % (conf.getNrThreadsD1() * conf.getNrItemsD1())) != 0 ) {
              continue;
            }
          } else if ( stepOne ) {
            if ( (observation.getNrDMsSubbanding() % (conf.getNrThreadsD1() * conf.getNrItemsD1())) != 0 ) {
              continue;
            }
          } else {
            if ( (observation.getNrDMs() % (conf.getNrThreadsD1() * conf.getNrItemsD1())) != 0 ) {
              continue;
            }
          }
          unsigned int nrItems = conf.getNrItemsD1() + (conf.getNrItemsD0() * conf.getNrItemsD1());
          if ( singleStep ) {
            if ( conf.getLocalMem() ) {
              nrItems += 9;
            } else {
              nrItems += 4;
            }
            if ( inputBits < 8 ) {
              nrItems += 4;
            }
          } else if ( stepOne ) {
            if ( conf.getLocalMem() ) {
              nrItems += 10;
            } else {
              nrItems += 5;
            }
            if ( inputBits < 8 ) {
              nrItems += 4;
            }
          } else {
            if ( conf.getLocalMem() ) {
              nrItems += 10;
            } else {
              nrItems += 5;
            }
          }

          if ( nrItems > maxItems ) {
            break;
          }
          for ( unsigned int unroll = 1; unroll <= maxUnroll; unroll++ ) {
            conf.setUnroll(unroll);
            if ( singleStep ) {
              if ( observation.getNrChannels() % conf.getUnroll() != 0 ) {
                continue;
              }
            } else if ( stepOne ) {
              if ( observation.getNrChannelsPerSubband() % conf.getUnroll() != 0 ) {
                continue;
              }
            } else {
              if ( observation.getNrSubbands() % conf.getUnroll() != 0 ) {
                continue;
              }
            }

            // Generate kernel
            double gflops = 0.0;
            isa::utils::Timer timer;
            cl::Kernel * kernel;
            std::string * code = 0;

            if ( reInit ) {
              delete clQueues;
              clQueues = new std::vector< std::vector < cl::CommandQueue > >();
              isa::OpenCL::initializeOpenCL(clPlatformID, 1, clPlatforms, &clContext, clDevices, clQueues);
              try {
                if ( singleStep ) {
                  initializeDeviceMemorySingleStep(clContext, &(clQueues->at(clDeviceID)[0]), shiftsSingleStep, &shiftsSingleStep_d, zappedChannels, &zappedChannels_d, beamDriverSingleStep, &beamDriverSingleStep_d, &dispersedData_d, dispersedData_size, &dedispersedData_d, dedispersedData_size);
                } else if ( stepOne ) {
                  initializeDeviceMemoryStepOne(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepOne, &shiftsStepOne_d, zappedChannels, &zappedChannels_d, dispersedData_size, &dispersedData_d, subbandedData_size, &subbandedData_d);
                } else {
                  initializeDeviceMemoryStepTwo(clContext, &(clQueues->at(clDeviceID)[0]), shiftsStepTwo, &shiftsStepTwo_d, beamDriverStepTwo, &beamDriverStepTwo_d, subbandedData_size, &subbandedData_d, dedispersedData_size, &dedispersedData_d);
                }
              } catch ( cl::Error & err ) {
                std::cerr << "Error in memory allocation: ";
                std::cerr << std::to_string(err.err()) << "." << std::endl;
                return -1;
              }
              reInit = false;
            }
            if ( singleStep ) {
              code = PulsarSearch::getDedispersionOpenCL< inputDataType, outputDataType >(conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shiftsSingleStep);
              gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrSynthesizedBeams()) * observation.getNrDMs() * (observation.getNrChannels() - observation.getNrZappedChannels()) * observation.getNrSamplesPerBatch());
            } else if ( stepOne ) {
              code = PulsarSearch::getSubbandDedispersionStepOneOpenCL< inputDataType, outputDataType >(conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shiftsStepOne);
              gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrBeams()) * observation.getNrDMsSubbanding() * (observation.getNrChannels() - observation.getNrZappedChannels()) * observation.getNrSamplesPerBatchSubbanding());
            } else {
              code = PulsarSearch::getSubbandDedispersionStepTwoOpenCL< outputDataType >(conf, padding, outputDataName, observation, *shiftsStepTwo);
              gflops = isa::utils::giga(static_cast< uint64_t >(observation.getNrSynthesizedBeams()) * observation.getNrDMsSubbanding() * observation.getNrDMs() * observation.getNrSubbands() * observation.getNrSamplesPerBatch());
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
              break;
            }
            delete code;

            cl::NDRange global;
            cl::NDRange local;

            if ( singleStep ) {
              global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch() / conf.getNrItemsD0(), conf.getNrThreadsD0()), observation.getNrDMs() / conf.getNrItemsD1(), observation.getNrSynthesizedBeams());
              local = cl::NDRange(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
            } else if ( stepOne ) {
              global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatchSubbanding() / conf.getNrItemsD0(), conf.getNrThreadsD0()), observation.getNrDMsSubbanding() / conf.getNrItemsD1(), observation.getNrBeams() * observation.getNrSubbands());
              local = cl::NDRange(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
            } else {
              global = cl::NDRange(isa::utils::pad(observation.getNrSamplesPerBatch() / conf.getNrItemsD0(), conf.getNrThreadsD0()), observation.getNrDMs() / conf.getNrItemsD1(), observation.getNrSynthesizedBeams() * observation.getNrDMsSubbanding());
              local = cl::NDRange(conf.getNrThreadsD0(), conf.getNrThreadsD1(), 1);
            }

            if ( singleStep ) {
              kernel->setArg(0, dispersedData_d);
              kernel->setArg(1, dedispersedData_d);
              kernel->setArg(2, beamDriverSingleStep_d);
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
              kernel->setArg(2, beamDriverStepTwo_d);
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
              std::cerr << conf.print() << "): ";
              std::cerr << std::to_string(err.err()) << "." << std::endl;
              delete kernel;
              if ( err.err() == -4 || err.err() == -61 ) {
                return -1;
              }
              reInit = true;
              break;
            }
            delete kernel;

            std::cout << observation.getNrBeams() << " " << observation.getNrSynthesizedBeams() << " ";
            std::cout << observation.getNrDMsSubbanding() << " " << observation.getNrDMs() << " ";
            std::cout << observation.getNrSubbands() << " " << observation.getNrChannels() << " " << observation.getNrZappedChannels() << " ";
            std::cout << observation.getNrSamplesPerBatchSubbanding() << " " << observation.getNrSamplesPerBatch() << " ";
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

void initializeDeviceMemorySingleStep(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsSingleStep, cl::Buffer * shiftsSingleStep_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, std::vector< uint8_t > & beamDriverSingleStep, cl::Buffer * beamDriverSingleStep_d, cl::Buffer * dispersedData_d, const unsigned int dispersedData_size, cl::Buffer * dedispersedData_d, const unsigned int dedispersedData_size) {
  try {
    *shiftsSingleStep_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsSingleStep->size() * sizeof(float), 0, 0);
    *zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(uint8_t), 0, 0);
    *beamDriverSingleStep_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamDriverSingleStep.size() * sizeof(uint8_t), 0, 0);
    *dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
    *dedispersedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, dedispersedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shiftsSingleStep_d, CL_FALSE, 0, shiftsSingleStep->size() * sizeof(float), reinterpret_cast< void * >(shiftsSingleStep->data()));
    clQueue->enqueueWriteBuffer(*zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(uint8_t), reinterpret_cast< void * >(zappedChannels.data()));
    clQueue->enqueueWriteBuffer(*beamDriverSingleStep_d, CL_FALSE, 0, beamDriverSingleStep.size() * sizeof(uint8_t), reinterpret_cast< void * >(beamDriverSingleStep.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << std::to_string(err.err()) << "." << std::endl;
    throw;
  }
}

void initializeDeviceMemoryStepOne(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepOne, cl::Buffer * shiftsStepOne_d, std::vector< uint8_t > & zappedChannels, cl::Buffer * zappedChannels_d, const unsigned int dispersedData_size, cl::Buffer * dispersedData_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d) {
  try {
    *shiftsStepOne_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsStepOne->size() * sizeof(float), 0, 0);
    *zappedChannels_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, zappedChannels.size() * sizeof(uint8_t), 0, 0);
    *dispersedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, dispersedData_size * sizeof(inputDataType), 0, 0);
    *subbandedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, subbandedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shiftsStepOne_d, CL_FALSE, 0, shiftsStepOne->size() * sizeof(float), reinterpret_cast< void * >(shiftsStepOne->data()));
    clQueue->enqueueWriteBuffer(*zappedChannels_d, CL_FALSE, 0, zappedChannels.size() * sizeof(uint8_t), reinterpret_cast< void * >(zappedChannels.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << std::to_string(err.err()) << "." << std::endl;
    throw;
  }
}

void initializeDeviceMemoryStepTwo(cl::Context & clContext, cl::CommandQueue * clQueue, std::vector< float > * shiftsStepTwo, cl::Buffer * shiftsStepTwo_d, std::vector< uint8_t > & beamDriver, cl::Buffer * beamDriver_d, const unsigned int subbandedData_size, cl::Buffer * subbandedData_d, const unsigned int dedispersedData_size, cl::Buffer * dedispersedData_d) {
  try {
    *shiftsStepTwo_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, shiftsStepTwo->size() * sizeof(float), 0, 0);
    *beamDriver_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, beamDriver.size() * sizeof(uint8_t), 0, 0);
    *subbandedData_d = cl::Buffer(clContext, CL_MEM_READ_ONLY, subbandedData_size * sizeof(outputDataType), 0, 0);
    *dedispersedData_d = cl::Buffer(clContext, CL_MEM_READ_WRITE, dedispersedData_size * sizeof(outputDataType), 0, 0);
    clQueue->enqueueWriteBuffer(*shiftsStepTwo_d, CL_FALSE, 0, shiftsStepTwo->size() * sizeof(float), reinterpret_cast< void * >(shiftsStepTwo->data()));
    clQueue->enqueueWriteBuffer(*beamDriver_d, CL_FALSE, 0, beamDriver.size() * sizeof(uint8_t), reinterpret_cast< void * >(beamDriver.data()));
    clQueue->finish();
  } catch ( cl::Error & err ) {
    std::cerr << "OpenCL error: " << std::to_string(err.err()) << "." << std::endl;
    throw;
  }
}

