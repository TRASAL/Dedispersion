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


int main(int argc, char *argv[]) {
  unsigned int padding = 0;
  AstroData::Observation observation;

  try {
    isa::utils::ArgumentList args(argc, argv);
    padding = args.getSwitchArgument< unsigned int >("-padding");
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
    observation.setNrSamplesPerBatch(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch  ( isa::utils::SwitchNotFound &err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception &err ) {
    std::cerr << "Usage: " << argv[0] << " -padding ... -min_freq ... -channel_bandwidth ... -subbands ... -channels ... -samples ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	}

  std::vector< float > * shifts = PulsarSearch::getShifts(observation, padding);

  for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
    std::cout << shifts->at(channel) << " ";
  }
  std::cout << std::endl;

	return 0;
}

