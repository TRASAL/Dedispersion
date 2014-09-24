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
#include <cstring>
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
std::string typeName("float");


int main(int argc, char * argv[]) {
  bool avx = false;
  bool phi = false;
  unsigned int nrIterations = 0;
	unsigned int maxItemsPerThread = 0;
  AstroData::Observation observation;

	try {
    isa::utils::ArgumentList args(argc, argv);

    avx = args.getSwitch("-avx");
    phi = args.getSwitch("-phi");
    if ( !(avx || phi) ) {
      throw isa::Exceptions::EmptyCommandLine();
    }
    nrIterations = args.getSwitchArgument< unsigned int >("-iterations");
		observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
		maxItemsPerThread = args.getSwitchArgument< unsigned int >("-max_items");
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch ( isa::Exceptions::EmptyCommandLine &err ) {
		std::cerr << argv[0] << " [-avx] [-phi] -iterations ... -padding ... -max_items ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
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
  double flops = isa::utils::giga(static_cast< long long unsigned int >(observation.getNrDMs()) * observation.getNrChannels() * observation.getNrSamplesPerSecond());

  for ( unsigned int samplesPerThread = 1; samplesPerThread <= maxItemsPerThread; samplesPerThread++ ) {
    if ( avx ) {
      if ( (observation.getNrSamplesPerPaddedSecond() % (samplesPerThread * 8)) != 0 ) {
        continue;
      }
    } else if ( phi ) {
      if ( (observation.getNrSamplesPerPaddedSecond() % (samplesPerThread * 16)) != 0 ) {
        continue;
      }
    }
    for ( unsigned int DMsPerThread = 1; DMsPerThread <= maxItemsPerThread; DMsPerThread++ ) {
      if ( (observation.getNrDMs() % DMsPerThread) != 0 ) {
        continue;
      }
      if ( ( samplesPerThread * DMsPerThread ) > maxItemsPerThread ) {
        break;
      }

      // Tuning runs
      isa::utils::Timer timer;
      isa::utils::Stats< double > stats;
      PulsarSearch::dedispersionFunc< dataType > dedispersion = 0;

      if ( avx ) {
        dedispersion = functionPointers->at("dedispersionAVX" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread));
      } else if ( phi ) {
        dedispersion = functionPointers->at("dedispersionPhi" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread));
      }
      for ( unsigned int iteration = 0; iteration < nrIterations; iteration++ ) {
        std::memcpy(dispersedData.data(), dispersedData.data(), dispersedData.size() * sizeof(dataType));
        timer.start();
        dedispersion(observation.getNrDMs(), observation.getNrSamplesPerSecond(), observation.getNrSamplesPerDispersedChannel(), observation.getNrSamplesPerPaddedSecond(), observation.getNrChannels(), observation.getNrPaddedChannels(), dispersedData.data(), dedispersedData.data(), shifts->data());
        timer.stop();
        stats.addElement(flops / timer.getLastRunTime());
      }
      
      std::cout << observation.getNrDMs() << " " << observation.getNrChannels() << " " << observation.getNrSamplesPerSecond() << " " << samplesPerThread << " " << DMsPerThread << " " << std::setprecision(3) << stats.getAverage() << " " << stats.getStdDev() << " " << std::setprecision(6) << timer.getAverageTime() << " " << timer.getStdDev() << std::endl;
    }
  }

	return 0;
}

