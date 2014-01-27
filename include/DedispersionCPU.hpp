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

#include <Observation.hpp>
using AstroData::Observation;


#ifndef DEDISPERSION_CPU_HPP
#define DEDISPERSION_CPU_HPP

namespace PulsarSearch {

// OpenMP + SIMD dedispersion algorithm
template< typename T > void dedispersion(const unsigned int nrSamplesPerChannel, Observation< T > & observation, const T  * const __restrict__ input, T * const __restrict__ output, unsigned int * const __restrict__ shifts);
template< typename T > void dedispersionAVX(const unsigned int nrSamplesPerChannel, Observation< T > & observation, const T  * const __restrict__ input, T * const __restrict__ output, const unsigned int * const __restrict__ shifts);
template< typename T > void dedispersionPhi(const unsigned int nrSamplesPerChannel, const unsigned int nrDMs, const unsigned int nrSamplesPerSecond, const unsigned int nrChannels, const unsigned int nrSamplesPerPaddedSecond, const T  * const __restrict__ input, T * const __restrict__ output, const unsigned int * const __restrict__ shifts);


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

template< typename T > void dedispersionAVX(const unsigned int nrSamplesPerChannel, Observation< T > & observation, const T * const __restrict__ input, T * const __restrict__ output, unsigned int * const __restrict__ shifts) {
	#pragma omp parallel for schedule(static)
	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		#pragma omp parallel for schedule(static)
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample += 8 ) {
			__m256 dedispersedSample = _mm256_setzero_ps();

			for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
				unsigned int shift = shifts[(dm * observation.getNrChannels()) + channel];
				__m256 dispersedSample = _mm256_loadu_ps(&(input[(channel * nrSamplesPerChannel) + (sample + shift)]));
				
				dedispersedSample = _mm256_add_ps(dedispersedSample, dispersedSample);
			}

			_mm256_store_ps(&(output[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]), dedispersedSample);
		}
	}
}

template< typename T > void dedispersionPhi(const unsigned int nrSamplesPerChannel, const unsigned int nrDMs, const unsigned int nrSamplesPerSecond, const unsigned int nrChannels, const unsigned int nrSamplesPerPaddedSecond, const T  * const __restrict__ input, T * const __restrict__ output, const unsigned int * const __restrict__ shifts) {
	#pragma omp parallel for schedule(static)
	for ( unsigned int dm = 0; dm < nrDMs; dm++ ) {
		#pragma omp parallel for schedule(static)
		for ( unsigned int sample = 0; sample < nrSamplesPerSecond; sample += 16 ) {
			__m512 dedispersedSample = _mm512_setzero_ps();

			for ( unsigned int channel = 0; channel < nrChannels; channel++ ) {
				unsigned int shift = shifts[(dm * nrChannels) + channel];
				__m512 dispersedSample;

				dispersedSample = _mm512_loadunpacklo_ps(dispersedSample, &(input[(channel * nrSamplesPerChannel) + (sample + shift)]));
				dispersedSample = _mm512_loadunpackhi_ps(dispersedSample, &(input[(channel * nrSamplesPerChannel) + (sample + shift)]) + 16);
				dedispersedSample = _mm512_add_ps(dedispersedSample, dispersedSample);
			}

			_mm512_packstorelo_ps(&(output[(dm * nrSamplesPerPaddedSecond) + sample]), dedispersedSample);
			_mm512_packstorehi_ps(&(output[(dm * nrSamplesPerPaddedSecond) + sample]) + 16, dedispersedSample);
		}
	}
}

} // PulsarSearch

#endif // DEDISPERSION_CPU_HPP