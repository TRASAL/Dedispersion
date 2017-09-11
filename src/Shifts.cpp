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

#include <Shifts.hpp>

namespace PulsarSearch {

std::vector< float > * getShifts(AstroData::Observation & observation, const unsigned int padding) {
  float inverseHighFreq = 1.0f / std::pow(observation.getMaxFreq(), 2.0f);
  std::vector< float > * shifts = new std::vector< float >(observation.getNrPaddedChannels(padding / sizeof(float)));

  for ( unsigned int channel = 0; channel < observation.getNrChannels() - 1; channel++ ) {
    float inverseFreq = 1.0f / std::pow(observation.getMinFreq() + (channel * observation.getChannelBandwidth()), 2.0f);

    shifts->at(channel) = 4148.808f * (inverseFreq - inverseHighFreq) * observation.getNrSamplesPerBatch();
	}
  shifts->at(observation.getNrChannels() - 1) = 0;

	return shifts;
}

std::vector< float > * getShiftsStepTwo(AstroData::Observation & observation, const unsigned int padding) {
  float inverseHighFreq = 1.0f / std::pow(observation.getSubbandMaxFreq(), 2.0f);
  std::vector< float > * shifts = new std::vector< float >(observation.getNrPaddedSubbands(padding / sizeof(float)));

  for ( unsigned int subband = 0; subband < observation.getNrSubbands() - 1; subband++ ) {
    float inverseFreq = 1.0f / std::pow(observation.getSubbandMinFreq() + (subband * observation.getSubbandBandwidth()), 2.0f);

    shifts->at(subband) = 4148.808f * (inverseFreq - inverseHighFreq) * observation.getNrSamplesPerBatch();
	}
  shifts->at(observation.getNrSubbands() - 1) = 0;

	return shifts;
}
} // PulsarSearch

