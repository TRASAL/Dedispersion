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

#include <Observation.hpp>


#ifndef SHIFTS_HPP
#define SHIFTS_HPP

namespace PulsarSearch {

std::vector< unsigned int > * getShifts(AstroData::Observation & observation);


// Implementation
std::vector< unsigned int > * getShifts(AstroData::Observation & observation) {
	float inverseHighFreq = 1.0f / (observation.getMaxFreq() * observation.getMaxFreq());
  std::vector< unsigned int > * shifts = new std::vector< unsigned int >(observation.getNrChannels() * observation.getNrPaddedDMs());

  for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
    float inverseFreq = 1.0f / ((observation.getMinFreq() + (channel * observation.getChannelBandwidth())) * (observation.getMinFreq() + (channel * observation.getChannelBandwidth())));
    for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
      float kDM = 4148.808f * (observation.getFirstDM() + (dm * observation.getDMStep()));
			float delta = kDM * (inverseFreq - inverseHighFreq);

      shifts->at((channel * observation.getNrPaddedDMs()) + dm) = static_cast< unsigned int >(delta * observation.getNrSamplesPerSecond());
		}
	}

	return shifts;
}
} // PulsarSearch

#endif // SHIFTS_HPP

