// Copyright 2015 Alessio Sclocco <a.sclocco@vu.nl>
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

#include <Dedispersion.hpp>

namespace PulsarSearch {

void readTunedDedispersionConf(tunedDedispersionConf & tunedDedispersion, const std::string  & dedispersionFilename) {
	std::string temp;
	std::ifstream dedispersionFile(dedispersionFilename);

	while ( ! dedispersionFile.eof() ) {
		unsigned int splitPoint = 0;

		std::getline(dedispersionFile, temp);
		if ( ! std::isalpha(temp[0]) ) {
			continue;
		}
		std::string deviceName;
		unsigned int nrDMs = 0;
    PulsarSearch::DedispersionConf conf;

		splitPoint = temp.find(" ");
		deviceName = temp.substr(0, splitPoint);
		temp = temp.substr(splitPoint + 1);
		splitPoint = temp.find(" ");
		nrDMs = isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint));
		temp = temp.substr(splitPoint + 1);
		splitPoint = temp.find(" ");
		conf.setSplitSeconds(isa::utils::castToType< std::string, bool >(temp.substr(0, splitPoint)));
		temp = temp.substr(splitPoint + 1);
		splitPoint = temp.find(" ");
		conf.setLocalMem(isa::utils::castToType< std::string, bool >(temp.substr(0, splitPoint)));
		temp = temp.substr(splitPoint + 1);
		splitPoint = temp.find(" ");
		conf.setUnroll(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
		temp = temp.substr(splitPoint + 1);
		splitPoint = temp.find(" ");
		conf.setNrSamplesPerBlock(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
		temp = temp.substr(splitPoint + 1);
		splitPoint = temp.find(" ");
		conf.setNrDMsPerBlock(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
		temp = temp.substr(splitPoint + 1);
		splitPoint = temp.find(" ");
		conf.setNrSamplesPerThread(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
		temp = temp.substr(splitPoint + 1);
		conf.setNrDMsPerThread(isa::utils::castToType< std::string, unsigned int >(temp));

		if ( tunedDedispersion.count(deviceName) == 0 ) {
      std::map< unsigned int, PulsarSearch::DedispersionConf > container;

			container.insert(std::make_pair(nrDMs, conf));
			tunedDedispersion.insert(std::make_pair(deviceName, container));
		} else {
			tunedDedispersion[deviceName].insert(std::make_pair(nrDMs, conf));
		}
	}
}

DedispersionConf::DedispersionConf() {}

DedispersionConf::~DedispersionConf() {}

std::string DedispersionConf::print() const {
  return std::string(isa::utils::toString(splitSeconds) + " " + isa::utils::toString(local) + " " + isa::utils::toString(unroll) + " " + isa::utils::toString(nrSamplesPerBlock) + " " + isa::utils::toString(nrDMsPerBlock) + " " + isa::utils::toString(nrSamplesPerThread) + " " + isa::utils::toString(nrDMsPerThread));
}

} // PulsarSearch

