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

#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cmath>
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
#include <CLData.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
using isa::utils::ArgumentList;
using isa::utils::giga;
using AstroData::Observation;
using isa::OpenCL::CLData;
using PulsarSearch::getShifts;
using PulsarSearch::Dedispersion;

typedef float dataType;
const string typeName("float");

// Common parameters
const unsigned int nrBeams = 1;
const unsigned int nrStations = 64;
const unsigned int padding = 32;
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
	unsigned int nrDMs = 0;
	unsigned int nrSamplesPerBlock = 0;
	unsigned int nrSamplesPerThread = 0;
	unsigned int nrDMsPerBlock = 0;
	unsigned int nrDMsPerThread = 0;
	Observation< dataType > observation("DedispersionTuning", typeName);
	CLData< unsigned int > * shifts = 0;

	try {
		ArgumentList args(argc, argv);

		nrDMs = args.getSwitchArgument< unsigned int >("-dm");
		nrSamplesPerBlock = args.getSwitchArgument< unsigned int >("-sb");
		nrSamplesPerThread = args.getSwitchArgument< unsigned int >("-st");
		nrDMsPerBlock = args.getSwitchArgument< unsigned int >("-db");
		nrDMsPerThread = args.getSwitchArgument< unsigned int >("-dt");

	} catch ( exception &err ) {
		cerr << err.what() << endl;
		return 1;
	}

	// Setup of the observation
	observation.setPadding(padding);
	observation.setNrBeams(nrStations);
	observation.setNrStations(nrBeams);
	observation.setMinFreq(minFreq);
	observation.setMaxFreq(minFreq + (channelBandwidth * (nrChannels - 1)));
	observation.setChannelBandwidth(channelBandwidth);
	observation.setNrSamplesPerSecond(nrSamplesPerSecond);
	observation.setNrChannels(nrChannels);
	observation.setNrDMs(nrDMs);
	observation.setFirstDM(firstDM);
	observation.setDMStep(DMStep);

	long long unsigned int mem = 0;
	unsigned int nrBlocks = (observation.getNrSamplesPerSecond() / (nrSamplesPerBlock * nrSamplesPerThread)) * (observation.getNrDMs() / (nrDMsPerBlock * nrDMsPerThread));

	delete shifts;
	shifts = getShifts(observation);

	// Writes
	mem = observation.getNrSamplesPerSecond() * observation.getNrDMs() * 4;
	// Reads
	mem += static_cast< long long unsigned int >(((nrSamplesPerBlock * nrSamplesPerThread) + ((*shifts)[(observation.getNrDMs() - 1) * observation.getNrPaddedChannels()] - (*shifts)[(observation.getNrDMs() - (nrDMsPerBlock * nrDMsPerThread)) * observation.getNrPaddedChannels()]) + 1)) * observation.getNrChannels() * 4 * nrBlocks;

	cout << nrDMs << " " << giga(mem) << endl;

	cout << endl;

	return 0;
}

