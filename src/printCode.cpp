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
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>


int main(int argc, char *argv[]) {
  uint8_t inputBits = 0;
  std::string inputTypeName;
  std::string intermediateTypeName;
  std::string outputTypeName;
  PulsarSearch::DedispersionConf conf;
  AstroData::Observation observation;

  try {
    isa::utils::ArgumentList args(argc, argv);
    observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
    conf.setLocalMem(args.getSwitch("-local"));
    conf.setNrSamplesPerBlock(args.getSwitchArgument< unsigned int >("-sb"));
		conf.setNrDMsPerBlock(args.getSwitchArgument< unsigned int >("-db"));
		conf.setNrSamplesPerThread(args.getSwitchArgument< unsigned int >("-st"));
		conf.setNrDMsPerThread(args.getSwitchArgument< unsigned int >("-dt"));
    conf.setUnroll(args.getSwitchArgument< unsigned int >("-unroll"));
    inputBits = args.getSwitchArgument< uint8_t >("-input_bits");
    inputTypeName = args.getSwitchArgument< std::string >("-input_type");
    intermediateTypeName = args.getSwitchArgument< std::string >("-intermediate_type");
    outputTypeName = args.getSwitchArgument< std::string >("-output_type");
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch  ( isa::utils::SwitchNotFound & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception & err ) {
    std::cerr << "Usage: " << argv[0] << " -padding ... [-local] -sb ... -db ... -st ... -dt ... -unroll ... -input_bits ... -input_type ... -intermediate_type ... -output_type ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	}

  std::vector< float > * shifts = PulsarSearch::getShifts(observation);
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));

	// Generate kernel
  std::string * code = PulsarSearch::getDedispersionOpenCL(conf, inputBits, inputTypeName, intermediateTypeName, outputTypeName, observation, *shifts);
  std::cout << *code << std::endl;

	return 0;
}

