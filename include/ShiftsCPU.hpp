// Copyright 2013 Alessio Sclocco <a.sclocco@vu.nl>
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

#include <string>
#include <vector>
#include <cmath>
#include <x86intrin.h>
using std::string;
using std::vector;
using std::make_pair;
using std::pow;
using std::ceil;

#include <Observation.hpp>
using AstroData::Observation;


#ifndef SHIFTS_CPU_HPP
#define SHIFTS_CPU_HPP

namespace PulsarSearch {

template< typename T > unsigned int * getShiftsCPU(Observation< T > & observation);


// Implementation
template< typename T > unsigned int * getShiftsCPU(Observation< T > & observation) {
	float inverseHighFreq = 1.0f / (observation.getMaxFreq() * observation.getMaxFreq());
	unsigned int * shifts = new unsigned int [observation.getNrDMs() * observation.getNrPaddedChannels()];

	#pragma omp parallel for
	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		float kDM = 4148.808f * (observation.getFirstDM() + (dm * observation.getDMStep()));

		#pragma omp parallel for
		for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
			float inverseFreq = 1.0f / ((observation.getMinFreq() + (channel * observation.getChannelBandwidth())) * (observation.getMinFreq() + (channel * observation.getChannelBandwidth())));
			float delta = kDM * (inverseFreq - inverseHighFreq);

			shifts[(dm * observation.getNrPaddedChannels()) + channel] = static_cast< unsigned int >(delta * observation.getNrSamplesPerSecond());
		}
	}

	return shifts;
}

}

#endif // SHIFTS_CPU_HPP
