// Copyright 2011 Alessio Sclocco <a.sclocco@vu.nl>
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

#include <vector>
#include <cmath>

#include <Observation.hpp>


#ifndef SHIFTS_HPP
#define SHIFTS_HPP

namespace PulsarSearch {

std::vector< float > * getShifts(AstroData::Observation & observation);


// Implementation
std::vector< float > * getShifts(AstroData::Observation & observation) {
  float inverseHighFreq = 1.0f / std::pow(observation.getMaxFreq(), 2.0f);
  std::vector< float > * shifts = new std::vector< float >(observation.getNrPaddedChannels());

  for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
    float channelFreq = observation.getMinFreq() + (channel * observation.getChannelBandwidth());
    float inverseFreq = 1.0f / std::pow(channelFreq, 2.0f);

    shifts->at(channel) = 4148.808f * (inverseFreq - inverseHighFreq);
	}

	return shifts;
}
} // PulsarSearch

#endif // SHIFTS_HPP

