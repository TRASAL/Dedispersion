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
#include <string>
#include <vector>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ctime>

#include <ArgumentList.hpp>
#include <Exceptions.hpp>
#include <Observation.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <Dedispersion.hpp>


int main(int argc, char *argv[]) {
  bool avx = false;
  bool phi = false;
	unsigned int nrSamplesPerThread = 0;
	unsigned int nrDMsPerThread = 0;
	long long unsigned int wrongSamples = 0;
	AstroData::Observation observation;

	try {
    isa::utils::ArgumentList args(argc, argv);
    avx = args.getSwitch("-avx");
    phi = args.getSwitch("-phi");
    if ( ! (avx || phi) ) {
      throw isa::Exceptions::EmptyCommandLine();
    }
    observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
		nrSamplesPerThread = args.getSwitchArgument< unsigned int >("-st");
		nrDMsPerThread = args.getSwitchArgument< unsigned int >("-dt");
		observation.setMinFreq(args.getSwitchArgument< float >("-min_freq"));
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
		observation.setNrSamplesPerSecond(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
	} catch  ( isa::Exceptions::SwitchNotFound &err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  }catch ( std::exception &err ) {
    std::cerr << "Usage: " << argv[0] << " [-avx] [-phi] -padding ... -st ... -dt ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	}

  std::vector< unsigned int > * shifts = PulsarSearch::getShifts(observation);
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);

	// Allocate memory
  std::vector< float > dispersedData = std::vector< float >(observation.getNrChannels() * observation.getNrSamplesPerDispersedChannel());
  std::vector< float > dedispersedData = std::vector< float >(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());
  std::vector< float > dedispersedData_c = std::vector< float >(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond());

	srand(time(NULL));
	for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerDispersedChannel(); sample++ ) {
      dispersedData[(channel * observation.getNrSamplesPerDispersedChannel()) + sample] = static_cast< float >(rand() % 10);
		}
	}

  // Run SIMD kernel and CPU control
  PulsarSearch::dedispersionFunc< dataType > dedispersion = 0;
  if ( avx ) {
    dedispersion = functionPointers->at("dedispersionAVX" + isa::utils::toString< unsigned int >(nrSamplesPerThread) + "x" + isa::utils::toString< unsigned int >(nrDMsPerThread));
  } else if ( phi ) {
    dedispersion = functionPointers->at("dedispersionPhi" + isa::utils::toString< unsigned int >(nrSamplesPerThread) + "x" + isa::utils::toString< unsigned int >(nrDMsPerThread));
  }
  dedispersion(observation.getNrDMs(), observation.getNrSamplesPerSecond(), observation.getNrSamplesPerDispersedChannel(), observation.getNrSamplesPerPaddedSecond(), observation.getNrChannels(), observation.getNrPaddedChannels(), dispersedData.data(), dedispersedData.data(), shifts->data());
  PulsarSearch::dedispersion< dataType >(observation, dispersedData, dedispersedData_c, *shifts);

	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
      if ( !isa::utils::same(dedispersedData_c[(dm * observation.getNrSamplesPerPaddedSecond()) + sample], dedispersedData[(dm * observation.getNrSamplesPerPaddedSecond()) + sample]) ) {
        wrongSamples++;
			}
		}
	}

  if ( wrongSamples > 0 ) {
    std::cout << "Wrong samples: " << wrongSamples << " (" << (wrongSamples * 100.0) / (static_cast< long long unsigned int >(observation.getNrDMs()) * observation.getNrSamplesPerSecond()) << "%)." << std::endl;
  } else {
    std::cout << "TEST PASSED." << std::endl;
  }

	return 0;
}

