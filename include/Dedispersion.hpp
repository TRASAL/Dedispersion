/*
 * Copyright (C) 2011
 * Alessio Sclocco <a.sclocco@vu.nl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#include <string>
#include <vector>
#include <cmath>
using std::string;
using std::vector;
using std::make_pair;
using std::pow;
using std::ceil;

#include <Exceptions.hpp>
#include <CLData.hpp>
#include <utils.hpp>
#include <Kernel.hpp>
#include <Observation.hpp>
using isa::Exceptions::OpenCLError;
using isa::OpenCL::CLData;
using isa::utils::giga;
using isa::utils::toString;
using isa::utils::toStringValue;
using isa::utils::replace;
using isa::OpenCL::Kernel;
using AstroData::Observation;


#ifndef DEDISPERSION_HPP
#define DEDISPERSION_HPP

namespace TDM {

// OpenCL dedispersion algorithm
template< typename T > class Dedispersion : public Kernel< T > {
public:
	Dedispersion(string name, string dataType);

	void generateCode() throw (OpenCLError);
	void operator()(CLData< T > *input, CLData< T > *output) throw (OpenCLError);

	inline void setNrSamplesPerBlock(unsigned int samples);
	inline void setNrDMsPerBlock(unsigned int DMs);
	inline void setNrSamplesPerThread(unsigned int samples);
	inline void setNrDMsPerThread(unsigned int DMs);

	inline void setNrSamplesPerDispersedChannel(unsigned int samples);

	inline void setObservation(Observation< T > *obs);
	inline void setShifts(CLData< unsigned int > *data);
	
private:
	unsigned int nrSamplesPerBlock;
	unsigned int nrDMsPerBlock;
	unsigned int nrSamplesPerThread;
	unsigned int nrDMsPerThread;
	unsigned int nrSamplesPerDispersedChannel;
	cl::NDRange globalSize;
	cl::NDRange localSize;

	Observation< T > * observation;
	CLData< unsigned int > * shifts;
};


// Implementation
template< typename T > Dedispersion< T >::Dedispersion(string name, string dataType) : Kernel< T >(name, dataType), nrSamplesPerBlock(0), nrDMsPerBlock(0), nrSamplesPerThread(0), nrDMsPerThread(0), nrSamplesPerDispersedChannel(0), globalSize(cl::NDRange(1, 1, 1)), localSize(cl::NDRange(1, 1, 1)), observation(0), shifts(0) {}

template< typename T > void Dedispersion< T >::generateCode() throw (OpenCLError) {
	// Begin kernel's template
	string nrTotalDMsPerBlock_s = toStringValue< unsigned int >(nrDMsPerBlock * nrDMsPerThread);
	string nrDMsPerThread_s = toStringValue< unsigned int >(nrDMsPerThread);
	string nrTotalSamplesPerBlock_s = toStringValue< unsigned int >(nrSamplesPerBlock * nrSamplesPerThread);
	string nrSamplesPerBlock_s = toStringValue< unsigned int >(nrSamplesPerBlock);
	string necessarySharedMemory_s = toStringValue((nrSamplesPerBlock * nrSamplesPerThread) + ((*shifts)[(observation->getNrDMs() - 1) * observation->getNrPaddedChannels()] - (*shifts)[(observation->getNrDMs() - (nrDMsPerBlock * nrDMsPerThread)) * observation->getNrPaddedChannels()]) + 1);
	string nrChannels_s = toStringValue< unsigned int >(observation->getNrChannels());
	string nrPaddedChannels_s = toStringValue< unsigned int >(observation->getNrPaddedChannels());
	string nrSamplesPerPaddedSecond_s = toStringValue< unsigned int >(observation->getNrSamplesPerPaddedSecond());
	string nrSamplesPerDispersedChannel_s = toStringValue< unsigned int >(nrSamplesPerDispersedChannel);
	string nrTotalThreads_s = toStringValue< unsigned int >(nrSamplesPerBlock * nrDMsPerBlock);

	delete this->code;
	this->code = new string();
	
	*(this->code) = "__kernel void " + this->name + "(__global const " + this->dataType + " * restrict const input, __global " + this->dataType + " * output, __global const unsigned int * restrict const shifts) {\n"
		"unsigned int dm = (get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + (get_local_id(1) * " + nrDMsPerThread_s + ");\n"
		"unsigned int sample = (get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + get_local_id(0);\n"
		"unsigned int inShMem = 0;\n"
		"unsigned int inGlMem = 0;\n"
		"unsigned int minShift = 0;\n"
		"unsigned int maxShift = 0;\n"
		"__local " + this->dataType + " buffer[" + necessarySharedMemory_s + "];\n" 
		"\n"
		"<%DEFS%>"
		"\n"
		"for ( unsigned int channel = 0; channel < " + nrChannels_s + "; channel++ ) {\n"
		"minShift = shifts[((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") * " + nrPaddedChannels_s + ") + channel];\n"
		"<%SHIFTS%>"
		"maxShift = shifts[(((get_group_id(1) * " + nrTotalDMsPerBlock_s + ") + " + nrTotalDMsPerBlock_s + " - 1) * " + nrPaddedChannels_s + ") + channel];\n"
		"\n"
		"inShMem = (get_local_id(1) * " + nrSamplesPerBlock_s + ") + get_local_id(0);\n"
		"inGlMem = ((get_group_id(0) * " + nrTotalSamplesPerBlock_s + ") + inShMem) + minShift;\n"
		"while ( inShMem < (" + nrTotalSamplesPerBlock_s + " + (maxShift - minShift)) ) {\n"
		"buffer[inShMem] = input[(channel * " + nrSamplesPerDispersedChannel_s + ") + inGlMem];\n"
		"inShMem += " + nrTotalThreads_s + ";\n"
		"inGlMem += " + nrTotalThreads_s + ";\n"
		"}\n"
		"barrier(CLK_LOCAL_MEM_FENCE);\n"
		"\n"
		"<%SUMS%>"
		"}\n"
		"\n"
		"<%STORES%>"
		"}";

	string defsTemplate = this->dataType + " dedispersedSample<%NUM%>DM<%DM_NUM%> = 0;\n";
	string shiftsTemplate = "unsigned int shiftDM<%DM_NUM%> = shifts[((dm + <%DM_NUM%>) * " + nrPaddedChannels_s + ") + channel];\n";
	string sumsTemplate = "dedispersedSample<%NUM%>DM<%DM_NUM%> += buffer[(get_local_id(0) + <%OFFSET%>) + (shiftDM<%DM_NUM%> - minShift)];\n";
	string storesTemplate = "output[((dm + <%DM_NUM%>) * " + nrSamplesPerPaddedSecond_s + ") + (sample + <%OFFSET%>)] = dedispersedSample<%NUM%>DM<%DM_NUM%>;\n";
	// End kernel's template

	string *defs = new string();
	string *shifts = new string();
	string *sums = new string();
	string *stores = new string();
	for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
		string *dm_s = toString< unsigned int >(dm);
		string *temp = 0;

		temp = replace(&shiftsTemplate, "<%DM_NUM%>", *dm_s);
		shifts->append(*temp);
		delete temp;

		delete dm_s;
	}
	for ( unsigned int sample = 0; sample < nrSamplesPerThread; sample++ ) {
		string *sample_s = toString< unsigned int >(sample);
		string *offset_s = toString< unsigned int >(sample * nrSamplesPerBlock);
		string *defsDM = new string();
		string *sumsDM = new string();
		string *storesDM = new string();

		for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
			string *dm_s = toString< unsigned int >(dm);
			string *temp = 0;

			temp = replace(&defsTemplate, "<%DM_NUM%>", *dm_s);
			defsDM->append(*temp);
			delete temp;
			temp = replace(&sumsTemplate, "<%DM_NUM%>", *dm_s);
			sumsDM->append(*temp);
			delete temp;
			temp = replace(&storesTemplate, "<%DM_NUM%>", *dm_s);
			storesDM->append(*temp);
			delete temp;

			delete dm_s;
		}
		defsDM = replace(defsDM, "<%NUM%>", *sample_s, true);
		defs->append(*defsDM);
		delete defsDM;
		sumsDM = replace(sumsDM, "<%NUM%>", *sample_s, true);
		sumsDM = replace(sumsDM, "<%OFFSET%>", *offset_s, true);
		sums->append(*sumsDM);
		delete sumsDM;
		storesDM = replace(storesDM, "<%NUM%>", *sample_s, true);
		storesDM = replace(storesDM, "<%OFFSET%>", *offset_s, true);
		stores->append(*storesDM);
		delete storesDM;
		
		delete offset_s;
		delete sample_s;
	}
	this->code = replace(this->code, "<%DEFS%>", *defs, true);
	this->code = replace(this->code, "<%SHIFTS%>", *shifts, true);
	this->code = replace(this->code, "<%SUMS%>", *sums, true);
	this->code = replace(this->code, "<%STORES%>", *stores, true);
	delete defs;
	delete shifts;
	delete sums;
	delete stores;

	globalSize = cl::NDRange((observation->getNrSamplesPerSecond() / nrSamplesPerThread), (observation->getNrDMs() / nrDMsPerThread));
	localSize = cl::NDRange(nrSamplesPerBlock, nrDMsPerBlock);

	this->compile();
}

template< typename T > void Dedispersion< T >::operator()(CLData< T > *input, CLData< T > *output) throw (OpenCLError) {
	this->setArgument(0, *(input->getDeviceData()));
	this->setArgument(1, *(output->getDeviceData()));
	this->setArgument(2, *(shifts->getDeviceData()));

	this->run(globalSize, localSize);
}
	
template< typename T > inline void Dedispersion< T >::setNrSamplesPerBlock(unsigned int samples) {
	nrSamplesPerBlock = samples;
}

template< typename T > inline void Dedispersion< T >::setNrDMsPerBlock(unsigned int DMs) {
	nrDMsPerBlock = DMs;
}

template< typename T > inline void Dedispersion< T >::setNrSamplesPerThread(unsigned int samples) {
	nrSamplesPerThread = samples;
}

template< typename T > inline void Dedispersion< T >::setNrDMsPerThread(unsigned int DMs) {
	nrDMsPerThread = DMs;
}

template< typename T > inline void Dedispersion< T >::setNrSamplesPerDispersedChannel(unsigned int samples) {
	nrSamplesPerDispersedChannel = samples;
}

template< typename T > inline void Dedispersion< T >::setObservation(Observation< T > *obs) {
	observation = obs;
	this->gflop = giga(static_cast< long long unsigned int >(observation->getNrDMs()) * observation->getNrSamplesPerSecond() * observation->getNrChannels());
}

template< typename T > inline void Dedispersion< T >::setShifts(CLData< unsigned int > *data) {
	shifts = data;
}
	
} // TDM

#endif // DEDISPERSION_HPP
