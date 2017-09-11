// Copyright 2017 Netherlands Institute for Radio Astronomy (ASTRON)
// Copyright 2017 Netherlands eScience Center
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

namespace Dedispersion {

void readTunedDedispersionConf(tunedDedispersionConf & tunedDedispersion, const std::string  & dedispersionFilename) {
  unsigned int splitPoint = 0;
  unsigned int nrDMs = 0;
  std::string temp;
  std::string deviceName;
  Dedispersion::DedispersionConf * conf = 0;
  std::ifstream dedispersionFile;

  dedispersionFile.open(dedispersionFilename);
  if ( !dedispersionFile ) {
    throw AstroData::FileError("Impossible to open " + dedispersionFilename);
  }
  while ( ! dedispersionFile.eof() ) {
    std::getline(dedispersionFile, temp);
    if ( ! std::isalpha(temp[0]) ) {
      continue;
    }

    conf = new Dedispersion::DedispersionConf();
    splitPoint = temp.find(" ");
    deviceName = temp.substr(0, splitPoint);
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    nrDMs = isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setSplitBatches(isa::utils::castToType< std::string, bool >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setLocalMem(isa::utils::castToType< std::string, bool >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setUnroll(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setNrThreadsD0(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setNrThreadsD1(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setNrThreadsD2(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setNrItemsD0(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    splitPoint = temp.find(" ");
    conf->setNrItemsD1(isa::utils::castToType< std::string, unsigned int >(temp.substr(0, splitPoint)));
    temp = temp.substr(splitPoint + 1);
    conf->setNrItemsD2(isa::utils::castToType< std::string, unsigned int >(temp));

    if ( tunedDedispersion.count(deviceName) == 0 ) {
      std::map< unsigned int, Dedispersion::DedispersionConf * > * container = new std::map< unsigned int, Dedispersion::DedispersionConf * >();

      container->insert(std::make_pair(nrDMs, conf));
      tunedDedispersion.insert(std::make_pair(deviceName, container));
    } else {
      tunedDedispersion.at(deviceName)->insert(std::make_pair(nrDMs, conf));
    }
  }
  dedispersionFile.close();
}

DedispersionConf::DedispersionConf() : KernelConf(), splitBatches(false), local(false), unroll(1) {}

DedispersionConf::~DedispersionConf() {}

std::string DedispersionConf::print() const {
  return std::to_string(splitBatches) + " " + std::to_string(local) + " " + std::to_string(unroll) + " " + isa::OpenCL::KernelConf::print();
}

} // Dedispersion

