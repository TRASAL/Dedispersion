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
#include <omp.h>
using std::string;
using std::vector;
using std::make_pair;
using std::pow;
using std::ceil;


#ifndef DEDISPERSION_PHI_HPP
#define DEDISPERSION_PHI_HPP

namespace PulsarSearch {

// OpenMP + SIMD dedispersion algorithm
template< typename T > void dedispersion(const unsigned int nrSamplesPerChannel, const unsigned int nrDMs, const unsigned int nrSamplesPerSecond, const unsigned int nrSamplesPerPaddedSecond, const unsigned int nrChannels, const T  * const __restrict__ input, T * const __restrict__ output, unsigned int * const __restrict__ shifts);


// Implementation
template< typename T > void dedispersion(const unsigned int nrSamplesPerChannel, const unsigned int nrDMs, const unsigned int nrSamplesPerSecond, const unsigned int nrSamplesPerPaddedSecond, const unsigned int nrChannels, const T  * const __restrict__ input, T * const __restrict__ output, unsigned int * const __restrict__ shifts) {
	#pragma offload target(mic) nocopy(input: alloc_if(0) free_if(0)) nocopy(output: alloc_if(0) free_if(0)) nocopy(shifts: alloc_if(0) free_if(0))
	{
		#pragma omp parallel for
		for ( unsigned int dm = 0; dm < nrDMs; dm++ ) {
			#pragma omp parallel for
			for ( unsigned int sample = 0; sample < nrSamplesPerSecond; sample += 16 ) {
				__m512 dedispersedSample = _mm512_setzero_ps();
	
				for ( unsigned int channel = 0; channel < nrChannels; channel++ ) {
					unsigned int shift = shifts[(dm * nrChannels) + channel];
					__m512 dispersedSample;
					
					dispersedSample = _mm512_loadunpackhi_ps(_mm512_loadunpacklo_ps(dispersedSample, &(input[(channel * nrSamplesPerChannel) + (sample + shift)])), &(input[(channel * nrSamplesPerChannel) + (sample + shift)]) + 16);
					dedispersedSample = _mm512_add_ps(dedispersedSample, dispersedSample);
				}

				_mm512_store_ps(&(output[(dm * nrSamplesPerPaddedSecond) + sample]), dedispersedSample);
			}
		}
	}
}

} // PulsarSearch

#endif // DEDISPERSION_PHI_HPP
