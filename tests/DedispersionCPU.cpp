/*
 * Copyright (C) 2014
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

#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <ctime>
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::exception;
using std::ofstream;
using std::fixed;
using std::setprecision;
using std::numeric_limits;

#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <ShiftsCPU.hpp>
#include <DedispersionCPU.hpp>
#include <DedispersionAVX.hpp>
#include <DedispersionPhi.hpp>
#include <Timer.hpp>
#include <utils.hpp>
using isa::utils::ArgumentList;
using isa::utils::Timer;
using isa::utils::giga;
using isa::utils::same;
using AstroData::Observation;
using PulsarSearch::getShiftsCPU;
using PulsarSearch::dedispersion;
using PulsarSearch::dedispersionAVX;
using PulsarSearch::dedispersionPhi;

typedef float dataType;
const string typeName("float");
const unsigned int padding = 16;

// Common parameters
const unsigned int nrBeams = 1;
const unsigned int nrStations = 64;
// LOFAR
/*const float minFreq = 138.965f;
const float channelBandwidth = 0.195f;
const unsigned int nrSamplesPerSecond = 200000;
const unsigned int nrChannels = 32;*/
// Apertif
const float minFreq = 1425.0f;
const float channelBandwidth = 0.2929f;
const unsigned int nrSamplesPerSecond = 20000;
const unsigned int nrChannels = 1024;
// DMs
const float firstDM = 0.0f;
const float DMStep = 0.25f;


int main(int argc, char * argv[]) {
	unsigned int nrSamplesPerChannel = 0;
	long long unsigned int wrongOnes = 0;
	Observation< dataType > observation("DedispersionTest", typeName);
	unsigned int * shifts = 0;
	dataType * dispersedData = 0;
	dataType * dedispersedData = 0;
	dataType * dedispersedDataPar = 0;

	observation.setPadding(padding);
	try {
		ArgumentList args(argc, argv);

		observation.setNrDMs(args.getSwitchArgument< unsigned int >("-dms"));
	} catch ( exception & err ) {
		cerr << err.what() << endl;
		return 1;
	}

	// Setup of the observation
	observation.setNrBeams(nrStations);
	observation.setNrStations(nrBeams);
	observation.setMinFreq(minFreq);
	observation.setMaxFreq(minFreq + (channelBandwidth * (nrChannels - 1)));
	observation.setChannelBandwidth(channelBandwidth);
	observation.setNrSamplesPerSecond(nrSamplesPerSecond);
	observation.setNrChannels(nrChannels);
	observation.setFirstDM(firstDM);
	observation.setDMStep(DMStep);
	
	shifts = getShiftsCPU(observation);

	if ( ((observation.getNrSamplesPerSecond() + shifts[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % padding) != 0 ) {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + shifts[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) + (padding - ((observation.getNrSamplesPerSecond() + shifts[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % padding));
	} else {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + shifts[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
	}

	// Allocate memory
	dispersedData = new dataType [observation.getNrChannels() * nrSamplesPerChannel];
	dedispersedData = new dataType [observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond()];
	dedispersedDataPar = new dataType [observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond()];

	srand(time(NULL));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < nrSamplesPerChannel; sample++ ) {
			dispersedData[(channel * nrSamplesPerChannel) + sample] = rand() % 1000;
		}
	}

	dedispersion(nrSamplesPerChannel, observation, dispersedData, dedispersedData, shifts);
	dedispersionAVX(nrSamplesPerChannel, observation.getNrDMs(), observation.getNrSamplesPerSecond(), observation.getNrChannels(), observation.getNrSamplesPerPaddedSecond(), dispersedData, dedispersedDataPar, shifts);
	//dedispersionPhi(nrSamplesPerChannel, observation.getNrDMs(), observation.getNrSamplesPerSecond(), observation.getNrChannels(), observation.getNrSamplesPerPaddedSecond(), dispersedData, dedispersedDataPar, shifts);

	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
			if ( !same(dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample], dedispersedDataPar[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]) ) {
				wrongOnes++;

				cout << dm << " " << sample << " " << dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " " << dedispersedDataPar[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " " << dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] - dedispersedDataPar[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << endl;
			}
		}
	}

	cout << endl;

	cout << "Wrong samples: " << wrongOnes << " (" << (wrongOnes * 100) / (static_cast< long long unsigned int >(observation.getNrDMs()) * observation.getNrSamplesPerSecond()) << "%)." << endl;
	cout << endl;
	
	return 0;
}
