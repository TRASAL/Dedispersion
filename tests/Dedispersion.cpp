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
#include <GPUData.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
using isa::utils::ArgumentList;
using AstroData::Observation;
using AstroData::readLOFAR;
using isa::OpenCL::initializeOpenCL;
using isa::OpenCL::GPUData;
using isa::utils::same;
using TDM::getShifts;
using TDM::Dedispersion;
using TDM::dedisperse;

typedef float dataType;
const string typeName("float");


int main(int argc, char *argv[]) {
	unsigned int firstSecond = 0;
	unsigned int nrSeconds = 0;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	long long unsigned int wrongOnes = 0;
	string headerFileName;
	string dataFileName;
	Observation< dataType > observation("DedispersionTest", typeName);

	try {
		ArgumentList args(argc, argv);

		firstSecond = args.getSwitchArgument< unsigned int >("-fs");
		nrSeconds = args.getSwitchArgument< unsigned int >("-ns");
		
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");

		headerFileName = args.getSwitchArgument< string >("-header");
		dataFileName = args.getSwitchArgument< string >("-data");

		observation.setFirstDM(args.getSwitchArgument< float >("-dm_first"));
		observation.setDMStep(args.getSwitchArgument< float >("-dm_step"));
		observation.setNrDMs(args.getSwitchArgument< unsigned int >("-dm_number"));
	}
	catch ( exception &err ) {
		cerr << err.what() << endl;
		return 1;
	}
	
	// Load the observation data
	vector< GPUData< dataType > * > *input = new vector< GPUData< dataType > * >(1);
	readLOFAR(headerFileName, dataFileName, observation, *input, nrSeconds, firstSecond);

	// Print some statistics
	cout << fixed << setprecision(3) << endl;
	cout << "Total seconds: \t\t" << observation.getNrSeconds() << endl;
	cout << "Min frequency: \t\t" << observation.getMinFreq() << " MHz" << endl;
	cout << "Max frequency: \t\t" << observation.getMaxFreq() << " MHz" << endl;
	cout << "Nr. channels: \t\t" << observation.getNrChannels() << endl;
	cout << "Channel bandwidth: \t" << observation.getChannelBandwidth() << " MHz" << endl;
	cout << "Samples/second: \t" << observation.getNrSamplesPerSecond() << endl;
	cout << "Min sample: \t\t" << observation.getMinValue() << endl;
	cout << "Max sample: \t\t" << observation.getMaxValue() << endl;
	cout << endl;

	// Test
	cl::Context *clContext = new cl::Context();
	vector< cl::Platform > *clPlatforms = new vector< cl::Platform >();
	vector< cl::Device > *clDevices = new vector< cl::Device >();
	vector< vector< cl::CommandQueue > > *clQueues = new vector< vector < cl::CommandQueue > >();
		
	unsigned int nrSamplesPerChannel = 0;
	unsigned int secondsToBuffer = 0;
	GPUData< unsigned int > *shifts = getShifts(observation);
	GPUData< dataType > *dispersedData = new GPUData< dataType >("DispersedData", true, true);
	GPUData< dataType > *dedispersedData = new GPUData< dataType >("DedispersedData", true);
	GPUData< dataType > *clDedispersedData = new GPUData< dataType >("clDedispersedData", true);

	initializeOpenCL(clPlatformID, 1, clPlatforms, clContext, clDevices, clQueues);
	
	if ( ((observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % 4) != 0 ) {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) + (4 - ((observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % 4));
	}
	else {
		nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
	}
	secondsToBuffer = static_cast< unsigned int >(ceil(static_cast< float >(nrSamplesPerChannel) / observation.getNrSamplesPerPaddedSecond()));
	if ( nrSeconds < secondsToBuffer ) {
		cerr << "Not enough seconds." << endl;
		return 1;
	}
	
	// Allocate memory
	shifts->setCLContext(clContext);
	shifts->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
	dispersedData->allocateHostData(secondsToBuffer * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond());
	dispersedData->setCLContext(clContext);
	dispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
	dedispersedData->allocateHostData(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
	clDedispersedData->allocateHostData(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
	clDedispersedData->setCLContext(clContext);
	clDedispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
	
	shifts->allocateDeviceData();
	shifts->copyHostToDevice();
	dispersedData->allocateDeviceData();
	clDedispersedData->allocateDeviceData();
	
	// Generate kernel
	Dedispersion< dataType > clDedisperse("Dedisperse", typeName);
	clDedisperse.bindOpenCL(clContext, &(clDevices->at(clDeviceID)), &((clQueues->at(clDeviceID)).at(0)));
	clDedisperse.setNrSamplesPerBlock(126);
	clDedisperse.setNrDMsPerBlock(1);
	clDedisperse.setNrSamplesPerThread(1);
	clDedisperse.setNrDMsPerThread(1);
	clDedisperse.setNrSamplesPerDispersedChannel(secondsToBuffer * observation.getNrSamplesPerPaddedSecond());
	clDedisperse.setObservation(&observation);
	clDedisperse.setShifts(shifts);
	clDedisperse.generateCode();

	cout << clDedisperse.getCode() << endl;
	cout << endl;

	// Dedispersion loop
	for ( unsigned int second = 0; second < nrSeconds - (secondsToBuffer - 1); second++ ) {
		for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
			for ( unsigned int chunk = 0; chunk < secondsToBuffer; chunk++ ) {
				memcpy(dispersedData->getRawHostDataAt((channel * secondsToBuffer * observation.getNrSamplesPerPaddedSecond()) + (chunk * observation.getNrSamplesPerSecond())), (input->at(second + chunk))->getRawHostDataAt(channel * observation.getNrSamplesPerPaddedSecond()), observation.getNrSamplesPerSecond() * sizeof(dataType));
			}
		}

		dispersedData->copyHostToDevice();
		clDedisperse(dispersedData, clDedispersedData);
		clDedispersedData->copyDeviceToHost();

		dedisperse(observation.getNrSamplesPerPaddedSecond() * secondsToBuffer, observation, dispersedData, dedispersedData, shifts);

		for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
			for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
				if ( !same((*dedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample], (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]) ) {
					wrongOnes++;

					cout << dm << " " << sample << " " << (*dedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " " << (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << " " << (*dedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] - (*clDedispersedData)[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] << endl;
				}
			}
		}
	}
	cout << endl;

	cout << "Wrong samples: " << wrongOnes << " (" << (wrongOnes * 100) / (static_cast< long long unsigned int >(observation.getNrSeconds()) * observation.getNrDMs() * observation.getNrSamplesPerSecond()) << "%)." << endl;
	cout << endl;
	cout << fixed << setprecision(6);
	cout << "Kernel timing: " << clDedisperse.getTime() << " (total), " << clDedisperse.getTime() / (nrSeconds - (secondsToBuffer - 1)) << " (average)" << endl;
	cout << endl;

	return 0;
}

