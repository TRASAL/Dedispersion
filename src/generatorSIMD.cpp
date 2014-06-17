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
  AstroData::Observation< dataType > observation("DedispersionTuning", typeName);

	try {
    isa::utils::ArgumentList args(argc, argv);

		observation.setPadding(args.getSwitchArgument< unsigned int >("-padding"));
    headerFilename = args.getSwitchArgument< std::string >("-header");
    avx = args.getSwitch("-avx");
    phi = args.getSwitch("-phi");
    if ( !(avx ^ phi) ) {
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
		std::cerr << argv[0] << " -padding ... -header ... [-avx] [-phi] -max_items ... -min_freq ... -channel_bandwidth ... -samples ... -channels ... -dms ... -dm_first ... -dm_step ..." << std::endl;
		return 1;
	} catch ( std::exception &err ) {
		std::cerr << err.what() << std::endl;
		return 1;
	}
  std::vector< unsigned int > * shifts = PulsarSearch::getShifts(observation);
  observation.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerSecond() + (*shifts)[((observation.getNrDMs() - 1) * observation.getNrPaddedChannels())]);
  delete shifts;

  std::string underscore("_");
  std::string * defineName = isa::utils::replace(&headerFilename, ".", underscore);
  std::ofstream headerFile(headerFilename);
  std::string implementation;

  headerFile << "#ifndef " + *defineName + "\n#define " + *defineName << std::endl;
  implementation += "namespace PulsarSearch {\ntemplate< typename T > std::map< std::string, dedispersionFunc< T > > * getDedispersionPointers() {\n";
  implementation += "std::map< std::string, dedispersionFunc< T > > * functionPointers = new std::map< std::string, dedispersionFunc< T > >();\n";

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
        implementation += "functionPointers->insert(std::pair< std::string, dedispersionFunc< T > >(\"dedispersionAVX" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "\", dedispersionAVX" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "));\n";
      } else if ( phi ) {
        code = PulsarSearch::getDedispersionPhi(samplesPerThread, DMsPerThread, observation);
        implementation += "functionPointers->insert(std::pair< std::string, dedispersionFunc< T > >(\"dedispersionPhi" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "\", dedispersionPhi" + isa::utils::toString< unsigned int >(samplesPerThread) + "x" + isa::utils::toString< unsigned int >(DMsPerThread) + "));\n";
      }
      headerFile << *code << std::endl;
      delete code;
    }
  }

  implementation +="return functionPointers;\n}\n}\n";
  headerFile << implementation << std::endl;
  headerFile << "#endif" << std::endl;

	return 0;
}

