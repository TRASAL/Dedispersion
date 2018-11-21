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

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cstdint>

#include <Kernel.hpp>
#include <Observation.hpp>
#include <utils.hpp>
#include <Bits.hpp>
#include <Platform.hpp>


#pragma once

namespace Dedispersion {

class DedispersionConf : public isa::OpenCL::KernelConf {
public:
  DedispersionConf();
  ~DedispersionConf();

  // Get
  bool getSplitBatches() const;
  bool getLocalMem() const;
  unsigned int getUnroll() const;
  // Set
  void setSplitBatches(bool split);
  void setLocalMem(bool local);
  void setUnroll(unsigned int unroll);
  // Utils
  std::string print() const;

private:
  bool splitBatches;
  bool local;
  unsigned int unroll;
};

typedef std::map< std::string, std::map< unsigned int, Dedispersion::DedispersionConf * > * > tunedDedispersionConf;

// Sequential
template< typename I, typename L, typename O > void dedispersion(AstroData::Observation & observation, const std::vector<unsigned int> & zappedChannels, const std::vector<unsigned int> & beamMapping, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits);
template< typename I, typename L, typename O > void subbandDedispersionStepOne(AstroData::Observation & observation, const std::vector<unsigned int> & zappedChannels, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits);
template< typename I, typename L, typename O > void subbandDedispersionStepTwo(AstroData::Observation & observation, const std::vector<unsigned int> & beamMapping, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding);
// OpenCL
template< typename I, typename O > std::string * getDedispersionOpenCL(const DedispersionConf & conf, const unsigned int padding, const uint8_t inputBits, const std::string & inputDataType, const std::string & intermediateDataType, const std::string & outputDataType, const AstroData::Observation & observation, std::vector< float > & shifts);
template< typename I, typename O > std::string * getSubbandDedispersionStepOneOpenCL(const DedispersionConf & conf, const unsigned int padding, const uint8_t inputBits, const std::string & inputDataType, const std::string & intermediateDataType, const std::string & outputDataType, const AstroData::Observation & observation, std::vector< float > & shifts);
template< typename I > std::string * getSubbandDedispersionStepTwoOpenCL(const DedispersionConf & conf, const unsigned int padding, const std::string & inputDataType, const AstroData::Observation & observation, std::vector< float > & shifts);
void readTunedDedispersionConf(tunedDedispersionConf & tunedDedispersion, const std::string & dedispersionFilename);


// Implementations
template< typename I, typename L, typename O > void dedispersion(AstroData::Observation & observation, const std::vector<unsigned int> & zappedChannels, const std::vector<unsigned int> & beamMapping, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits)
{
  for ( unsigned int sBeam = 0; sBeam < observation.getNrSynthesizedBeams(); sBeam++ )
  {
    for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ )
    {
      for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch() / observation.getDownsampling(); sample++ )
      {
        L dedispersedSample = static_cast< L >(0);
        for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ )
        {
          unsigned int shift = static_cast< unsigned int >((observation.getFirstDM() + (dm * observation.getDMStep())) * shifts[channel]);
          // TODO: zapping should be per beam
          if ( zappedChannels[channel] != 0 )
          {
            // If a channel is zapped, skip it
            continue;
          }
          if ( inputBits >= 8 )
          {
            dedispersedSample += static_cast< L >(input[(beamMapping[(sBeam * observation.getNrChannels(padding / sizeof(unsigned int))) + channel] * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + (sample + shift)]);
          }
          else
          {
            unsigned int byte = (sample + shift) / (8 / inputBits);
            uint8_t firstBit = ((sample + shift) % (8 / inputBits)) * inputBits;
            char value = 0;
            char buffer = input[(beamMapping[(sBeam * observation.getNrChannels(padding / sizeof(unsigned int))) + channel]) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(I))) + byte];
            for ( uint8_t bit = 0; bit < inputBits; bit++ )
            {
              isa::utils::setBit(value, isa::utils::getBit(buffer, firstBit + bit), bit);
            }
            dedispersedSample += static_cast< L >(value);
          }
        }
        output[(sBeam * observation.getNrDMs() * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(O))) + (dm * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(O))) + sample] = static_cast< O >(dedispersedSample);
      }
    }
  }
}

template< typename I, typename L, typename O > void subbandDedispersionStepOne(AstroData::Observation & observation, const std::vector<unsigned int> & zappedChannels, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding, const uint8_t inputBits)
{
  for ( unsigned int beam = 0; beam < observation.getNrBeams(); beam++ )
  {
    for ( unsigned int dm = 0; dm < observation.getNrDMs(true); dm++ )
    {
      for ( unsigned int subband = 0; subband < observation.getNrSubbands(); subband++ )
      {
        for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch(true) / observation.getDownsampling(); sample++ )
        {
          L dedispersedSample = static_cast< L >(0);
          for ( unsigned int channel = subband * observation.getNrChannelsPerSubband(); channel < (subband + 1) * observation.getNrChannelsPerSubband(); channel++ )
          {
            unsigned int shift = static_cast< unsigned int >((observation.getFirstDM(true) + (dm * observation.getDMStep(true))) * (shifts[channel] - shifts[((subband + 1) * observation.getNrChannelsPerSubband()) - 1]));

            if ( zappedChannels[channel] != 0 )
            {
              // If a channel is zapped, skip it
              continue;
            }
            if ( inputBits >= 8 )
            {
              dedispersedSample += static_cast< L >(input[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + (sample + shift)]);
            }
            else
            {
              unsigned int byte = (sample + shift) / (8 / inputBits);
              uint8_t firstBit = ((sample + shift) % (8 / inputBits)) * inputBits;
              char value = 0;
              char buffer = input[(beam * observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(I))) + (channel * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(I))) + byte];
              for ( uint8_t bit = 0; bit < inputBits; bit++ )
              {
                isa::utils::setBit(value, isa::utils::getBit(buffer, firstBit + bit), bit);
              }
              dedispersedSample += static_cast< L >(value);
            }
          }
          output[(beam * observation.getNrDMs(true) * observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / (8 / inputBits), padding / sizeof(O))) + (dm * observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / (8 / inputBits), padding / sizeof(O))) + (subband * isa::utils::pad(observation.getNrSamplesPerBatch(true) / (8 / inputBits), padding / sizeof(O))) + sample] = static_cast< O >(dedispersedSample);
        }
      }
    }
  }
}

template< typename I, typename L, typename O > void subbandDedispersionStepTwo(AstroData::Observation & observation, const std::vector<unsigned int> & beamMapping, const std::vector< I > & input, std::vector< O > & output, const std::vector< float > & shifts, const unsigned int padding)
{
  for ( unsigned int sBeam = 0; sBeam < observation.getNrSynthesizedBeams(); sBeam++ )
  {
    for ( unsigned int firstStepDM = 0; firstStepDM < observation.getNrDMs(true); firstStepDM++ )
    {
      for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ )
      {
        for ( unsigned int sample = 0; sample < observation.getNrSamplesPerBatch() / observation.getDownsampling(); sample++ )
        {
          L dedispersedSample = static_cast< L >(0);
          for ( unsigned int channel = 0; channel < observation.getNrSubbands(); channel++ )
          {
            unsigned int shift = static_cast< unsigned int >((observation.getFirstDM() + (dm * observation.getDMStep())) * shifts[channel]);
            dedispersedSample += static_cast< L >(input[(beamMapping[(sBeam * observation.getNrSubbands(padding / sizeof(unsigned int))) + channel] * observation.getNrDMs(true) * observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + (firstStepDM * observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + (channel * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + (sample + shift)]);
          }
          output[(sBeam * (observation.getNrDMs(true) * observation.getNrDMs()) * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(O))) + (((firstStepDM * observation.getNrDMs()) + dm) * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(O))) + sample] = static_cast< O >(dedispersedSample);
        }
      }
    }
  }
}

inline bool DedispersionConf::getSplitBatches() const {
  return splitBatches;
}

inline bool DedispersionConf::getLocalMem() const {
  return local;
}

inline unsigned int DedispersionConf::getUnroll() const {
  return unroll;
}

inline void DedispersionConf::setSplitBatches(bool split) {
  splitBatches = split;
}

inline void DedispersionConf::setLocalMem(bool local) {
  this->local = local;
}

inline void DedispersionConf::setUnroll(unsigned int unroll) {
  this->unroll = unroll;
}

// TODO: splitBatches mode
template< typename I, typename O > std::string * getDedispersionOpenCL(const DedispersionConf & conf, const unsigned int padding, const uint8_t inputBits, const std::string & inputDataType, const std::string & intermediateDataType, const std::string & outputDataType, const AstroData::Observation & observation, std::vector< float > & shifts)
{
  std::string * code = new std::string();
  std::string sum_sTemplate = std::string();
  std::string unrolled_sTemplate = std::string();
  std::string firstDM_s = std::to_string(observation.getFirstDM());
  if ( firstDM_s.find(".") == std::string::npos ) {
    firstDM_s.append(".0f");
  } else {
    firstDM_s.append("f");
  }
  std::string DMStep_s = std::to_string(observation.getDMStep());
  if ( DMStep_s.find(".") == std::string::npos ) {
    DMStep_s.append(".0f");
  } else {
    DMStep_s.append("f");
  }
  std::string nrTotalSamplesPerBlock_s = std::to_string(conf.getNrThreadsD0() * conf.getNrItemsD0());
  std::string nrTotalDMsPerBlock_s = std::to_string(conf.getNrThreadsD1() * conf.getNrItemsD1());
  std::string nrTotalThreads_s = std::to_string(conf.getNrThreadsD0() * conf.getNrThreadsD1());

  // Begin kernel's template
  if ( conf.getLocalMem() ) {
    if ( conf.getSplitBatches() ) {
    } else {
      *code = "__kernel void dedispersion(__global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __global const unsigned int * const restrict beamMapping, __constant const unsigned int * restrict const zappedChannels, __constant const float * restrict const shifts) {\n";
    }
    *code +=  "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "unsigned int sBeam = get_group_id(2);\n"
      "unsigned int inShMem = 0;\n"
      "unsigned int inGlMem = 0;\n"
      "<%DEFS%>"
      "__local " + intermediateDataType + " buffer[" + std::to_string((conf.getNrThreadsD0() * conf.getNrItemsD0()) + static_cast< unsigned int >(shifts[0] * (observation.getFirstDM() + ((conf.getNrThreadsD1() * conf.getNrItemsD1()) * observation.getDMStep())))) + "];\n";
    if ( inputBits < 8 ) {
      *code += inputDataType + " bitsBuffer;\n"
        "unsigned int byte = 0;\n"
        "uchar firstBit = 0;\n"
        + inputDataType + " interBuffer;\n";
    }
    *code += "\n"
      "for ( unsigned int channel = 0; channel < " + std::to_string(observation.getNrChannels()) + "; channel += " + std::to_string(conf.getUnroll()) + " ) {\n"
      "unsigned int minShift = 0;\n"
      "<%DEFS_SHIFT%>"
      "unsigned int diffShift = 0;\n"
      "\n"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "if ( zappedChannels[channel + <%UNROLL%>] == 0 ) {\n"
      "minShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") * " + DMStep_s + ")));\n"
      "<%SHIFTS%>"
      "diffShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + (((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + std::to_string((conf.getNrThreadsD1() * conf.getNrItemsD1()) - 1) + ") * " + DMStep_s + "))) - minShift;\n"
      "\n"
      "inShMem = (get_local_id(1) * " + std::to_string(conf.getNrThreadsD0()) + ") + get_local_id(0);\n";
    if ( conf.getSplitBatches() ) {
    } else {
      unrolled_sTemplate += "inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n";
    }
    unrolled_sTemplate += "while ( (inShMem < (" + nrTotalSamplesPerBlock_s + " + diffShift) && (inGlMem < " + std::to_string(observation.getNrSamplesPerDispersedBatch() / observation.getDownsampling()) + ")) ) {\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitBatches() ) {
      } else {
        unrolled_sTemplate += "buffer[inShMem] = input[(beamMapping[(sBeam * " + std::to_string(observation.getNrChannels(padding / sizeof(unsigned int))) + ") + (channel + <%UNROLL%>)] * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + inGlMem];\n";
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitBatches() ) {
      } else {
        unrolled_sTemplate += "interBuffer = 0;\n"
          "byte = (inGlMem / " + std::to_string(8 / inputBits) + ");\n"
          "firstBit = ((inGlMem % " + std::to_string(8 / inputBits) + ") * " + std::to_string(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        unrolled_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(bit)), std::to_string(bit));
      }
      if ( inputDataType == "char" || inputDataType == "uchar" ) {
        for ( unsigned int bit = inputBits; bit < 8; bit++ ) {
          unrolled_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(inputBits - 1)), std::to_string(bit));
        }
      }
      unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(interBuffer);\n";
    } else {
      if ( conf.getSplitBatches() ) {
      } else {
        unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(beamMapping[(sBeam * " + std::to_string(observation.getNrChannels(padding / sizeof(unsigned int))) + ") + (channel + <%UNROLL%>)] * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + inGlMem]);\n";
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
    if ( (observation.getNrSamplesPerBatch() % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
      sum_sTemplate += "if ( sample + <%OFFSET%> < " + std::to_string(observation.getNrSamplesPerBatch()) + " ) {\n";
    }
    sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + shiftDM<%DM_NUM%>];\n";
    if ( (observation.getNrSamplesPerBatch() % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
      sum_sTemplate += "}\n";
    }
  } else {
    if ( conf.getSplitBatches() ) {
    } else {
      *code = "__kernel void dedispersion(__global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __global const unsigned int * restrict const beamMapping,  __constant const unsigned int * restrict const zappedChannels, __constant const float * restrict const shifts) {\n";
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
      "for ( unsigned int channel = 0; channel < " + std::to_string(observation.getNrChannels()) + "; channel += " + std::to_string(conf.getUnroll()) + " ) {\n"
      "<%DEFS_SHIFT%>"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "if ( zappedChannels[channel + <%UNROLL%>] == 0 ) {\n"
      "<%SHIFTS%>"
      "\n"
      "<%SUMS%>"
      "}\n"
      "\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitBatches() ) {
      } else {
        if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch() / observation.getDownsampling()) + " ) {\n";
        }
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(beamMapping[(sBeam * " + std::to_string(observation.getNrChannels(padding / sizeof(unsigned int))) + ") + (channel + <%UNROLL%>)] * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)];\n";
        if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "}\n";
        }
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitBatches() ) {
      } else {
        if ( (observation.getNrSamplesPerBatch() % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch()) + " ) {\n";
        }
        sum_sTemplate += "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + std::to_string(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + std::to_string(8 / inputBits) + ") * " + std::to_string(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch() / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        sum_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(bit)), std::to_string(bit));
      }
      if ( inputDataType == "char" || inputDataType == "uchar" ) {
        for ( unsigned int bit = inputBits; bit < 8; bit++ ) {
          sum_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(inputBits - 1)), std::to_string(bit));
        }
      }
      sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(interBuffer);\n";
      if ( (observation.getNrSamplesPerBatch() % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
        sum_sTemplate += "}\n";
      }
    } else {
      if ( conf.getSplitBatches() ) {
      } else {
        if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch()) + " ) {\n";
        }
        sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[(beamMapping[(sBeam * " + std::to_string(observation.getNrChannels(padding / sizeof(unsigned int))) + ") + (channel + <%UNROLL%>)] * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(), padding / sizeof(I))) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)]);\n";
        if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "}\n";
        }
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
  if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
    store_sTemplate += "if ( sample + <%OFFSET%> < " + std::to_string(observation.getNrSamplesPerBatch() / observation.getDownsampling()) + " ) {\n";
  }
  if ( intermediateDataType == outputDataType ) {
    store_sTemplate += "output[(sBeam * " + std::to_string(observation.getNrDMs() * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(I))) + ") + ((dm + <%DM_OFFSET%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(I))) + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
  } else {
    store_sTemplate += "output[(sBeam * " + std::to_string(observation.getNrDMs() * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(I))) + ") + ((dm + <%DM_OFFSET%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(I))) + ") + (sample + <%OFFSET%>)] = convert_" + outputDataType + "(dedispersedSample<%NUM%>DM<%DM_NUM%>);\n";
  }
  if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
    store_sTemplate += "}\n";
  }
  // End kernel's template

  std::string * def_s =  new std::string();
  std::string * defsShift_s = new std::string();
  std::string * unrolled_s = new std::string();
  std::string * store_s =  new std::string();

  for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
    std::string dm_s = std::to_string(dm);
    std::string * temp_s = 0;

    temp_s = isa::utils::replace(&defsShiftTemplate, "<%DM_NUM%>", dm_s);
    defsShift_s->append(*temp_s);
    delete temp_s;
  }
  for ( unsigned int sample = 0; sample < conf.getNrItemsD0(); sample++ ) {
    std::string sample_s = std::to_string(sample);
    std::string offset_s = std::to_string(sample * conf.getNrThreadsD0());
    std::string * def_sDM =  new std::string();
    std::string * store_sDM =  new std::string();

    for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
      std::string dm_s = std::to_string(dm);
      std::string * temp_s = 0;

      temp_s = isa::utils::replace(&def_sTemplate, "<%DM_NUM%>", dm_s);
      def_sDM->append(*temp_s);
      delete temp_s;
      temp_s = isa::utils::replace(&store_sTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = std::to_string(dm * conf.getNrThreadsD1());
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", offset_s, true);
      }
      store_sDM->append(*temp_s);
      delete temp_s;
    }
    def_sDM = isa::utils::replace(def_sDM, "<%NUM%>", sample_s, true);
    def_s->append(*def_sDM);
    delete def_sDM;
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
    std::string loop_s = std::to_string(loop);
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
      std::string dm_s = std::to_string(dm);

      temp_s = isa::utils::replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = std::to_string(dm * conf.getNrThreadsD1());
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
      std::string sample_s = std::to_string(sample);
      std::string offset_s = std::to_string(sample * conf.getNrThreadsD0());
      std::string * sumsDM_s = new std::string();

      for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
        std::string dm_s = std::to_string(dm);

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
  code = isa::utils::replace(code, "<%UNROLLED_LOOP%>", *unrolled_s, true);
  code = isa::utils::replace(code, "<%STORES%>", *store_s, true);
  delete def_s;
  delete defsShift_s;
  delete unrolled_s;
  delete store_s;

  return code;
}

// TODO: splitBatches mode
template< typename I, typename O > std::string * getSubbandDedispersionStepOneOpenCL(const DedispersionConf & conf, const unsigned int padding, const uint8_t inputBits, const std::string & inputDataType, const std::string & intermediateDataType, const std::string & outputDataType, const AstroData::Observation & observation, std::vector< float > & shifts)
{
  std::string * code = new std::string();
  std::string sum_sTemplate = std::string();
  std::string unrolled_sTemplate = std::string();
  std::string firstDM_s = std::to_string(observation.getFirstDM(true));
  if ( firstDM_s.find(".") == std::string::npos ) {
    firstDM_s.append(".0f");
  } else {
    firstDM_s.append("f");
  }
  std::string DMStep_s = std::to_string(observation.getDMStep(true));
  if ( DMStep_s.find(".") == std::string::npos ) {
    DMStep_s.append(".0f");
  } else {
    DMStep_s.append("f");
  }
  std::string nrTotalSamplesPerBlock_s = std::to_string(conf.getNrThreadsD0() * conf.getNrItemsD0());
  std::string nrTotalDMsPerBlock_s = std::to_string(conf.getNrThreadsD1() * conf.getNrItemsD1());
  std::string nrTotalThreads_s = std::to_string(conf.getNrThreadsD0() * conf.getNrThreadsD1());

  // Begin kernel's template
  if ( conf.getLocalMem() ) {
    if ( conf.getSplitBatches() ) {
    } else {
      *code = "__kernel void dedispersionStepOne(__global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const uchar * restrict const zappedChannels, __constant const float * restrict const shifts) {\n";
    }
    *code += "unsigned int beam = get_group_id(2) / " + std::to_string(observation.getNrSubbands())  + ";\n"
      "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int subband = get_group_id(2) % " + std::to_string(observation.getNrSubbands())  + ";\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "unsigned int inShMem = 0;\n"
      "unsigned int inGlMem = 0;\n"
      "<%DEFS%>"
      "__local " + intermediateDataType + " buffer[" + std::to_string((conf.getNrThreadsD0() * conf.getNrItemsD0()) + static_cast< unsigned int >(shifts[0] * (observation.getFirstDM(true) + ((conf.getNrThreadsD1() * conf.getNrItemsD1()) * observation.getDMStep(true))))) + "];\n";
    if ( inputBits < 8 ) {
      *code += inputDataType + " bitsBuffer;\n"
        "unsigned int byte = 0;\n"
        "uchar firstBit = 0;\n"
        + inputDataType + " interBuffer;\n";
    }
    *code += "\n"
      "for ( unsigned int channel = subband * " + std::to_string(observation.getNrChannelsPerSubband()) + "; channel < (subband + 1) * " + std::to_string(observation.getNrChannelsPerSubband()) + "; channel += " + std::to_string(conf.getUnroll()) + " ) {\n"
      "unsigned int minShift = 0;\n"
      "<%DEFS_SHIFT%>"
      "unsigned int diffShift = 0;\n"
      "\n"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "if ( zappedChannels[channel + <%UNROLL%>] == 0 ) {\n"
      "minShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") * " + DMStep_s + ")));\n"
      "<%SHIFTS%>"
      "diffShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + (((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + std::to_string((conf.getNrThreadsD1() * conf.getNrItemsD1()) - 1) + ") * " + DMStep_s + "))) - minShift;\n"
      "\n"
      "inShMem = (get_local_id(1) * " + std::to_string(conf.getNrThreadsD0()) + ") + get_local_id(0);\n";
    if ( conf.getSplitBatches() ) {
    } else {
      unrolled_sTemplate += "inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n";
    }
    unrolled_sTemplate += "while ( (inShMem < (" + nrTotalSamplesPerBlock_s + " + diffShift) && (inGlMem < " + std::to_string(observation.getNrSamplesPerDispersedBatch(true) / observation.getDownsampling()) + ")) ) {\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitBatches() ) {
      } else {
        unrolled_sTemplate += "buffer[inShMem] = input[(beam * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + inGlMem];\n";
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitBatches() ) {
      } else {
        unrolled_sTemplate += "interBuffer = 0;\n"
          "byte = (inGlMem / " + std::to_string(8 / inputBits) + ");\n"
          "firstBit = ((inGlMem % " + std::to_string(8 / inputBits) + ") * " + std::to_string(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[(beam * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        unrolled_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(bit)), std::to_string(bit));
      }
      if ( inputDataType == "char" || inputDataType == "uchar" ) {
        for ( unsigned int bit = inputBits; bit < 8; bit++ ) {
          unrolled_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(inputBits - 1)), std::to_string(bit));
        }
      }
      unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(interBuffer);\n";
    } else {
      if ( conf.getSplitBatches() ) {
      } else {
        unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(beam * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + inGlMem]);\n";
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
    if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
      sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch(true)) + " ) {\n";
    }
    sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + shiftDM<%DM_NUM%>];\n";
    if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
      sum_sTemplate += "}\n";
    }
  } else {
    if ( conf.getSplitBatches() ) {
    } else {
      *code = "__kernel void dedispersionStepOne(__global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const unsigned int * restrict const zappedChannels, __constant const float * restrict const shifts) {\n";
    }
    *code += "unsigned int beam = get_group_id(2) / " + std::to_string(observation.getNrSubbands()) + ";\n"
      "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int subband = get_group_id(2) % " + std::to_string(observation.getNrSubbands()) + ";\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "<%DEFS%>";
    if ( inputBits < 8 ) {
      *code += inputDataType + " bitsBuffer;\n"
        "unsigned int byte = 0;\n"
        "uchar firstBit = 0;\n"
        + inputDataType + " interBuffer;\n";
    }
    *code += "\n"
      "for ( unsigned int channel = subband * " + std::to_string(observation.getNrChannelsPerSubband()) + "; channel < (subband + 1) * " + std::to_string(observation.getNrChannelsPerSubband()) + "; channel += " + std::to_string(conf.getUnroll()) + " ) {\n"
      "<%DEFS_SHIFT%>"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "if ( zappedChannels[channel + <%UNROLL%>] == 0 ) {\n"
      "<%SHIFTS%>"
      "\n"
      "<%SUMS%>"
      "}\n"
      "\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitBatches() ) {
      } else {
        if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) + " ) {\n";
        }
        sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(beam * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)];\n";
        if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "}\n";
        }
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitBatches() ) {
      } else {
        if ( (observation.getNrSamplesPerBatch(true) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch(true)) + " ) {\n";
        }
        sum_sTemplate += "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + std::to_string(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + std::to_string(8 / inputBits) + ") * " + std::to_string(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[(beam * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true) / (8 / inputBits), padding / sizeof(I))) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        sum_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(bit)), std::to_string(bit));
      }
      if ( inputDataType == "char" || inputDataType == "uchar" ) {
        for ( unsigned int bit = inputBits; bit < 8; bit++ ) {
          sum_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + std::to_string(inputBits - 1)), std::to_string(bit));
        }
      }
      sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(interBuffer);\n";
      if ( (observation.getNrSamplesPerBatch(true) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
        sum_sTemplate += "}\n";
      }
    } else {
      if ( conf.getSplitBatches() ) {
      } else {
        if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch(true) / observation.getDownsampling() ) + " ) {\n";
        }
        sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[(beam * " + std::to_string(observation.getNrChannels() * isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerDispersedBatch(true), padding / sizeof(I))) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)]);\n";
        if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
          sum_sTemplate += "}\n";
        }
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
  if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
    store_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) + " ) {\n";
  }
  if ( intermediateDataType == outputDataType ) {
    store_sTemplate += "output[(beam * " + std::to_string(observation.getNrSubbands() * observation.getNrDMs(true) * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(O))) + ") + ((dm + <%DM_OFFSET%>) * " + std::to_string(observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(O))) + ") + (subband * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(O))) + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
  } else {
    store_sTemplate += "output[(beam * " + std::to_string(observation.getNrSubbands() * observation.getNrDMs(true) * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(O))) + ") + ((dm + <%DM_OFFSET%>) * " + std::to_string(observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(O))) + ") + (subband * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(O))) + ") + (sample + <%OFFSET%>)] = convert_" + outputDataType + "(dedispersedSample<%NUM%>DM<%DM_NUM%>);\n";
  }
  if ( ((observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
      store_sTemplate += "}\n";
  }
  // End kernel's template

  std::string * def_s =  new std::string();
  std::string * defsShift_s = new std::string();
  std::string * unrolled_s = new std::string();
  std::string * store_s =  new std::string();

  for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
    std::string dm_s = std::to_string(dm);
    std::string * temp_s = 0;

    temp_s = isa::utils::replace(&defsShiftTemplate, "<%DM_NUM%>", dm_s);
    defsShift_s->append(*temp_s);
    delete temp_s;
  }
  for ( unsigned int sample = 0; sample < conf.getNrItemsD0(); sample++ ) {
    std::string sample_s = std::to_string(sample);
    std::string offset_s = std::to_string(sample * conf.getNrThreadsD0());
    std::string * def_sDM =  new std::string();
    std::string * store_sDM =  new std::string();

    for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
      std::string dm_s = std::to_string(dm);
      std::string * temp_s = 0;

      temp_s = isa::utils::replace(&def_sTemplate, "<%DM_NUM%>", dm_s);
      def_sDM->append(*temp_s);
      delete temp_s;
      temp_s = isa::utils::replace(&store_sTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = std::to_string(dm * conf.getNrThreadsD1());
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", offset_s, true);
      }
      store_sDM->append(*temp_s);
      delete temp_s;
    }
    def_sDM = isa::utils::replace(def_sDM, "<%NUM%>", sample_s, true);
    def_s->append(*def_sDM);
    delete def_sDM;
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
    std::string loop_s = std::to_string(loop);
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
      std::string dm_s = std::to_string(dm);

      temp_s = isa::utils::replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = std::to_string(dm * conf.getNrThreadsD1());
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
      std::string sample_s = std::to_string(sample);
      std::string offset_s = std::to_string(sample * conf.getNrThreadsD0());
      std::string * sumsDM_s = new std::string();

      for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
        std::string dm_s = std::to_string(dm);

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
  code = isa::utils::replace(code, "<%UNROLLED_LOOP%>", *unrolled_s, true);
  code = isa::utils::replace(code, "<%STORES%>", *store_s, true);
  delete def_s;
  delete defsShift_s;
  delete unrolled_s;
  delete store_s;

  return code;
}

template< typename I > std::string * getSubbandDedispersionStepTwoOpenCL(const DedispersionConf & conf, const unsigned int padding, const std::string & inputDataType, const AstroData::Observation & observation, std::vector< float > & shifts)
{
  std::string * code = new std::string();
  std::string unrolled_sTemplate = std::string();
  std::string firstDM_s = std::to_string(observation.getFirstDM());
  if ( firstDM_s.find(".") == std::string::npos ) {
    firstDM_s.append(".0f");
  } else {
    firstDM_s.append("f");
  }
  std::string DMStep_s = std::to_string(observation.getDMStep());
  if ( DMStep_s.find(".") == std::string::npos ) {
    DMStep_s.append(".0f");
  } else {
    DMStep_s.append("f");
  }
  std::string nrTotalSamplesPerBlock_s = std::to_string(conf.getNrThreadsD0() * conf.getNrItemsD0());
  std::string nrTotalDMsPerBlock_s = std::to_string(conf.getNrThreadsD1() * conf.getNrItemsD1());
  std::string nrTotalThreads_s = std::to_string(conf.getNrThreadsD0() * conf.getNrThreadsD1());

  // Begin kernel's template
  if ( conf.getLocalMem() ) {
    *code = "__kernel void dedispersionStepTwo(__global const " + inputDataType + " * restrict const input, __global " + inputDataType + " * restrict const output, __constant const unsigned int * const restrict beamMapping, __constant const float * restrict const shifts) {\n"
      "unsigned int sBeam = get_group_id(2) / " + std::to_string(observation.getNrDMs(true)) + ";\n"
      "unsigned int firstStepDM = get_group_id(2) % " + std::to_string(observation.getNrDMs(true)) + ";\n"
      "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "unsigned int inShMem = 0;\n"
      "unsigned int inGlMem = 0;\n"
      "<%DEFS%>"
      "__local " + inputDataType + " buffer[" + std::to_string((conf.getNrThreadsD0() * conf.getNrItemsD0()) + static_cast< unsigned int >(shifts[0] * (observation.getFirstDM() + ((conf.getNrThreadsD1() * conf.getNrItemsD1()) * observation.getDMStep())))) + "];\n"
      "\n"
      "for ( unsigned int channel = 0; channel < " + std::to_string(observation.getNrSubbands()) + "; channel += " + std::to_string(conf.getUnroll()) + " ) {\n"
      "unsigned int minShift = 0;\n"
      "<%DEFS_SHIFT%>"
      "unsigned int diffShift = 0;\n"
      "\n"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "minShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") * " + DMStep_s + ")));\n"
      "<%SHIFTS%>"
      "diffShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + (((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + std::to_string((conf.getNrThreadsD1() * conf.getNrItemsD1()) - 1) + ") * " + DMStep_s + "))) - minShift;\n"
      "\n"
      "inShMem = (get_local_id(1) * " + std::to_string(conf.getNrThreadsD0()) + ") + get_local_id(0);\n"
      "inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n"
      "while ( (inShMem < (" + nrTotalSamplesPerBlock_s + " + diffShift)) && (inGlMem < " + std::to_string(observation.getNrSamplesPerBatch(true) / observation.getDownsampling()) + ") ) {\n"
      "buffer[inShMem] = input[(beamMapping[(sBeam * " + std::to_string(observation.getNrSubbands(padding / sizeof(unsigned int))) + ") + (channel + <%UNROLL%>)] * " + std::to_string(observation.getNrSubbands() * observation.getNrDMs(true) * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + ") + (firstStepDM * " + std::to_string(observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + ") + inGlMem];\n"
      "inShMem += " + nrTotalThreads_s + ";\n"
      "inGlMem += " + nrTotalThreads_s + ";\n"
      "}\n"
      "barrier(CLK_LOCAL_MEM_FENCE);\n"
      "\n"
      "<%SUMS%>"
      "\n";
    if ( conf.getUnroll() > 1 ) {
      unrolled_sTemplate += "barrier(CLK_LOCAL_MEM_FENCE);\n";
    }
  } else {
    *code = "__kernel void dedispersionStepTwo(__global const " + inputDataType + " * restrict const input, __global " + inputDataType + " * restrict const output, __constant const unsigned int * restrict const beamMapping, __constant const float * restrict const shifts) {\n"
      "unsigned int sBeam = get_group_id(2) / " + std::to_string(observation.getNrDMs(true)) + ";\n"
      "unsigned int firstStepDM = get_group_id(2) % " + std::to_string(observation.getNrDMs(true)) + ";\n"
      "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "<%DEFS%>"
      "\n"
      "for ( unsigned int channel = 0; channel < " + std::to_string(observation.getNrSubbands()) + "; channel += " + std::to_string(conf.getUnroll()) + " ) {\n"
      "<%DEFS_SHIFT%>"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "<%SHIFTS%>"
      "\n"
      "<%SUMS%>"
      "\n";
  }
  std::string def_sTemplate = inputDataType + " dedispersedSample<%NUM%>DM<%DM_NUM%> = 0;\n";
  std::string defsShiftTemplate = "unsigned int shiftDM<%DM_NUM%> = 0;\n";
  std::string shiftsTemplate;
  std::string sum_sTemplate;
  if ( conf.getLocalMem() ) {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + DMStep_s + "))) - minShift;\n";
    sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + shiftDM<%DM_NUM%>];\n";
  } else {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + DMStep_s + ")));\n";
    if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
      sum_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch() / observation.getDownsampling()) + " ) {\n";
    }
    sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(beamMapping[(sBeam * " + std::to_string(observation.getNrSubbands(padding / sizeof(unsigned int))) + ") + (channel + <%UNROLL%>)] * " + std::to_string(observation.getNrSubbands() * observation.getNrDMs(true) * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + ") + (firstStepDM * " + std::to_string(observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + ") + ((channel + <%UNROLL%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerBatch(true) / observation.getDownsampling(), padding / sizeof(I))) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)];\n";
    if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
      sum_sTemplate += "}\n";
    }
  }
  std::string store_sTemplate;
  if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
    store_sTemplate += "if ( (sample + <%OFFSET%>) < " + std::to_string(observation.getNrSamplesPerBatch()) + " ) {\n";
  }
  store_sTemplate += "output[(sBeam * " + std::to_string(observation.getNrDMs(true) * observation.getNrDMs() * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(I))) + ") + (firstStepDM * " + std::to_string(observation.getNrDMs() * isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(I))) + ") + ((dm + <%DM_OFFSET%>) * " + std::to_string(isa::utils::pad(observation.getNrSamplesPerBatch() / observation.getDownsampling(), padding / sizeof(I))) + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
  if ( ((observation.getNrSamplesPerBatch() / observation.getDownsampling()) % (conf.getNrThreadsD0() * conf.getNrItemsD0())) != 0 ) {
    store_sTemplate += "}\n";
  }
  // End kernel's template

  std::string * def_s =  new std::string();
  std::string * defsShift_s = new std::string();
  std::string * unrolled_s = new std::string();
  std::string * store_s =  new std::string();

  for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
    std::string dm_s = std::to_string(dm);
    std::string * temp_s = 0;

    temp_s = isa::utils::replace(&defsShiftTemplate, "<%DM_NUM%>", dm_s);
    defsShift_s->append(*temp_s);
    delete temp_s;
  }
  for ( unsigned int sample = 0; sample < conf.getNrItemsD0(); sample++ ) {
    std::string sample_s = std::to_string(sample);
    std::string offset_s = std::to_string(sample * conf.getNrThreadsD0());
    std::string * def_sDM =  new std::string();
    std::string * store_sDM =  new std::string();

    for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
      std::string dm_s = std::to_string(dm);
      std::string * temp_s = 0;

      temp_s = isa::utils::replace(&def_sTemplate, "<%DM_NUM%>", dm_s);
      def_sDM->append(*temp_s);
      delete temp_s;
      temp_s = isa::utils::replace(&store_sTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = std::to_string(dm * conf.getNrThreadsD1());
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", offset_s, true);
      }
      store_sDM->append(*temp_s);
      delete temp_s;
    }
    def_sDM = isa::utils::replace(def_sDM, "<%NUM%>", sample_s, true);
    def_s->append(*def_sDM);
    delete def_sDM;
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
    std::string loop_s = std::to_string(loop);
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
      std::string dm_s = std::to_string(dm);

      temp_s = isa::utils::replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = std::to_string(dm * conf.getNrThreadsD1());
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
      std::string sample_s = std::to_string(sample);
      std::string offset_s = std::to_string(sample * conf.getNrThreadsD0());
      std::string * sumsDM_s = new std::string();

      for ( unsigned int dm = 0; dm < conf.getNrItemsD1(); dm++ ) {
        std::string dm_s = std::to_string(dm);

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
  code = isa::utils::replace(code, "<%UNROLLED_LOOP%>", *unrolled_s, true);
  code = isa::utils::replace(code, "<%STORES%>", *store_s, true);
  delete def_s;
  delete defsShift_s;
  delete unrolled_s;
  delete store_s;

  return code;
}

} // Dedispersion

