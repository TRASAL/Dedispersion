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
using TDM::getShifts;
using TDM::Dedispersion;

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

