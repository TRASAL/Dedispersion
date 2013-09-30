/*
 * Copyright (C) 2012
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
#include <ReadData.hpp>
#include <InitializeOpenCL.hpp>
#include <CLData.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
#include <DedispersionCPU.hpp>
using isa::utils::ArgumentList;
using AstroData::Observation;
using AstroData::readLOFAR;
using isa::OpenCL::initializeOpenCL;
using isa::OpenCL::CLData;
using isa::utils::same;
using TDM::getShifts;
using TDM::Dedispersion;
using TDM::dedispersion;

typedef float dataType;
const string typeName("float");

// Common parameters
const unsigned int nrBeams = 1;
const unsigned int nrStations = 64;
const unsigned int paddingCL = 32;
const unsigned int paddingCPU = 8;
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
const unsigned int nrDMs = 256;
const float firstDM = 0.0f;
const float DMStep = 0.25f;


int main(int argc, char *argv[]) {
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	unsigned int nrSamplesPerBlock = 0;
	unsigned int nrDMsPerBlock = 0;
	unsigned int nrSamplesPerThread = 0;
	unsigned int nrDMsPerThread = 0;
	unsigned int nrSamplesPerChannel = 0;
	unsigned int secondsToBuffer = 0;
	long long unsigned int wrongOnes = 0;
	Observation< dataType > observation("DedispersionTest", typeName);
	Observation< dataType > observationCPU("DedispersionTest", typeName);
	CLData< unsigned int > * shifts = 0;
	CLData< dataType > * dispersedData = new CLData< dataType >("DispersedData", true);
	CLData< dataType > * dedispersedData = new CLData< dataType >("DedispersedData", true);
	CLData< dataType > * clDedispersedData = new CLData< dataType >("clDedispersedData", true);

	try {
		ArgumentList args(argc, argv);

		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");

		nrSamplesPerBlock = args.getSwitchArgument< unsigned int >("-sb");
		nrDMsPerBlock = args.getSwitchArgument< unsigned int >("-db");
		nrSamplesPerThread = args.getSwitchArgument< unsigned int >("-st");
		nrDMsPerThread = args.getSwitchArgument< unsigned int >("-dt");
	} catch ( exception &err ) {
		cerr << err.what() << endl;
		return 1;
	}

	// Setup of the observation
	observation.setPadding(paddingCL);
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
	observationCPU = observation;
	observationCPU.setPadding(paddingCPU);
	
	// Test
	cl::Context *clContext = new cl::Context();
	vector< cl::Platform > *clPlatforms = new vector< cl::Platform >();
	vector< cl::Device > *clDevices = new vector< cl::Device >();
	vector< vector< cl::CommandQueue > > *clQueues = new vector< vector < cl::CommandQueue > >();

	initializeOpenCL(clPlatformID, 1, clPlatforms, clContext, clDevices, clQueues);
	shifts = getShifts(observation);

	if ( ((observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % padding) != 0 ) {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) + (padding - ((observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % padding));
	} else {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
	}
	secondsToBuffer = static_cast< unsigned int >(ceil(static_cast< float >(nrSamplesPerChannel) / observation.getNrSamplesPerPaddedSecond()));
		
	// Allocate memory
	dispersedData->allocateHostData(secondsToBuffer * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond());
	dedispersedData->allocateHostData(observationCPU.getNrDMs() * observationCPU.getNrSamplesPerPaddedSecond());
	clDedispersedData->allocateHostData(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());

	srand(time(NULL));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < (secondsToBuffer * observation.getNrSamplesPerSecond()); sample++ ) {
			dispersedData->setHostDataItem((channel * (secondsToBuffer * observation.getNrSamplesPerPaddedSecond())) + sample, static_cast< dataType >(rand() % 10));
		}
	}
			
	try {
		shifts->setCLContext(clContext);
		shifts->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
		shifts->allocateDeviceData();
		shifts->copyHostToDevice();

		dispersedData->setCLContext(clContext);
		dispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
		dispersedData->setDeviceReadOnly();
		dispersedData->allocateDeviceData();
		dispersedData->copyHostToDevice();

		clDedispersedData->setCLContext(clContext);
		clDedispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
		clDedispersedData->allocateDeviceData();
	} catch ( OpenCLError err ) {
		cerr << err.what() << endl;
		return 1;
	}
	
	// Generate kernel
	try {
		Dedispersion< dataType > clDedisperse("Dedisperse", typeName);
		clDedisperse.bindOpenCL(clContext, &(clDevices->at(clDeviceID)), &((clQueues->at(clDeviceID)).at(0)));
		clDedisperse.setNrSamplesPerBlock(nrSamplesPerBlock);
		clDedisperse.setNrDMsPerBlock(nrDMsPerBlock);
		clDedisperse.setNrSamplesPerThread(nrSamplesPerThread);
		clDedisperse.setNrDMsPerThread(nrDMsPerThread);
		clDedisperse.setNrSamplesPerDispersedChannel(secondsToBuffer * observation.getNrSamplesPerPaddedSecond());
		clDedisperse.setObservation(&observation);
		clDedisperse.setShifts(shifts);
		clDedisperse.generateCode();

		cout << clDedisperse.getCode() << endl;
		cout << endl;

		clDedisperse(dispersedData, clDedispersedData);
		clDedispersedData->copyDeviceToHost();
	} catch ( OpenCLError err ) {
		cerr << err.what() << endl;
		return 1;
	}

	dedispersion(secondsToBuffer * observationCPU.getNrSamplesPerPaddedSecond(), observationCPU, dispersedData->getHostData(), dedispersedData->getHostData(), shifts->getHostData());

	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
			if ( !same((*dedispersedData)[(dm * observationCPU.getNrSamplesPerPaddedSecond()) + sample], (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]) ) {
				wrongOnes++;

				cout << dm << " " << sample << " " << (*dedispersedData)[(dm * observationCPU.getNrSamplesPerPaddedSecond()) + sample] << " " << (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " " << (*dedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] - (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << endl;
			}
		}
	}

	cout << endl;

	cout << "Wrong samples: " << wrongOnes << " (" << (wrongOnes * 100) / (static_cast< long long unsigned int >(observation.getNrSeconds()) * observation.getNrDMs() * observation.getNrSamplesPerSecond()) << "%)." << endl;
	cout << endl;
	
	return 0;
}

