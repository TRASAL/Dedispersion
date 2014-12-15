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
  bool localMem = false;
	unsigned int nrSamplesPerBlock = 0;
	unsigned int nrDMsPerBlock = 0;
  unsigned int nrSamplesPerThread = 0;
  unsigned int nrDMsPerThread = 0;
  unsigned int unroll = 0;
  std::string typeName;
  AstroData::Observation observation;

  try {
    isa::utils::ArgumentList args(argc, argv);
    observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
    localMem = args.getSwitch("-local");
		nrSamplesPerBlock = args.getSwitchArgument< unsigned int >("-sb");
		nrDMsPerBlock = args.getSwitchArgument< unsigned int >("-db");
		nrSamplesPerThread = args.getSwitchArgument< unsigned int >("-st");
		nrDMsPerThread = args.getSwitchArgument< unsigned int >("-dt");
    unroll = args.getSwitchArgument< unsigned int >("-unroll");
    typeName = args.getSwitchArgument< std::string >("-type");
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch  ( isa::utils::SwitchNotFound &err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception &err ) {
    std::cerr << "Usage: " << argv[0] << " -padding ... [-local] -sb ... -db ... -st ... -dt ... -unroll ... -type ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	}

  std::vector< float > * shifts = PulsarSearch::getShifts(observation);
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));

	// Generate kernel
  std::string * code = PulsarSearch::getDedispersionOpenCL(localMem, nrSamplesPerBlock, nrDMsPerBlock, nrSamplesPerThread, nrDMsPerThread, unroll, typeName, observation, *shifts);
  std::cout << *code << std::endl;

	return 0;
}

