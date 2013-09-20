/*
 * Copyright (C) 2011
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

#include <CLData.hpp>
#include <Observation.hpp>
using isa::OpenCL::CLData;
using AstroData::Observation;


#ifndef SHIFTS_HPP
#define SHIFTS_HPP

namespace TDM {

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
} // TDM

#endif // SHIFTS_HPP

