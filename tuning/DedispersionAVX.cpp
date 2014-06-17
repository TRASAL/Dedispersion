// Copyright 2014 Alessio Sclocco <a.sclocco@vu.nl>
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
#include <fstream>
#include <string>
#include <vector>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>

#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>
#include <utils.hpp>
#include <Timer.hpp>
#include <Stats.hpp>
#include <Exceptions.hpp>

typedef float dataType;
string typeName("float");


int main(int argc, char * argv[]) {
  unsigned int nrIterations = 0;
	unsigned int maxItemsPerThread = 0;
  AstroData::Observation< dataType > observation("DedispersionTuning", typeName);

	try {
    isa::utils::ArgumentList args(argc, argv);

    nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
		observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
		maxItemsPerThread = args.getSwitchArgument< unsigned int >("-max_items");
		observation.setMinFreq(args.getSwitchArgument< float >("-min_freq"));
		observation.setChannelBandwidth(args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
		observation.setNrChannels(args.getSwitchArgument< unsigned int >("-channels"));
		observation.setNrDMs(args.getSwitchArgument< unsigned int >("-dms"));
		observation.setFirstDM(args.getSwitchArgument< float >("-dm_first"));
		observation.setDMStep(args.getSwitchArgument< float >("-dm_step"));
		observation.setMaxFreq(observation.getMinFreq() + (observation.getChannelBandwidth() * (observation.getNrChannels() - 1)));
	} catch ( isa::Exceptions::EmptyCommandLine &err ) {
		std::cerr << argv[0] << " -iterations ... -padding ... -max_items ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	} catch ( std::exception &err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}
  std::vector< unsigned int > * shifts = PulsarSearch::getShifts(observation);
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
	
  // Allocate memory
  std::vector< dataType > dispersedData = std::vector< dataType >(observation.getNrChannels() * observation.getNrSamplesPerDispersedChannel());
  std::vector< dataType > dedispersedData = std::vector< dataType >(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
  std::map< std::string, PulsarSearch::dedispersionFunc< dataType > > * functionPointers = PulsarSearch::getDedispersionPointers< dataType >();

	srand(time(NULL));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerDispersedChannel(); sample++ ) {
      dispersedData[(channel * observation.getNrSamplesPerDispersedChannel()) + sample] = static_cast< dataType >(rand() % 10);
		}
	}

	std::cout << std::fixed << std::endl;
	std::cout << "# nrDMs nrChannels nrSamples samplesPerThread DMsPerThread GFLOP/s err time err" << std::endl << std::endl;

  for ( unsigned int samplesPerThread = 1; samplesPerThread <= maxItemsPerThread; samplesPerThread++ ) {
    if ( (observation.getNrSamplesPerPaddedSecond() % samplesPerThread) != 0 ) {
      continue;
    }
    for ( unsigned int DMsPerThread = 1; DMsPerThread <= maxItemsPerThread; DMsPerThread++ ) {
      if ( (observation.getNrDMs() % DMsPerThread) != 0 ) {
        continue;
      }
      if ( ( samplesPerThread * DMsPerThread ) > maxItemsPerThread ) {
        break;
      }

      // Tuning runs
      double flops = isa::utils::giga(static_cast< long long unsigned int >(observation.getNrDMs()) * observation.getNrChannels() * observation.getNrSamplesPerSecond());
      isa::utils::Timer timer("Kernel Timer");
      isa::utils::Stats< double > stats;

      PulsarSearch::dedispersionFunc< dataType > dedispersion = functionPointers->at("dedispersionAVX" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread));
      for ( unsigned int iteration = 0; iteration < nrIterations; iteration++ ) {
        timer.start();
        dedispersion(dispersedData.data(), dedispersedData.data(), shifts->data());
        timer.stop();
        stats.addElement(flops / timer.getLastRunTime());
      }
      
      std::cout << observation.getNrDMs() << " " << observation.getNrChannels() << " " << observation.getNrSamplesPerSecond() << " " << samplesPerThread << " " << DMsPerThread << " " << std::setprecision(3) << stats.getAverage() << " " << stats.getStdDev() << " " << std::setprecision(6) << timer.getAverageTime() << " " << timer.getStdDev() << std::endl;
    }
  }

	return 0;
}

