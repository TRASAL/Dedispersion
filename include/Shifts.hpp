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

#include <CLData.hpp>
#include <Observation.hpp>
using isa::OpenCL::CLData;
using AstroData::Observation;


#ifndef SHIFTS_HPP
#define SHIFTS_HPP

namespace PulsarSearch {

template< typename T > CLData< unsigned int > * getShifts(Observation< T > & observation);


// Implementation
template< typename T > CLData< unsigned int > * getShifts(Observation< T > & observation) {
	float inverseHighFreq = 1.0f / (observation.getMaxFreq() * observation.getMaxFreq());
	CLData< unsigned int > * shifts = new CLData< unsigned int >("Shifts", true);

	shifts->allocateHostData(observation.getNrDMs() * observation.getNrPaddedChannels());
	shifts->setDeviceReadOnly();

	#pragma omp parallel for
	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		float kDM = 4148.808f * (observation.getFirstDM() + (dm * observation.getDMStep()));

		#pragma omp parallel for
		for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
			float inverseFreq = 1.0f / ((observation.getMinFreq() + (channel * observation.getChannelBandwidth())) * (observation.getMinFreq() + (channel * observation.getChannelBandwidth())));
			float delta = kDM * (inverseFreq - inverseHighFreq);

			*(shifts->getHostDataAt((dm * observation.getNrPaddedChannels()) + channel)) = static_cast< unsigned int >(delta * observation.getNrSamplesPerSecond());
		}
	}

	return shifts;
}
} // PulsarSearch

#endif // SHIFTS_HPP

