//
// Copyright (C) 2013
// Alessio Sclocco <a.sclocco@vu.nl>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
using std::fixed;
#include <string>
using std::string;
#include <vector>
using std::vector;
#include <exception>
using std::exception;
#include <fstream>
using std::ofstream;
#include <iomanip>
using std::setprecision;
#include <limits>
using std::numeric_limits;

#include <ArgumentList.hpp>
using isa::utils::ArgumentList;
#include <Observation.hpp>
using AstroData::Observation;
#include <InitializeOpenCL.hpp>
using isa::OpenCL::initializeOpenCL;
#include <CLData.hpp>
using isa::OpenCL::CLData;
#include <Shifts.hpp>
using PulsarSearch::getShifts;
#include <Dedispersion.hpp>
//#include <DedispersionNoLocal.hpp>
using PulsarSearch::Dedispersion;
#include <utils.hpp>
using isa::utils::pad;
#include <Exceptions.hpp>
using isa::Exceptions::OpenCLError;
using isa::Exceptions::EmptyCommandLine;

typedef float dataType;
const string typeName("float");


int main(int argc, char * argv[]) {
	unsigned int nrIterations = 0;
	unsigned int clPlatformID = 0;
	unsigned int clDeviceID = 0;
	unsigned int minThreads = 0;
	unsigned int maxThreadsPerBlock = 0;
	unsigned int maxItemsPerThread = 0;
	unsigned int maxRows = 0;
	unsigned int secondsToBuffer = 0;
	Observation< dataType > observation("DedispersionTuning", typeName);
	CLData< unsigned int > * shifts = 0;
	CLData< dataType > * dispersedData = new CLData< dataType >("DispersedData", true);
	CLData< dataType > * dedispersedData = new CLData< dataType >("DedispersedData", true);

	try {
		ArgumentList args(argc, argv);

		nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
		clPlatformID = args.getSwitchArgument< unsigned int >("-opencl_platform");
		clDeviceID = args.getSwitchArgument< unsigned int >("-opencl_device");
		observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
		minThreads = args.getSwitchArgument< unsigned int >("-min_threads");
		maxThreadsPerBlock = args.getSwitchArgument< unsigned int >("-max_threads");
		maxItemsPerThread = args.getSwitchArgument< unsigned int >("-max_items");
		maxRows = args.getSwitchArgument< unsigned int >("-max_rows");
		observation.setMinFreq(args.getSwitchArgument< float >("-min_freq"));
		observation.setMaxFreq(observation.getMinFreq() + (observation.getChannelBandwidth() * (observation.getNrChannels() - 1)));
		observation.setChannelBandwidth(args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
		observation.setNrChannels(args.getSwitchArgument< unsigned int >("-channels"));
		observation.setNrDMs(args.getSwitchArgument< unsigned int >("-dms"));
		observation.setFirstDM(args.getSwitchArgument< float >("-dm_first"));
		observation.setDMStep(args.getSwitchArgument< float >("-dm_step"));
	} catch ( EmptyCommandLine err ) {
		cerr << argv[0] << " -iterations ... -opencl_platform ... -opencl_device ... -padding ... -min_threads ... -max_threads ... -max_items ... -max_rows ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << endl;
		return 1;
	} catch ( exception &err ) {
		cerr << err.what() << endl;
		return 1;
	}

	// Tuning
	cl::Context *clContext = new cl::Context();
	vector< cl::Platform > *clPlatforms = new vector< cl::Platform >();
	vector< cl::Device > *clDevices = new vector< cl::Device >();
	vector< vector< cl::CommandQueue > > *clQueues = new vector< vector < cl::CommandQueue > >();
	
	initializeOpenCL(clPlatformID, 1, clPlatforms, clContext, clDevices, clQueues);

	cout << fixed << endl;
	cout << "# nrDMs samplesPerBlock DMsPerBlock samplesPerThread DMsPerThread GFLOP/s std.dev. time std.dev." << endl << endl;

	delete shifts;
	shifts = getShifts(observation);
	secondsToBuffer = static_cast< unsigned int >(ceil(static_cast< float >(pad(observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())], observation.getPadding())) / observation.getNrSamplesPerPaddedSecond()));
	
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
	for ( unsigned int samples = minThreads; samples <= maxThreadsPerBlock; samples++ ) {
		if ( (observation.getNrSamplesPerPaddedSecond() % samples) == 0 ) {
			samplesPerBlock.push_back(samples);
		}
	}
	vector< unsigned int > DMsPerBlock;
	for ( unsigned int DMs = 1; DMs <= maxRows; DMs++ ) {
		if ( (observation.getNrDMs() % DMs) == 0 ) {
			DMsPerBlock.push_back(DMs);
		}
	}

	for ( vector< unsigned int >::iterator samples = samplesPerBlock.begin(); samples != samplesPerBlock.end(); samples++ ) {
		for ( vector< unsigned int >::iterator DMs = DMsPerBlock.begin(); DMs != DMsPerBlock.end(); DMs++ ) {
			if ( ((*samples) * (*DMs)) > maxThreadsPerBlock ) {
				break;
			}

			for ( unsigned int samplesPerThread = 1; samplesPerThread <= maxItemsPerThread; samplesPerThread++ ) {
				if ( (observation.getNrSamplesPerPaddedSecond() % ((*samples) * samplesPerThread)) != 0 ) {
					continue;
				}

				for ( unsigned int DMsPerThread = 1; DMsPerThread <= maxItemsPerThread; DMsPerThread++ ) {
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

						cout << observation.getNrDMs() << " " << *samples << " " << *DMs << " " << samplesPerThread << " " << DMsPerThread << " " << setprecision(3) << Acur << " " << Vcur << " " << setprecision(6) << clDedisperse.getTimer().getAverageTime() << " " << clDedisperse.getTimer().getStdDev() << endl;
					} catch ( OpenCLError err ) {
						cerr << err.what() << endl;
						continue;
					}
				}
			}
		}
	}

	cout << endl;

	return 0;
}
