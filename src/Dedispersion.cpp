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

std::string * getDedispersionOpenCL(const DedispersionConf & conf, const uint8_t inputBits, const std::string & inputDataType, const std::string & intermediateDataType, const std::string & outputDataType, const AstroData::Observation & observation, std::vector< float > & shifts) {
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
  std::string nrTotalSamplesPerBlock_s = isa::utils::toString(conf.getNrSamplesPerBlock() * conf.getNrSamplesPerThread());
  std::string nrTotalDMsPerBlock_s = isa::utils::toString(conf.getNrDMsPerBlock() * conf.getNrDMsPerThread());
  std::string nrTotalThreads_s = isa::utils::toString(conf.getNrSamplesPerBlock() * conf.getNrDMsPerBlock());

  // Begin kernel's template
  if ( conf.getLocalMem() ) {
    if ( conf.getSplitSeconds() ) {
      *code = "__kernel void dedispersion(const unsigned int secondOffset, __global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts) {\n"
        "unsigned int sampleOffset = secondOffset * " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ";\n";
    } else {
      *code = "__kernel void dedispersion(__global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts) {\n";
    }
    *code +=  "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "unsigned int inShMem = 0;\n"
      "unsigned int inGlMem = 0;\n"
      "<%DEFS%>"
      "__local " + intermediateDataType + " buffer[" + isa::utils::toString((conf.getNrSamplesPerBlock() * conf.getNrSamplesPerThread()) + static_cast< unsigned int >(shifts[0] * (observation.getFirstDM() + (((conf.getNrDMsPerBlock() * conf.getNrDMsPerThread()) - 1) * observation.getDMStep())))) + "];\n";
    if ( inputBits < 8 ) {
      *code += "uchar bitsBuffer;\n"
        "uchar byte = 0;\n"
        "uchar firstBit = 0;\n"
        "unsigned int interBuffer;\n";
    }
    *code += "\n"
      "for ( unsigned int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(conf.getUnroll()) + " ) {\n"
      "unsigned int minShift = 0;\n"
      "<%DEFS_SHIFT%>"
      "unsigned int diffShift = 0;\n"
      "\n"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "inShMem = (get_local_id(1) * " + isa::utils::toString(conf.getNrSamplesPerBlock()) + ") + get_local_id(0);\n"
      "inGlMem = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem;\n"
      "while ( inShMem < " + nrTotalSamplesPerBlock_s + " ) {\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitSeconds() ) {
        *code += "buffer[inShMem] = input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + inGlMem];\n";
      } else {
        *code += "buffer[inShMem] = input[(" + isa::utils::toString(observation.getNrChannels() - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel()) + ") + inGlMem];\n";
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitSeconds() ) {
        *code += "interBuffer = 0;\n"
          "byte = (inGlMem / " + isa::utils::toString(8 / inputBits) + ");\n"
          "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerPaddedSecond() / (8 / inputBits), observation.getPadding())) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerPaddedSecond() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
      } else {
        *code += "interBuffer = 0;\n"
          "byte = (inGlMem / " + isa::utils::toString(8 / inputBits) + ");\n"
          "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[(" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
      }
      for ( uint8_t bit = 0; bit < inputBits; bit++ ) {
        *code += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), "byte");
      }
      *code += "buffer[inShMem] = convert_" + intermediateDataType + "(interBuffer);\n";
    } else {
      if ( conf.getSplitSeconds() ) {
        *code += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + inGlMem]);\n";
      } else {
        *code += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel()) + ") + inGlMem]);\n";
      }
    }
    *code += "inShMem += " + nrTotalThreads_s + ";\n"
      "inGlMem += " + nrTotalThreads_s + ";\n"
      "}\n"
      "barrier(CLK_LOCAL_MEM_FENCE);\n"
      "\n"
      "<%SUM0%>"
      "\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "minShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") * " + isa::utils::toString(observation.getDMStep()) + "f)));\n"
      "<%SHIFTS%>"
      "diffShift = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + (((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + isa::utils::toString((conf.getNrDMsPerBlock() * conf.getNrDMsPerThread()) - 1) + ") * " + isa::utils::toString(observation.getDMStep()) + "f))) - minShift;\n"
      "\n"
      "inShMem = (get_local_id(1) * " + isa::utils::toString(conf.getNrSamplesPerBlock()) + ") + get_local_id(0);\n";
    if ( conf.getSplitSeconds() ) {
      unrolled_sTemplate += "inGlMem = sampleOffset + ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n";
    } else {
      unrolled_sTemplate += "inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n";
    }
    unrolled_sTemplate += "while ( inShMem < (" + nrTotalSamplesPerBlock_s + " + diffShift) ) {\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitSeconds() ) {
        unrolled_sTemplate += "buffer[inShMem] = input[(((inGlMem / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + (inGlMem % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")];\n";
      } else {
        unrolled_sTemplate += "buffer[inShMem] = input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel()) + ") + inGlMem];\n";
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitSeconds() ) {
        unrolled_sTemplate += "interBuffer = 0;\n"
          "byte = inGlMem % " + isa::utils::toString(observation.getNrSamplesPerSecond() / (8 / inputBits)) + ";\n"
          "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[(((inGlMem / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerPaddedSecond() / (8 / inputBits), observation.getPadding())) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerPaddedSecond() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
      } else {
        unrolled_sTemplate += "interBuffer = 0;\n"
          "byte = (inGlMem / " + isa::utils::toString(8 / inputBits) + ");\n"
          "firstBit = ((inGlMem % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ");\n"
          "bitsBuffer = input[((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        unrolled_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), "byte");
      }
      unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(interBuffer);\n";
    } else {
      if ( conf.getSplitSeconds() ) {
        unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(input[(((inGlMem / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + (inGlMem % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")]);\n";
      } else {
        unrolled_sTemplate += "buffer[inShMem] = convert_" + intermediateDataType + "(input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel()) + ") + inGlMem]);\n";
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
    sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[get_local_id(0) + <%OFFSET%>];\n";
    sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + shiftDM<%DM_NUM%>];\n";
  } else {
    if ( conf.getSplitSeconds() ) {
      *code = "__kernel void dedispersion(const unsigned int secondOffset, __global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts) {\n"
        "unsigned int sampleOffset = secondOffset * " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ";\n";
    } else {
      *code = "__kernel void dedispersion(__global const " + inputDataType + " * restrict const input, __global " + outputDataType + " * restrict const output, __constant const float * restrict const shifts) {\n";
    }
    *code += "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "<%DEFS%>";
    if ( inputBits < 8 ) {
      *code += "uchar bitsBuffer;\n"
        "uchar byte = 0;\n"
        "uchar firstBit = 0;\n"
        "unsigned int interBuffer;\n";
    }
    *code += "\n"
      "for ( unsigned int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(conf.getUnroll()) + " ) {\n"
      "<%DEFS_SHIFT%>"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "<%SUM0%>"
      "\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "<%SHIFTS%>"
      "\n"
      "<%SUMS%>"
      "\n";
    if ( (inputDataType == intermediateDataType) && (inputBits >= 8) ) {
      if ( conf.getSplitSeconds() ) {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedSecond()) + ") + sample + <%OFFSET%>];\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[((((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + ((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")];\n";
      } else {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedDispersedChannel()) + ") + sample + <%OFFSET%>];\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel()) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)];\n";
      }
    } else if ( inputBits < 8 ) {
      if ( conf.getSplitSeconds() ) {
        sum0_sTemplate = "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%>) / " + isa::utils::toString(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
        sum_sTemplate =  "interBuffer = 0;\n"
          "byte = (sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(observation.getNrSamplesPerSecond() / (8 / inputBits)) + ";\n"
          "firstBit = ((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[((((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerSecond() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
      } else {
        sum0_sTemplate = "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%>) / " + isa::utils::toString(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[(" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
        sum_sTemplate =  "interBuffer = 0;\n"
          "byte = (sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(8 / inputBits) + ";\n"
          "firstBit = ((sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(8 / inputBits) + ") * " + isa::utils::toString(static_cast< unsigned int >(inputBits)) + ";\n"
          "bitsBuffer = input[((channel + <%UNROLL%>) * " + isa::utils::toString(isa::utils::pad(observation.getNrSamplesPerDispersedChannel() / (8 / inputBits), observation.getPadding())) + ") + byte];\n";
      }
      for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
        sum0_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), "byte");
        sum_sTemplate += isa::OpenCL::setBit("interBuffer", isa::OpenCL::getBit("bitsBuffer", "firstBit + " + isa::utils::toString(bit)), "byte");
      }
      sum0_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(interBuffer);\n";
      sum_sTemplate += "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(interBuffer);\n";
    } else {
      if ( conf.getSplitSeconds() ) {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[(secondOffset * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + (" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedSecond()) + ") + sample + <%OFFSET%>]);\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[((((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) / " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ") % " + isa::utils::toString(observation.getNrDelaySeconds()) + ") * " + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels()) * observation.getNrSamplesPerPaddedSecond()) + ") + ((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + ((sampleOffset + sample + <%OFFSET%> + shiftDM<%DM_NUM%>) % " + isa::utils::toString(observation.getNrSamplesPerSecond()) + ")]);\n";
      } else {
        sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[(" + isa::utils::toString(static_cast< uint64_t >(observation.getNrChannels() - 1) * observation.getNrSamplesPerPaddedDispersedChannel()) + ") + sample + <%OFFSET%>]);\n";
        sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += convert_" + intermediateDataType + "(input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedDispersedChannel()) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)]);\n";
      }
    }
  }
	std::string def_sTemplate = intermediateDataType + " dedispersedSample<%NUM%>DM<%DM_NUM%> = 0;\n";
  std::string defsShiftTemplate = "unsigned int shiftDM<%DM_NUM%> = 0;\n";
  std::string shiftsTemplate;
  if ( conf.getLocalMem() ) {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getDMStep()) + "f))) - minShift;\n";
  } else {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_uint_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getDMStep()) + "f)));\n";
  }
  std::string store_sTemplate;
  if ( intermediateDataType == outputDataType ) {
    store_sTemplate = "output[((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
  } else {
    store_sTemplate = "output[((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + (sample + <%OFFSET%>)] = convert_" + outputDataType + "(dedispersedSample<%NUM%>DM<%DM_NUM%>);\n";
  }
	// End kernel's template

  std::string * def_s =  new std::string();
  std::string * defsShift_s = new std::string();
  std::string * sum0_s = new std::string();
  std::string * unrolled_s = new std::string();
  std::string * store_s =  new std::string();

  for ( unsigned int dm = 0; dm < conf.getNrDMsPerThread(); dm++ ) {
    std::string dm_s = isa::utils::toString(dm);
    std::string * temp_s = 0;

    temp_s = isa::utils::replace(&defsShiftTemplate, "<%DM_NUM%>", dm_s);
    defsShift_s->append(*temp_s);
    delete temp_s;
  }
  for ( unsigned int sample = 0; sample < conf.getNrSamplesPerThread(); sample++ ) {
    std::string sample_s = isa::utils::toString(sample);
    std::string offset_s = isa::utils::toString(sample * conf.getNrSamplesPerBlock());
    std::string * def_sDM =  new std::string();
    std::string * store_sDM =  new std::string();

    for ( unsigned int dm = 0; dm < conf.getNrDMsPerThread(); dm++ ) {
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
        std::string offset_s = isa::utils::toString(dm * conf.getNrDMsPerBlock());
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", offset_s, true);
      }
      store_sDM->append(*temp_s);
      delete temp_s;
    }
    def_sDM = isa::utils::replace(def_sDM, "<%NUM%>", sample_s, true);
    def_s->append(*def_sDM);
    delete def_sDM;
    sum0_s = isa::utils::replace(sum0_s, "<%NUM%>", sample_s, true);
    if ( sample * conf.getNrSamplesPerBlock() == 0 ) {
      std::string empty_s;
      sum0_s = isa::utils::replace(sum0_s, " + <%OFFSET%>", empty_s, true);
    } else {
      sum0_s = isa::utils::replace(sum0_s, "<%OFFSET%>", offset_s, true);
    }
    store_sDM = isa::utils::replace(store_sDM, "<%NUM%>", sample_s, true);
    if ( sample * conf.getNrSamplesPerBlock() == 0 ) {
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
    for ( unsigned int dm = 0; dm < conf.getNrDMsPerThread(); dm++ ) {
      std::string dm_s = isa::utils::toString(dm);

      temp_s = isa::utils::replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        std::string offset_s = isa::utils::toString(dm * conf.getNrDMsPerBlock());
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
    for ( unsigned int sample = 0; sample < conf.getNrSamplesPerThread(); sample++ ) {
      std::string sample_s = isa::utils::toString(sample);
      std::string offset_s = isa::utils::toString(sample * conf.getNrSamplesPerBlock());
      std::string * sumsDM_s = new std::string();

      for ( unsigned int dm = 0; dm < conf.getNrDMsPerThread(); dm++ ) {
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
      if ( sample * conf.getNrSamplesPerBlock() == 0 ) {
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

