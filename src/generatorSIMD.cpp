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
#include <Exceptions.hpp>

typedef float dataType;
string typeName("float");


int main(int argc, char * argv[]) {
  bool avx = false;
  bool phi = false;
	unsigned int maxItemsPerThread = 0;
  std::string headerFilename;
  std::string implementationFilename;
  AstroData::Observation< dataType > observation("DedispersionTuning", typeName);

	try {
    isa::utils::ArgumentList args(argc, argv);

		observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
    headerFilename = args.getSwitchArgument< std::string >("-header");
    implementationFilename = args.getSwitchArgument< std::string >("-implementation");
    avx = args.getSwitch("-avx");
    phi = args.getSwitch("-phi");
    if ( avx ^ phi ) {
      throw isa::Exceptions::EmptyCommandLine();
    }
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
		std::cerr << argv[0] << " -padding ... -header ... -implementation ... [-avx] [-phi] -max_items ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	} catch ( std::exception &err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}
  std::vector< unsigned int > * shifts = PulsarSearch::getShifts(observation);
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
  delete shifts;

  std::ofstream headerFile(headerFilename);
  std::ofstream implementationFile(implementationFilename);

  headerFile << "#ifndef " + headerFilename + "\n#define " + headerFilename << std::endl;
  implementationFile << "#include <" + headerFilename + ">\n#include <Dedispersion.hpp>" << std::endl;
  implementationFile << "namespace PulsarSearch {\nstd::map< std::string, void * > * getDedispersionPointers() {" << std::endl;
  implementationFile << "std::map< std::string, void * > functionPointers = new std::map< std::string, void * >();" << std::endl;

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

      // Generate kernel
      std::string * code = 0;
      
      if ( avx ) {
        code = PulsarSearch::getDedispersionAVX(samplesPerThread, DMsPerThread, observation);
        implementationFile << "functionPointers->insert(std::make_pair(\"dedispersionAVX" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "\", &PulsarSearch::dedispersionAVX" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "));" << std::endl;
      } else if ( phi ) {
        code = PulsarSearch::getDedispersionPhi(samplesPerThread, DMsPerThread, observation);
        implementationFile << "functionPointers->insert(std::make_pair(\"dedispersionPhi" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "\", &PulsarSearch::dedispersionPhi" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "));" << std::endl;
      }
      headerFile << *code << std::endl;
      delete code;
    }
  }

  implementationFile << "}\n" << std::endl;
  headerFile << "#endif" << std::endl;

	return 0;
}

