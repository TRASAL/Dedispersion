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
#include <Shifts.hpp>
#include <Dedispersion.hpp>


int main(int argc, char *argv[]) {
  unsigned int padding = 0;
  uint8_t inputBits = 0;
  std::string channelsFile;
  PulsarSearch::DedispersionConf conf;
  AstroData::Observation observation;

  try {
    isa::utils::ArgumentList args(argc, argv);
    padding = args.getSwitchArgument< unsigned int >("-padding");
    channelsFile = args.getSwitchArgument< std::string >("-zapped_channels");
    conf.setSplitSeconds(args.getSwitch("-split_seconds"));
    conf.setLocalMem(args.getSwitch("-local"));
    conf.setNrThreadsD0(args.getSwitchArgument< unsigned int >("-threads0"));
		conf.setNrThreadsD1(args.getSwitchArgument< unsigned int >("-threads1"));
		conf.setNrItemsD0(args.getSwitchArgument< unsigned int >("-items0"));
		conf.setNrItemsD1(args.getSwitchArgument< unsigned int >("-items1"));
    conf.setUnroll(args.getSwitchArgument< unsigned int >("-unroll"));
    inputBits = args.getSwitchArgument< unsigned int >("-input_bits");
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerBatch(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch  ( isa::utils::SwitchNotFound & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception & err ) {
    std::cerr << "Usage: " << argv[0] << " -padding ... -zapped_channels ... [-split_seconds] [-local] -threads0 ... -threads1 ... -items0 ... -items1 ... -unroll ... -input_bits ... -min_freq ... -channel_bandwidth ... -samples ... -subbandsd ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	}

  std::vector< float > * shifts = PulsarSearch::getShifts(observation, padding);
  std::vector< uint8_t > zappedChannels(observation.getNrPaddedChannels(padding / sizeof(uint8_t)));
  AstroData::readZappedChannels(observation, channelsFile, zappedChannels);
  if ( conf.getSplitSeconds() ) {
    if ( (observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) % observation.getNrSamplesPerBatch() == 0 ) {
      observation.setNrDelaySeconds((observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerBatch());
    } else {
      observation.setNrDelaySeconds(((observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep())))) / observation.getNrSamplesPerBatch()) + 1);
    }
  }
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));

	// Generate kernel
  std::string * code = PulsarSearch::getDedispersionOpenCL< inputDataType, outputDataType >(conf, padding, inputBits, inputDataName, intermediateDataName, outputDataName, observation, *shifts, zappedChannels);
  std::cout << *code << std::endl;

	return 0;
}

