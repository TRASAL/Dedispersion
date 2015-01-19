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

DedispersionConf::DedispersionConf() {}

DedispersionConf::~DedispersionConf() {}

inline bool DedispersionConf::getLocalMem() const {
  return local;
}

inline unsigned int DedispersionConf::getNrSamplesPerBlock() const {
  return nrSamplesPerBlock;
}

inline unsigned int DedispersionConf::getNrSamplesPerThread() const {
  return nrSamplesPerThread;
}

inline unsigned int DedispersionConf::getNrDMsPerBlock() const {
  return nrDMsPerBlock;
}

inline unsigned int DedispersionConf::getNrDMsPerThread() const {
  return nrDMsPerThread;
}

inline unsigned int DedispersionConf::getUnroll() const {
  return unroll;
}

inline void DedispersionConf::setLocalMem(bool local) {
  this->local = local;
}

inline void DedispersionConf::setNrSamplesPerBlock(unsigned int samples) {
  nrSamplesPerBlock = samples;
}

inline void DedispersionConf::setNrSamplesPerThread(unsigned int samples) {
  nrSamplesPerThread = samples;
}

inline void DedispersionConf::setNrDMsPerBlock(unsigned int dms) {
  nrDMsPerBlock = dms;
}

inline void DedispersionConf::setNrDMsPerThread(unsigned int dms) {
  nrDMsPerThread = dms;
}

inline void DedispersionConf::setUnroll(unsigned int unroll) {
  this->unroll = unroll;
}

template< typename T > void dedispersion(AstroData::Observation & observation, const std::vector< T > & input, std::vector< T > & output, const std::vector< float > & shifts) {
	for ( unsigned int dm = 0; dm < observation.getNrDMs(); dm++ ) {
		for ( unsigned int sample = 0; sample < observation.getNrSamplesPerSecond(); sample++ ) {
			T dedispersedSample = static_cast< T >(0);

			for ( unsigned int channel = 0; channel < observation.getNrChannels(); channel++ ) {
				unsigned int shift = static_cast< unsigned int >((observation.getFirstDM() + (dm * observation.getDMStep())) * shifts[channel]);

				dedispersedSample += input[(channel * observation.getNrSamplesPerDispersedChannel()) + (sample + shift)];
			}

			output[(dm * observation.getNrSamplesPerPaddedSecond()) + sample] = dedispersedSample;
		}
	}
}

std::string * getDedispersionOpenCL(const DedispersionConf & conf, const std::string & dataType, const AstroData::Observation & observation, std::vector< float > & shifts) {
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
    *code = "__kernel void dedispersion(__global const " + dataType + " * restrict const input, __global " + dataType + " * restrict const output, __constant const float * restrict const shifts) {\n"
      "int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "int inShMem = 0;\n"
      "int inGlMem = 0;\n"
      "<%DEFS%>"
      "__local " + dataType + " buffer[" + isa::utils::toString((conf.getNrSamplesPerBlock() * nrSamplesPerThread) + static_cast< unsigned int >(shifts[0] * (observation.getFirstDM() + (((conf.getNrDMsPerBlock() * conf.getNrDMsPerThread()) - 1) * observation.getDMStep())))) + "];\n"
      "\n"
      "for ( int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(conf.getUnroll()) + " ) {\n"
      "int minShift = 0;\n"
      "<%DEFS_SHIFT%>"
      "int diffShift = 0;\n"
      "\n"
      "<%UNROLLED_LOOP%>"
      "}\n"
      "inShMem = (get_local_id(1) * " + isa::utils::toString(conf.getNrSamplesPerBlock()) + ") + get_local_id(0);\n"
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
    unrolled_sTemplate = "minShift = convert_int_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") * " + isa::utils::toString(observation.getDMStep()) + "f)));\n"
      "<%SHIFTS%>"
      "diffShift = convert_int_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + (((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + isa::utils::toString((conf.getNrDMsPerBlock() * conf.getNrDMsPerThread()) - 1) + ") * " + isa::utils::toString(observation.getDMStep()) + "f))) - minShift;\n"
      "\n"
      "inShMem = (get_local_id(1) * " + isa::utils::toString(conf.getNrSamplesPerBlock()) + ") + get_local_id(0);\n"
      "inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n"
      "while ( inShMem < (" + nrTotalSamplesPerBlock_s + " + diffShift) ) {\n"
      "buffer[inShMem] = input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerDispersedChannel()) + ") + inGlMem];\n"
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
    sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[get_local_id(0) + <%OFFSET%>];\n";
    sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + shiftDM<%DM_NUM%>];\n";
  } else {
    *code = "__kernel void dedispersion(__global const " + dataType + " * restrict const input, __global " + dataType + " * restrict const output, __constant const float * restrict const shifts) {\n"
      "int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + get_local_id(1);\n"
      "int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
      "<%DEFS%>"
      "\n"
      "for ( int channel = 0; channel < " + isa::utils::toString(observation.getNrChannels() - 1) + "; channel += " + isa::utils::toString(conf.getUnroll()) + " ) {\n"
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
    sum0_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[(" + isa::utils::toString(static_cast< long long unsigned int >(observation.getNrChannels() - 1) * observation.getNrSamplesPerDispersedChannel()) + ") + sample + <%OFFSET%>];\n";
    sum_sTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += input[((channel + <%UNROLL%>) * " + isa::utils::toString(observation.getNrSamplesPerDispersedChannel()) + ") + (sample + <%OFFSET%> + shiftDM<%DM_NUM%>)];\n";
  }
	std::string def_sTemplate = dataType + " dedispersedSample<%NUM%>DM<%DM_NUM%> = 0;\n";
  std::string defsShiftTemplate = "int shiftDM<%DM_NUM%> = 0;\n";
  std::string shiftsTemplate;
  if ( conf.getLocalMem() ) {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_int_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getDMStep()) + "f))) - minShift;\n";
  } else {
    shiftsTemplate = "shiftDM<%DM_NUM%> = convert_int_rtz(shifts[channel + <%UNROLL%>] * (" + firstDM_s + " + ((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getDMStep()) + "f)));\n";
  }
	std::string store_sTemplate = "output[((dm + <%DM_OFFSET%>) * " + isa::utils::toString(observation.getNrSamplesPerPaddedSecond()) + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
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

