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
#include <fstream>

#include <Observation.hpp>
#include <utils.hpp>
#include <Bits.hpp>

#ifndef DEDISPERSION_HPP
#define DEDISPERSION_HPP

namespace PulsarSearch {

class DedispersionConf {
public:
  DedispersionConf();
  ~DedispersionConf();

  // Get
  bool getSplitSeconds() const;
  bool getLocalMem() const;
  unsigned int getNrThreadsD0() const;
  unsigned int getNrItemsD0() const;
  unsigned int getNrThreadsD1() const;
  unsigned int getNrItemsD1() const;
  unsigned int getUnroll() const;
  // Set
  void setSplitSeconds(bool split);
  void setLocalMem(bool local);
  void setNrThreadsD0(unsigned int threads);
  void setNrItemsD0(unsigned int items);
  void setNrThreadsD1(unsigned int threads);
  void setNrItemsD1(unsigned int items);
  void setUnroll(unsigned int unroll);
  // Utils
  std::string print() const;

private:
  bool splitSeconds;
  bool local;
  unsigned int nrThreadsD0;
  unsigned int nrItemsD0;
  unsigned int nrThreadsD1;
  unsigned int nrItemsD1;
  unsigned int unroll;
};

typedef std::map< std::string, std::map< unsigned int, PulsarSearch::DedispersionConf * > * > tunedDedispersionConf;

// Sequential
template< typename I, typename L, typename O > void subbandDedispersion(AstroData::Observation & observation, const std::vector< uint8_t > & zappedChannels, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits);
template< typename I, typename L, typename O > void dedispersion(AstroData::Observation & observation, const std::vector< uint8_t > & zappedChannels, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits);
// OpenCL
template< typename I, typename O > std::string * getDedispersionOpenCL(const DedispersionConf & conf, const unsigned int padding, const uint8_t inputBits, const std::string & inputDataType, const std::string & intermediateDataType, const std::string & outputDataType, const AstroData::Observation & observation, std::vector< float > & shifts, const std::vector< uint8_t > & zappedChannels);
void readTunedDedispersionConf(tunedDedispersionConf & tunedDedispersion, const std::string & dedispersionFilename);


// Implementations
template< typename I, typename L, typename O > void subbandDedispersion(AstroData::Observation & observation, const std::vector< uint8_t > & zappedChannels, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits) {
	for ( unsigned int dm = 0; dm < observation.getNrDMsSubbanding(); dm++ ) {
    for ( unsigned int subband = 0; subband < observation.getNrSubbands(); subband++ ) {
      for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatchSubbanding(); sample++ ) {
        L dedispersedSample = static_cast< L >(0);

        for ( unsigned int channel = subband * observation.getNrChannelsPerSubband(); channel < (subband + 1) * observation.getNrChannelsPerSubband(); channel++ ) {
          unsigned int shift = static_cast< unsigned int >((observation.getFirstDMSubbanding() + (dm * observation.getDMSubbandingStep())) * (shifts[channel] - shifts[((subband + 1) * observation.getNrChannelsPerSubband()) - 1]));

          if ( zappedChannels[channel] != 0 ) {
            // If a channel is zapped, skip it
            continue;
          }
          if ( inputBits >= 8 ) {
            dedispersedSample += static_cast< L >(input[(channel * observation.getNrSamplesPerPaddedSubbandingDispersedChannel(padding / sizeof(I))) + (sample + shift)]);
          } else {
            unsigned int byte = (sample + shift) / (8 / inputBits);
            uint8_t firstBit = ((sample + shift) % (8 / inputBits)) * inputBits;
            char value = 0;
            char buffer = input[(channel * isa::utils::pad(observation.getNrSamplesPerSubbandingDispersedChannel() / (8 / inputBits), padding / sizeof(I))) + byte];

            for ( uint8_t bit = 0; bit < inputBits; bit++ ) {
              isa::utils::setBit(value, isa::utils::getBit(buffer, firstBit + bit), bit);
            }
            dedispersedSample += static_cast< L >(value);
          }
        }

        output[(subband * observation.getNrDMsSubbanding() * observation.getNrSamplesPerPaddedBatch(padding / sizeof(O))) + (dm * observation.getNrSamplesPerPaddedBatchSubbanding(padding / sizeof(O))) + sample] = static_cast< O >(dedispersedSample);
      }
    }
	}
}

template< typename I, typename L, typename O > void dedispersion(AstroData::Observation & observation, const std::vector< uint8_t > & zappedChannels, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits) {
	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch(); sample++ ) {
			L dedispersedSample = static_cast< L >(0);

			for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
				unsigned int shift = static_cast< unsigned int >((observation.getFirstDM() + (dm * observation.getDMStep())) * shifts[channel]);

        if ( zappedChannels[channel] != 0 ) {
          // If a channel is zapped, skip it
          continue;
        }
        if ( inputBits >= 8 ) {
          dedispersedSample += static_cast< L >(input[(channel * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + (sample + shift)]);
        } else {
          unsigned int byte = (sample + shift) / (8 / inputBits);
          uint8_t firstBit = ((sample + shift) % (8 / inputBits)) * inputBits;
          char value = 0;
          char buffer = input[(channel * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(I))) + byte];

          for ( uint8_t bit = 0; bit < inputBits; bit++ ) {
            isa::utils::setBit(value, isa::utils::getBit(buffer, firstBit + bit), bit);
          }
          dedispersedSample += static_cast< L >(value);
        }
			}

			output[(dm * observation.getNrSamplesPerPaddedSecond(padding / sizeof(O))) + sample] = static_cast< O >(dedispersedSample);
		}
	}
}

inline bool DedispersionConf::getSplitSeconds() const {
  return splitSeconds;
}

inline bool DedispersionConf::getLocalMem() const {
  return local;
}

inline unsigned int DedispersionConf::getNrThreadsD0() const {
  return nrThreadsD0;
}

inline unsigned int DedispersionConf::getNrItemsD0() const {
  return nrItemsD0;
}

inline unsigned int DedispersionConf::getNrThreadsD1() const {
  return nrThreadsD1;
}

inline unsigned int DedispersionConf::getNrItemsD1() const {
  return nrItemsD1;
}

inline unsigned int DedispersionConf::getUnroll() const {
  return unroll;
}

inline void DedispersionConf::setSplitSeconds(bool split) {
  splitSeconds = split;
}

inline void DedispersionConf::setLocalMem(bool local) {
  this->local = local;
}

inline void DedispersionConf::setNrThreadsD0(unsigned int threads) {
  nrThreadsD0 = threads;
}

inline void DedispersionConf::setNrItemsD0(unsigned int items) {
  nrItemsD0 = items;
}

inline void DedispersionConf::setNrThreadsD1(unsigned int threads) {
  nrThreadsD1 = threads;
}

inline void DedispersionConf::setNrItemsD1(unsigned int items) {
  nrItemsD1 = items;
}

inline void DedispersionConf::setUnroll(unsigned int unroll) {
  this->unroll = unroll;
}

template< typename I, typename O > std::string * getDedispersionOpenCL(const DedispersionConf & conf, const unsigned int padding, const uint8_t inputBits, const std::string & inputDataType, const std::string & intermediateDataType, const std::string & outputDataType, const AstroData::Observation & observation, std::vector< float > & shifts, const std::vector< uint8_t > & zappedChannels) {
  std::string * code = new std::string();
  std::string sum0_sTemplate = std::string();
  std::string sum_sTemplate = std::string();
  std::string unrolled_sTemplate = std::string();
  std::string firstDM_s = isa::utils::toString(observation.getFirstDM());
  if ( firstDM_s.find(".") == std::string::npos ) {
    firstDM_s.append(".0f");
  } else {
    firstDM_s.append("f");
  }
  std::string DMStep_s = isa::utils::toString(observation.getDMStep());
  if ( DMStep_s.find(".") == std::string::npos ) {
    DMStep_s.append(".0f");
  } else {
    DMStep_s.append("f");
  }
  std::string nrTotalSamplesPerBlock_s = isa::utils::toString(conf.getNrThreadsD0() * conf.getNrItemsD0());
  std::string nrTotalDMsPerBlock_s = isa::utils::toString(conf.getNrThreadsD1() * conf.getNrItemsD1());
  std::string nrTotalThreads_s = isa::utils::toString(conf.getNrThreadsD0() * conf.getNrThreadsD1());

  // Begin kernel's template
  if ( conf.getLocalMem() ) {
    if ( conf.getSplitSeconds() ) {
      *code = "__kernel void dedispersion(const unsigned int secondOffset, __global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts, __constant const uchar * restrict const zappedChannels) {\n"
        "unsigned int sampleOffset = secondOffset * " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ";\n";
    } else {
      *code = "__kernel void dedispersion(__global const " + inputDataType + " * restrict const input, __global const uchar * const restrict beamDriver, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts, __constant const uchar * restrict const zappedChannels) {\n";
    }
    *code +=  "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "unsigned int sBeam = get_group_id(2);\n"
      "unsigned int inShMem = 0;\n"
      "unsigned int inGlMem = 0;\n"
      "<%DEFS%>"
      "__local " + intermediateDataType + " buffer[" + isa::utils::toString((conf.getNrThreadsD0() * conf.getNrItemsD0()) + static_cast< unsigned int >(shifts[0] * (observation.getFirstDM() + (((conf.getNrThreadsD1() * conf.getNrItemsD1()) - 1) * observation.getDMStep())))) + "];\n";
    if ( inputBits < 8 ) {
      *code += inputDataType + " bitsBuffer;\n"
        "unsigned int byte = 0;\n"
        "uchar firstBit = 0;\n"
        + inputDataType + " interBuffer;\n";
    }
    *code += "\n"
      "for ( unsigned int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(conf.getUnroll()) + " ) {\n"
      "unsigned int minShift = 0;\n"
      "<%DEFS_SHIFT%>"
      "unsigned int diffShift = 0;\n"
      "\n"
      "<%UNROLLED_LOOP%>"
      "}\n";
      if ( zappedChannels[observation.getNrChannels() - 1] == 0 ) {
        *code += "inShMem = (get_local_id(1) * " + isa::utils::toString(conf.getNrThreadsD0()) + ") + get_local_id(0);\n"
        "inGlMem = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem;\n"
        "while ( inShMem < " + nrTotalSamplesPerBlock_s + " ) {\n";
      if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
        if ( conf.getSplitSeconds() ) {
          *code += "buffer[inShMem] = input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + inGlMem];\n";
        } else {
          *code += "buffer[inShMem] = input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + " + isa::utils::toString(observation.getNrChannels() - 1) + "] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + (" + isa::utils::toString(observation.getNrChannels() - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + inGlMem];\n";
        }
      } else if ( inputBits < 8 ) {
        if ( conf.getSplitSeconds() ) {
          *code += "interBuffer = 0;\n"
            "byte = (inGlMem / " + isa::utils::toString(8 / inputBits) + ");\n"
            "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
            "bitsBuffer = input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
        } else {
          *code += "interBuffer = 0;\n"
            "byte = (inGlMem / " + isa::utils::toString(8 / inputBits) + ");\n"
            "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
            "bitsBuffer = input[(" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
        }
        for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
          *code += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), isa::utils::toString(bit));
        }
        if ( inputDataType == "char" ) {
          for ( unsigned int bit = inputBits; bit < 8; bit++ ) {
            *code += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(inputBits - 1)), isa::utils::toString(bit));
          }
        }
        *code += "buffer[inShMem] = convert_" + intermediateDataType + "(interBuffer);\n";
      } else {
        if ( conf.getSplitSeconds() ) {
          *code += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + inGlMem]);\n";
        } else {
          *code += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + " + isa::utils::toString(observation.getNrChannels() - 1) + "] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + inGlMem]);\n";
        }
      }
      *code += "inShMem += " + nrTotalThreads_s + ";\n"
        "inGlMem += " + nrTotalThreads_s + ";\n"
        "}\n"
        "barrier(CLK_LOCAL_MEM_FENCE);\n"
        "\n"
        "<%SUM0%>"
        "\n";
      }
      *code += "<%STORES%>"
      "}";
    unrolled_sTemplate = "if ( zappedChannels[channel + <%UNROLL%>] == 0 ) {\n"
      "minShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") * " + DMStep_s + ")));\n"
      "<%SHIFTS%>"
      "diffShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + (((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + isa::utils::toString((conf.getNrThreadsD1() * conf.getNrItemsD1()) - 1) + ") * " + DMStep_s + "))) - minShift;\n"
      "\n"
      "inShMem = (get_local_id(1) * " + isa::utils::toString(conf.getNrThreadsD0()) + ") + get_local_id(0);\n";
    if ( conf.getSplitSeconds() ) {
      unrolled_sTemplate += "inGlMem = sampleOffset + ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n";
    } else {
      unrolled_sTemplate += "inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n";
    }
    unrolled_sTemplate += "while ( inShMem < (" + nrTotalSamplesPerBlock_s + " + diffShift) ) {\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitSeconds() ) {
        unrolled_sTemplate += "buffer[inShMem] = input[(((inGlMem / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + (inGlMem % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")];\n";
      } else {
        unrolled_sTemplate += "buffer[inShMem] = input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + (channel + <%UNROLL%>)] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + inGlMem];\n";
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitSeconds() ) {
        unrolled_sTemplate += "interBuffer = 0;\n"
          "byte = inGlMem % " + isa::utils::toString(observation.getNrSamplesPerSecond() / (8 / inputBits)) + ";\n"
          "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[(((inGlMem / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      } else {
        unrolled_sTemplate += "interBuffer = 0;\n"
          "byte = (inGlMem / " + isa::utils::toString(8 / inputBits) + ");\n"
          "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        unrolled_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), isa::utils::toString(bit));
      }
      if ( inputDataType == "char" ) {
        for ( unsigned int bit = inputBits; bit < 8; bit++ ) {
          unrolled_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(inputBits - 1)), isa::utils::toString(bit));
        }
      }
      unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(interBuffer);\n";
    } else {
      if ( conf.getSplitSeconds() ) {
        unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(((inGlMem / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + (inGlMem % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")]);\n";
      } else {
        unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + (channel + <%UNROLL%>)] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + inGlMem]);\n";
      }
    }
    unrolled_sTemplate += "inShMem += " + nrTotalThreads_s + ";\n"
      "inGlMem += " + nrTotalThreads_s + ";\n"
      "}\n"
      "barrier(CLK_LOCAL_MEM_FENCE);\n"
      "\n"
      "<%SUMS%>"
      "\n";
    if ( conf.getUnroll() > 1 ) {
      unrolled_sTemplate += "barrier(CLK_LOCAL_MEM_FENCE);\n";
    }
    unrolled_sTemplate += "}\n";
    sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[get_local_id(0) + <%OFFSET%>];\n";
    sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + shiftDM<%DM_NUM%>];\n";
  } else {
    if ( conf.getSplitSeconds() ) {
      *code = "__kernel void dedispersion(const unsigned int secondOffset, __global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts, __constant const uchar * restrict const zappedChannels) {\n"
        "unsigned int sampleOffset = secondOffset * " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ";\n";
    } else {
      *code = "__kernel void dedispersion(__global const " + inputDataType + " * restrict const input, __global const uchar * restrict const beamDriver, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts, __constant const uchar * restrict const zappedChannels) {\n";
    }
    *code += "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "unsigned int sBeam = get_group_id(2);\n"
      "<%DEFS%>";
    if ( inputBits < 8 ) {
      *code += inputDataType + " bitsBuffer;\n"
        "unsigned int byte = 0;\n"
        "uchar firstBit = 0;\n"
        + inputDataType + " interBuffer;\n";
    }
    *code += "\n"
      "for ( unsigned int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(conf.getUnroll()) + " ) {\n"
      "<%DEFS_SHIFT%>"
      "<%UNROLLED_LOOP%>"
      "}\n";
    if ( zappedChannels[observation.getNrChannels() - 1] == 0 ) {
      *code += "<%SUM0%>"
      "\n";
    }
    *code += "<%STORES%>"
      "}";
    unrolled_sTemplate = "if ( zappedChannels[channel + <%UNROLL%>] == 0 ) {\n"
      "<%SHIFTS%>"
      "\n"
      "<%SUMS%>"
      "}\n"
      "\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitSeconds() ) {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + sample + <%OFFSET%>];\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[((((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + ((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")];\n";
      } else {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + " + isa::utils::toString(observation.getNrChannels() - 1) + "] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + sample + <%OFFSET%>];\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + (channel + <%UNROLL%>)] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)];\n";
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitSeconds() ) {
        sum0_sTemplate = "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%>) / " + isa::utils::toString(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
        sum_sTemplate =  "interBuffer = 0;\n"
          "byte = (sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(observation.getNrSamplesPerSecond() / (8 / inputBits)) + ";\n"
          "firstBit = ((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[((((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      } else {
        sum0_sTemplate = "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%>) / " + isa::utils::toString(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[(" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
        sum_sTemplate =  "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        sum0_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), isa::utils::toString(bit));
        sum_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), isa::utils::toString(bit));
      }
      if ( inputDataType == "char" ) {
        for ( unsigned int bit = inputBits; bit < 8; bit++ ) {
          sum0_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(inputBits - 1)), isa::utils::toString(bit));
          sum_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(inputBits - 1)), isa::utils::toString(bit));
        }
      }
      sum0_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(interBuffer);\n";
      sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(interBuffer);\n";
    } else {
      if ( conf.getSplitSeconds() ) {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + sample + <%OFFSET%>]);\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[((((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(I))) + ") + ((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")]);\n";
      } else {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + " + isa::utils::toString(observation.getNrChannels() - 1) + "] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + sample + <%OFFSET%>]);\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[(beamDriver[(sBeam * " + isa::utils::toString(observation.getNrPaddedChannels(padding / sizeof(uint8_t))) + ") + (channel + <%UNROLL%>)] * " + isa::utils::toString(observation.getNrChannels() * observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(I))) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)]);\n";
      }
    }
  }
	std::string def_sTemplate = intermediateDataType + " dedispersedSample<%NUM%>DM<%DM_NUM%> = 0;\n";
  std::string defsShiftTemplate = "unsigned int shiftDM<%DM_NUM%> = 0;\n";
  std::string shiftsTemplate;
  if ( conf.getLocalMem() ) {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + DMStep_s + "))) - minShift;\n";
  } else {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + DMStep_s + ")));\n";
  }
  std::string store_sTemplate;
  if ( intermediateDataType == outputDataType ) {
    store_sTemplate = "output[(sBeam * " + isa::utils::toString(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(O))) + ") + ((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(O))) + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
  } else {
    store_sTemplate = "output[(sBeam * " + isa::utils::toString(observation.getNrDMs() * observation.getNrSamplesPerPaddedSecond(padding / sizeof(O))) + ") + ((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond(padding / sizeof(O))) + ") + (sample + <%OFFSET%>)] = convert_" + outputDataType + "(dedispersedSample<%NUM%>DM<%DM_NUM%>);\n";
  }
	// End kernel's template

  std::string * def_s =  new std::string();
  std::string * defsShift_s = new std::string();
  std::string * sum0_s = new std::string();
  std::string * unrolled_s = new std::string();
  std::string * store_s =  new std::string();

  for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
    std::string dm_s = isa::utils::toString(dm);
    std::string * temp_s = 0;

    temp_s = isa::utils::replace(&defsShiftTemplate, "<%DM_NUM%>", dm_s);
    defsShift_s->append(*temp_s);
    delete temp_s;
  }
  for ( unsigned int sample = 0; sample < conf.getNrItemsD0(); sample++ ) {
    std::string sample_s = isa::utils::toString(sample);
    std::string offset_s = isa::utils::toString(sample * conf.getNrThreadsD0());
    std::string * def_sDM =  new std::string();
    std::string * store_sDM =  new std::string();

    for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
      std::string dm_s = isa::utils::toString(dm);
      std::string * temp_s = 0;

      temp_s = isa::utils::replace(&def_sTemplate, "<%DM_NUM%>", dm_s);
      def_sDM->append(*temp_s);
      delete temp_s;
      temp_s = isa::utils::replace(&sum0_sTemplate, "<%DM_NUM%>", dm_s);
      sum0_s->append(*temp_s);
      delete temp_s;
      temp_s = isa::utils::replace(&store_sTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = isa::utils::toString(dm * conf.getNrThreadsD1());
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", offset_s, true);
      }
      store_sDM->append(*temp_s);
      delete temp_s;
    }
    def_sDM = isa::utils::replace(def_sDM, "<%NUM%>", sample_s, true);
    def_s->append(*def_sDM);
    delete def_sDM;
    sum0_s = isa::utils::replace(sum0_s, "<%NUM%>", sample_s, true);
    if ( sample * conf.getNrThreadsD0() == 0 ) {
      std::string empty_s;
      sum0_s = isa::utils::replace(sum0_s, " + <%OFFSET%>", empty_s, true);
    } else {
      sum0_s = isa::utils::replace(sum0_s, "<%OFFSET%>", offset_s, true);
    }
    store_sDM = isa::utils::replace(store_sDM, "<%NUM%>", sample_s, true);
    if ( sample * conf.getNrThreadsD0() == 0 ) {
      std::string empty_s;
      store_sDM = isa::utils::replace(store_sDM, " + <%OFFSET%>", empty_s, true);
    } else {
      store_sDM = isa::utils::replace(store_sDM, "<%OFFSET%>", offset_s, true);
    }
    store_s->append(*store_sDM);
    delete store_sDM;
  }
  for ( unsigned int loop = 0; loop < conf.getUnroll(); loop++ ) {
    std::string loop_s = isa::utils::toString(loop);
    std::string * temp_s = 0;
    std::string * shifts_s = new std::string();
    std::string * sums_s = new std::string();

    if ( loop == 0 ) {
      std::string empty_s;
      temp_s = isa::utils::replace(&unrolled_sTemplate, " + <%UNROLL%>", empty_s);
    } else {
      temp_s = isa::utils::replace(&unrolled_sTemplate, "<%UNROLL%>", loop_s);
    }
    unrolled_s->append(*temp_s);
    delete temp_s;
    for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
      std::string dm_s = isa::utils::toString(dm);

      temp_s = isa::utils::replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = isa::utils::toString(dm * conf.getNrThreadsD1());
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", offset_s, true);
      }
      if ( loop == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%UNROLL%>", empty_s, true);
      } else {
        temp_s = isa::utils::replace(temp_s, "<%UNROLL%>", loop_s, true);
      }
      shifts_s->append(*temp_s);
      delete temp_s;
    }
    unrolled_s = isa::utils::replace(unrolled_s, "<%SHIFTS%>", *shifts_s, true);
    delete shifts_s;
    for ( unsigned int sample = 0; sample < conf.getNrItemsD0(); sample++ ) {
      std::string sample_s = isa::utils::toString(sample);
      std::string offset_s = isa::utils::toString(sample * conf.getNrThreadsD0());
      std::string * sumsDM_s = new std::string();

      for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
        std::string dm_s = isa::utils::toString(dm);

        temp_s = isa::utils::replace(&sum_sTemplate, "<%DM_NUM%>", dm_s);
        if ( loop == 0 ) {
          std::string empty_s;
          temp_s = isa::utils::replace(temp_s, " + <%UNROLL%>", empty_s, true);
        } else {
          temp_s = isa::utils::replace(temp_s, "<%UNROLL%>", loop_s, true);
        }
        sumsDM_s->append(*temp_s);
        delete temp_s;
      }
      sumsDM_s = isa::utils::replace(sumsDM_s, "<%NUM%>", sample_s, true);
      if ( sample * conf.getNrThreadsD0() == 0 ) {
        std::string empty_s;
        sumsDM_s = isa::utils::replace(sumsDM_s, " + <%OFFSET%>", empty_s, true);
      } else {
        sumsDM_s = isa::utils::replace(sumsDM_s, "<%OFFSET%>", offset_s, true);
      }
      sums_s->append(*sumsDM_s);
      delete sumsDM_s;
    }
    unrolled_s = isa::utils::replace(unrolled_s, "<%SUMS%>", *sums_s, true);
    if ( conf.getUnroll() == 0 ) {
      std::string empty_s;
      unrolled_s = isa::utils::replace(unrolled_s, " + <%UNROLL%>", empty_s, true);
    } else {
      unrolled_s = isa::utils::replace(unrolled_s, "<%UNROLL%>", loop_s, true);
    }
    delete sums_s;
  }
  code = isa::utils::replace(code, "<%DEFS%>", *def_s, true);
  code = isa::utils::replace(code, "<%DEFS_SHIFT%>", *defsShift_s, true);
  code = isa::utils::replace(code, "<%SUM0%>", *sum0_s, true);
  code = isa::utils::replace(code, "<%UNROLLED_LOOP%>", *unrolled_s, true);
  code = isa::utils::replace(code, "<%STORES%>", *store_s, true);
  delete def_s;
  delete defsShift_s;
  delete sum0_s;
  delete unrolled_s;
  delete store_s;

  return code;
}

} // PulsarSearch

#endif // DEDISPERSION_HPP

