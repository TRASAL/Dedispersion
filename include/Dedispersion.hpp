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

// Sequential dedispersion
template< typename T > void dedispersion(AstroData::Observation & observation, const std::vector< T > & input, std::vector< T > & output, const std::vector< unsigned int > & shifts);
// OpenCL dedispersion algorithm
std::string * getDedispersionOpenCL(const bool localMem, const unsigned int nrSamplesPerBlock, const unsigned int nrDMsPerBlock, const unsigned int nrSamplesPerThread, const unsigned int nrDMsPerThread, const unsigned int unroll, const std::string & dataType, const AstroData::Observation & observation, std::vector< unsigned int > & shifts);


// Implementations
template< typename T > void dedispersion(AstroData::Observation & observation, const std::vector< T > & input, std::vector< T > & output, const std::vector< int > & shifts) {
	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
			T dedispersedSample = static_cast< T >(0);

			for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
				unsigned int shift = shifts[(channel * observation.getNrPaddedDMs()) + dm];

				dedispersedSample += input[(channel * observation.getNrSamplesPerDispersedChannel()) + (sample + shift)];
			}

			output[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] = dedispersedSample;
		}
	}
}

std::string * getDedispersionOpenCL(const bool localMem, const unsigned int nrSamplesPerBlock, const unsigned int nrDMsPerBlock, const unsigned int nrSamplesPerThread, const unsigned int nrDMsPerThread, const unsigned int unroll, const std::string & dataType, const AstroData::Observation & observation, std::vector< unsigned int > & shifts) {
  std::string * code = new std::string();
  std::string sum0_sTemplate = std::string();
  std::string sum_sTemplate = std::string();
  std::string unrolled_sTemplate = std::string();
  std::string nrTotalSamplesPerBlock_s = isa::utils::toString(nrSamplesPerBlock * nrSamplesPerThread);
  std::string nrTotalDMsPerBlock_s = isa::utils::toString(nrDMsPerBlock * nrDMsPerThread);
  std::string nrTotalThreads_s = isa::utils::toString(nrSamplesPerBlock * nrDMsPerBlock);

  // Begin kernel's template
  if ( localMem ) {
    *code = "__kernel void dedispersion(__global const " + dataType + " * restrict const input, __global " + dataType + " * restrict const output, __global const unsigned int * restrict const shifts) {\n"
      "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + (get_local_id(1) * " + isa::utils::toString(nrDMsPerThread) + ");\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "unsigned int inShMem = 0;\n"
      "unsigned int inGlMem = 0;\n"
      "<%DEFS%>"
      "__local " + dataType + " buffer[" + isa::utils::toString((nrSamplesPerBlock * nrSamplesPerThread) + (shifts[observation.getNrDMs() - 1] - shifts[observation.getNrDMs() - (nrDMsPerBlock * nrDMsPerThread)])) + "];\n"
      "\n"
      "for ( unsigned int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(unroll) + " ) {\n"
      "<%DEFS_SHIFT%>"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "inShMem = (get_local_id(1) * " + isa::utils::toString(nrSamplesPerBlock) + ") + get_local_id(0);\n"
      "inGlMem = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem;\n"
      "while ( inShMem < " + nrTotalSamplesPerBlock_s + " ) {\n"
      "buffer[inShMem] = input[(" + isa::utils::toString(observation.getNrChannels() - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerDispersedChannel()) + ") + inGlMem];\n"
      "inShMem += " + nrTotalThreads_s + ";\n"
      "inGlMem += " + nrTotalThreads_s + ";\n"
      "}\n"
      "barrier(CLK_LOCAL_MEM_FENCE);\n"
      "\n"
      "<%SUM0%>"
      "\n"
      "<%STORES%>"
      "}";
    unrolled_sTemplate = "const unsigned int minShift = shifts[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrPaddedDMs()) + ") + (get_group_id(1) * " + nrTotalDMsPerBlock_s + ")];\n"
      "<%SHIFTS%>"
      "const unsigned int maxShift = shifts[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrPaddedDMs()) + ") + ((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + isa::utils::toString((nrDMsPerBlock * nrDMsPerThread) - 1) + ")];\n"
      "\n"
      "inShMem = (get_local_id(1) * " + isa::utils::toString(nrSamplesPerBlock) + ") + get_local_id(0);\n"
      "inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n"
      "while ( inShMem < (" + nrTotalSamplesPerBlock_s + " + (maxShift - minShift)) ) {\n"
      "buffer[inShMem] = input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerDispersedChannel()) + ") + inGlMem];\n"
      "inShMem += " + nrTotalThreads_s + ";\n"
      "inGlMem += " + nrTotalThreads_s + ";\n"
      "}\n"
      "barrier(CLK_LOCAL_MEM_FENCE);\n"
      "\n"
      "<%SUMS%>"
      "\n";
    if ( unroll > 1 ) {
      unrolled_sTemplate += "barrier(CLK_LOCAL_MEM_FENCE);\n";
    }
    sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[get_local_id(0) + <%OFFSET%>];\n";
    sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + (shiftDM<%DM_NUM%> - minShift)];\n";
  } else {
    *code = "__kernel void dedispersion(__global const " + dataType + " * restrict const input, __global " + dataType + " * restrict const output, __global const unsigned int * restrict const shifts) {\n"
      "unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + (get_local_id(1) * " + isa::utils::toString(nrDMsPerThread) + ");\n"
      "unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "<%DEFS%>"
      "\n"
      "for ( unsigned int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(unroll) + " ) {\n"
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
    sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(" + isa::utils::toString(observation.getNrChannels() - 1) + " * " + isa::utils::toString(observation.getNrSamplesPerDispersedChannel()) + ") + sample + <%OFFSET%>];\n";
    sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerDispersedChannel()) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)];\n";
  }
	std::string def_sTemplate = dataType + " dedispersedSample<%NUM%>DM<%DM_NUM%> = 0;\n";
  std::string defsShiftTemplate = "unsigned int shiftDM<%DM_NUM%> = 0;\n";
	std::string shiftsTemplate = "shiftDM<%DM_NUM%> = shifts[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrPaddedDMs()) + ") + (dm + <%DM_OFFSET%>)];\n";
	std::string store_sTemplate = "output[((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
	// End kernel's template

  std::string * def_s =  new std::string();
  std::string * defsShift_s = new std::string();
  std::string * sum0_s = new std::string();
  std::string * unrolled_s = new std::string();
  std::string * store_s =  new std::string();

  for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
    std::string dm_s = isa::utils::toString(dm);
    std::string * temp_s = 0;

    temp_s = isa::utils::replace(&defsShiftTemplate, "<%DM_NUM%>", dm_s);
    defsShift_s->append(*temp_s);
    delete temp_s;
  }
  for ( unsigned int sample = 0; sample < nrSamplesPerThread; sample++ ) {
    std::string sample_s = isa::utils::toString(sample);
    std::string offset_s = isa::utils::toString(sample * nrSamplesPerBlock);
    std::string * def_sDM =  new std::string();
    std::string * store_sDM =  new std::string();

    for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
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
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", dm_s, true);
      }
      store_sDM->append(*temp_s);
      delete temp_s;
    }
    def_sDM = isa::utils::replace(def_sDM, "<%NUM%>", sample_s, true);
    def_s->append(*def_sDM);
    delete def_sDM;
    sum0_s = isa::utils::replace(sum0_s, "<%NUM%>", sample_s, true);
    if ( sample * nrSamplesPerBlock == 0 ) {
      std::string empty_s;
      sum0_s = isa::utils::replace(sum0_s, " + <%OFFSET%>", empty_s, true);
    } else {
      sum0_s = isa::utils::replace(sum0_s, "<%OFFSET%>", offset_s, true);
    }
    store_sDM = isa::utils::replace(store_sDM, "<%NUM%>", sample_s, true);
    if ( sample * nrSamplesPerBlock == 0 ) {
      std::string empty_s;
      store_sDM = isa::utils::replace(store_sDM, " + <%OFFSET%>", empty_s, true);
    } else {
      store_sDM = isa::utils::replace(store_sDM, "<%OFFSET%>", offset_s, true);
    }
    store_s->append(*store_sDM);
    delete store_sDM;
  }
  for ( unsigned int loop = 0; loop < unroll; loop++ ) {
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
    for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
      std::string dm_s = isa::utils::toString(dm);

      temp_s = isa::utils::replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
      if ( dm == 0 ) {
        std::string empty_s;
        temp_s = isa::utils::replace(temp_s, " + <%DM_OFFSET%>", empty_s, true);
      } else {
        temp_s = isa::utils::replace(temp_s, "<%DM_OFFSET%>", dm_s, true);
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
    for ( unsigned int sample = 0; sample < nrSamplesPerThread; sample++ ) {
      std::string sample_s = isa::utils::toString(sample);
      std::string offset_s = isa::utils::toString(sample * nrSamplesPerBlock);
      std::string * sumsDM_s = new std::string();

      for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
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
      if ( sample * nrSamplesPerBlock == 0 ) {
        std::string empty_s;
        sumsDM_s = isa::utils::replace(sumsDM_s, " + <%OFFSET%>", empty_s, true);
      } else {
        sumsDM_s = isa::utils::replace(sumsDM_s, "<%OFFSET%>", offset_s, true);
      }
      sums_s->append(*sumsDM_s);
      delete sumsDM_s;
    }
    unrolled_s = isa::utils::replace(unrolled_s, "<%SUMS%>", *sums_s, true);
    if ( unroll == 0 ) {
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

