// Copyright 2011 Alessio Sclocco <a.sclocco@vu.nl>
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

#include <string>
#include <vector>

#include <Observation.hpp>

#ifndef DEDISPERSION_HPP
#define DEDISPERSION_HPP

namespace PulsarSearch {

// OpenCL dedispersion algorithm
std::string * getDedispersionOpenCL(bool localMem, unsigned int nrSamplesPerBlock, unsigned int nrDMsPerBlock, unsigned int nrSamplesPerThread, unsigned int nrDMsPerThread, unsigned int inputChannelSize, std::string &dataType, AstroData::Observation &observation, std::vector & shifts);

} // PulsarSearch

#endif // DEDISPERSION_HPP
