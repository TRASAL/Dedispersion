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
#include <InitializeOpenCL.hpp>
#include <CLData.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
using isa::utils::ArgumentList;
using AstroData::Observation;
using isa::OpenCL::initializeOpenCL;
using isa::OpenCL::CLData;
using PulsarSearch::getShifts;
using PulsarSearch::Dedispersion;

typedef float dataType;
const string typeName("float");
const unsigned int maxThreadsPerBlock = 1024;
const unsigned int maxItemsPerThread = 256;

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
	unsigned int nrIterations = 0;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	Observation< dataType > observation("DedispersionTuning", typeName);
	CLData< unsigned int > * shifts = 0;
	CLData< dataType > * dispersedData = new CLData< dataType >("DispersedData", true);
	CLData< dataType > * dedispersedData = new CLData< dataType >("DedispersedData", true);

	try {
		ArgumentList args(argc, argv);

		nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
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
	observation.setFirstDM(firstDM);
	observation.setDMStep(DMStep);
		
	// Tuning
	cl::Context *clContext = new cl::Context();
	vector< cl::Platform > *clPlatforms = new vector< cl::Platform >();
	vector< cl::Device > *clDevices = new vector< cl::Device >();
	vector< vector< cl::CommandQueue > > *clQueues = new vector< vector < cl::CommandQueue > >();
	
	initializeOpenCL(clPlatformID, 1, clPlatforms, clContext, clDevices, clQueues);

	cout << fixed << endl;
	cout << "# nrDMs samplesPerBlock DMsPerBlock samplesPerThread DMsPerThread GFLOP/s std.dev. time std.dev." << endl << endl;

	for ( unsigned int nrDMs = 2; nrDMs <= 4096; nrDMs *= 2 ) {
		unsigned int nrSamplesPerChannel = 0;
		unsigned int secondsToBuffer = 0;
		observation.setNrDMs(nrDMs);
		delete shifts;
		shifts = getShifts(observation);
				
		if ( ((observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % padding) != 0 ) {
			nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) + (padding - ((observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]) % padding));
		} else {
			nrSamplesPerChannel = (observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
		}
		secondsToBuffer = static_cast< unsigned int >(ceil(static_cast< float >(nrSamplesPerChannel) / observation.getNrSamplesPerPaddedSecond()));
		
		// Allocate memory
		dispersedData->allocateHostData(secondsToBuffer * observation.getNrChannels() * observation.getNrSamplesPerPaddedSecond());
		dedispersedData->allocateHostData(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
				
		try {
			shifts->setCLContext(clContext);
			shifts->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
			shifts->allocateDeviceData();
			shifts->copyHostToDevice();

			dispersedData->setCLContext(clContext);
			dispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
			dispersedData->setDeviceReadOnly();
			dispersedData->allocateDeviceData();

			dedispersedData->setCLContext(clContext);
			dedispersedData->setCLQueue(&((clQueues->at(clDeviceID)).at(0)));
			dedispersedData->allocateDeviceData();
		} catch ( OpenCLError err ) {
			cerr << err.what() << endl;
			return 1;
		}
		
		// Find the parameters
		vector< unsigned int > samplesPerBlock;
		for ( unsigned int samples = 32; samples <= maxThreadsPerBlock; samples++ ) {
			if ( (observation.getNrSamplesPerSecond() % samples) == 0 ) {
				samplesPerBlock.push_back(samples);
			}
		}
		vector< unsigned int > DMsPerBlock;
		for ( unsigned int DMs = 1; DMs <= 32; DMs++ ) {
			if ( (observation.getNrDMs() % DMs) == 0 ) {
				DMsPerBlock.push_back(DMs);
			}
		}

		for ( vector< unsigned int >::iterator samples = samplesPerBlock.begin(); samples != samplesPerBlock.end(); samples++ ) {
			for ( vector< unsigned int >::iterator DMs = DMsPerBlock.begin(); DMs != DMsPerBlock.end(); DMs++ ) {
				if ( ((*samples) * (*DMs)) > maxThreadsPerBlock ) {
					break;
				}

				for ( unsigned int samplesPerThread = 1; samplesPerThread <= 32; samplesPerThread++ ) {
					if ( (observation.getNrSamplesPerSecond() % ((*samples) * samplesPerThread)) != 0 ) {
						continue;
					}

					for ( unsigned int DMsPerThread = 1; DMsPerThread <= 32; DMsPerThread++ ) {
						double Acur = 0.0;
						double Aold = 0.0;
						double Vcur = 0.0;
						double Vold = 0.0;

						if ( (observation.getNrDMs() % ((*DMs) * DMsPerThread)) != 0 ) {
							continue;
						}
						if ( ( samplesPerThread * DMsPerThread ) > maxItemsPerThread ) {
							break;
						}

						try {
							// Generate kernel
							Dedispersion< dataType > clDedisperse("Dedisperse", typeName);
							clDedisperse.bindOpenCL(clContext, &(clDevices->at(clDeviceID)), &((clQueues->at(clDeviceID)).at(0)));
							clDedisperse.setNrSamplesPerBlock(*samples);
							clDedisperse.setNrDMsPerBlock(*DMs);
							clDedisperse.setNrSamplesPerThread(samplesPerThread);
							clDedisperse.setNrDMsPerThread(DMsPerThread);
							clDedisperse.setNrSamplesPerDispersedChannel(secondsToBuffer * observation.getNrSamplesPerPaddedSecond());
							clDedisperse.setObservation(&observation);
							clDedisperse.setShifts(shifts);
							clDedisperse.generateCode();

							// Warm-up
							clDedisperse(dispersedData, dedispersedData);
							clDedisperse.getTimer().reset();

							for ( unsigned int iteration = 0; iteration < nrIterations; iteration++ ) {
								clDedisperse(dispersedData, dedispersedData);

								if ( iteration == 0 ) {
									Acur = clDedisperse.getGFLOP() / clDedisperse.getTimer().getLastRunTime();
								}
								else {
									Aold = Acur;
									Vold = Vcur;

									Acur = Aold + (((clDedisperse.getGFLOP() / clDedisperse.getTimer().getLastRunTime()) - Aold) / (iteration + 1));
									Vcur = Vold + (((clDedisperse.getGFLOP() / clDedisperse.getTimer().getLastRunTime()) - Aold) * ((clDedisperse.getGFLOP() / clDedisperse.getTimer().getLastRunTime()) - Acur));
								}
							}
							Vcur = sqrt(Vcur / nrIterations);

							cout << nrDMs << " " << *samples << " " << *DMs << " " << samplesPerThread << " " << DMsPerThread << " " << setprecision(3) << Acur << " " << Vcur << " " << setprecision(6) << clDedisperse.getTimer().getAverageTime() << " " << clDedisperse.getTimer().getStdDev() << endl;
						} catch ( OpenCLError err ) {
							cerr << err.what() << endl;
							continue;
						}
					}
				}
			}
		}

		cout << endl << endl;
	}

	cout << endl;

	return 0;
}

