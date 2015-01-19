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

#include <string>
#include <vector>
#include <map>

#include <Observation.hpp>
#include <utils.hpp>

#ifndef DEDISPERSION_HPP
#define DEDISPERSION_HPP

namespace PulsarSearch {

public DedispersionConf {
public:
  DedispersionConf();
  ~DedispersionConf();

  // Get
  inline bool getLocalMem() const;
  inline unsigned int getNrsamplesPerBlock() const;
  inline unsigned int getNrSamplesPerThread() const;
  inline unsigned int getNrDMsPerBlock() const;
  inline unsigned int getNrDMsPerThread() const;
  inline unsigned int getUnroll() const;
  // Set
  inline void setLocalMem(bool local);
  inline void setNrSamplesPerBlock(unsigned int samples);
  inline void setNrSamplesPerThread(unsigned int samples);
  inline void setNrDMsPerBlock(unsigned int dms);
  inline void setNrDMsPerThread(unsigned int dms);
  inline void setUnroll(unsigned int unroll);

private:
  bool local;
  unsigned int nrSamplesPerBlock;
  unsigned int nrSamplesPerThread;
  unsigned int nrDMsPerBlock;
  unsigned int nrDMsPerThread;
  unsigned int unroll;
};

// Sequential dedispersion
template< typename T > void dedispersion(AstroData::Observation & observation, const std::vector< T > & input, std::vector< T > & output, const std::vector< float > & shifts);
// OpenCL dedispersion algorithm
std::string * getDedispersionOpenCL(const DedispersionConf & conf, const std::string & dataType, const AstroData::Observation & observation, std::vector< float > & shifts);

} // PulsarSearch

#endif // DEDISPERSION_HPP

