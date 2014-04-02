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

#include <Observation.hpp>
using AstroData::Observation;


#ifndef DEDISPERSION_CPU_HPP
#define DEDISPERSION_CPU_HPP

namespace PulsarSearch {

// Sequential dedispersion
template< typename T > void dedispersion(const unsigned int nrSamplesPerChannel, Observation< T > & observation, const T  * const __restrict__ input, T * const __restrict__ output, unsigned int * const __restrict__ shifts);


// Implementation
template< typename T > void dedispersion(const unsigned int nrSamplesPerChannel, Observation< T > & observation, const T * const __restrict__ input, T * const __restrict__ output, unsigned int * const __restrict__ shifts) {
	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
			T dedispersedSample = static_cast< T >(0);

			for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
				unsigned int shift = shifts[(dm * observation.getNrChannels()) + channel];

				dedispersedSample += input[(channel * nrSamplesPerChannel) + (sample + shift)];
			}

			output[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] = dedispersedSample;
		}
	}
}

} // PulsarSearch

#endif // DEDISPERSION_CPU_HPP
