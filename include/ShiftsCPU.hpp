/*
 * Copyright (C) 2013
 * Alessio Sclocco <a.sclocco@vu.nl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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

namespace TDM {

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
