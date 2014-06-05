// Copyright 2012 Alessio Sclocco <a.sclocco@vu.nl>
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
#include <ShiftsCPU.hpp>
#include <Dedispersion.hpp>
#include <DedispersionCPU.hpp>
using isa::utils::ArgumentList;
using AstroData::Observation;
using AstroData::readLOFAR;
using isa::OpenCL::initializeOpenCL;
using isa::OpenCL::CLData;
using isa::utils::same;
using PulsarSearch::getShifts;
using PulsarSearch::getShiftsCPU;
using PulsarSearch::Dedispersion;
using PulsarSearch::dedispersion;

typedef float dataType;
const string typeName("float");

// Common parameters
const unsigned int nrSeconds = 1;
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
	CLData< unsigned int > * clShifts = 0;
	unsigned int * shifts = 0;
	CLData< dataType > * dispersedData = new CLData< dataType >("DispersedData", true);
	dataType * dedispersedData = 0;
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
	observation.setNrSeconds(nrSeconds);
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
	clShifts = getShifts(observation);
	shifts = getShiftsCPU(observationCPU);

	if ( ((observation.getNrSamplesPerSecond() + (*clShifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % paddingCL) != 0 ) {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*clShifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) + (paddingCL - ((observation.getNrSamplesPerSecond() + (*clShifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % paddingCL));
	} else {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*clShifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
	}
	secondsToBuffer = static_cast< unsigned int >(ceil(static_cast< float >(nrSamplesPerChannel) / observation.getNrSamplesPerPaddedSecond()));

	// Allocate memory
	dispersedData->allocateHostData(secondsToBuffer * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond());
	dedispersedData = new dataType [observationCPU.getNrDMs() * observationCPU.getNrSamplesPerPaddedSecond()];
	clDedispersedData->allocateHostData(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());

	srand(time(NULL));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < (secondsToBuffer * observation.getNrSamplesPerSecond()); sample++ ) {
			dispersedData->setHostDataItem((channel * (secondsToBuffer * observation.getNrSamplesPerPaddedSecond())) + sample, static_cast< dataType >(rand() % 10));
		}
	}

	try {
		clShifts->setCLContext(clContext);
		clShifts->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
		clShifts->allocateDeviceData();
		clShifts->copyHostToDevice();

		dispersedData->setCLContext(clContext);
		dispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
		dispersedData->setDeviceReadOnly();
		dispersedData->allocateDeviceData();
		dispersedData->copyHostToDevice();

		clDedispersedData->setCLContext(clContext);
		clDedispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
		clDedispersedData->allocateDeviceData();
	} catch ( OpenCLError &err ) {
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
		clDedisperse.setShifts(clShifts);
		clDedisperse.generateCode();

		cout << clDedisperse.getCode() << endl;
		cout << endl;

		clDedisperse(dispersedData, clDedispersedData);
		clDedispersedData->copyDeviceToHost();
	} catch ( OpenCLError &err ) {
		cerr << err.what() << endl;
		return 1;
	}

	dedispersion(secondsToBuffer * observationCPU.getNrSamplesPerPaddedSecond(), observationCPU, dispersedData->getHostData(), dedispersedData, shifts);

	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
			if ( !same(dedispersedData[(dm * observationCPU.getNrSamplesPerPaddedSecond()) + sample], (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]) ) {
				wrongOnes++;

				cout << dm << " " << sample << " " << dedispersedData[(dm * observationCPU.getNrSamplesPerPaddedSecond()) + sample] << " " << (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " " << dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] - (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << endl;
			}
		}
	}

	cout << endl;

	cout << "Wrong samples: " << wrongOnes << " (" << (wrongOnes * 100) / (static_cast< long long unsigned int >(observation.getNrSeconds()) * observation.getNrDMs() * observation.getNrSamplesPerSecond()) << "%)." << endl;
	cout << endl;

	return 0;
}

