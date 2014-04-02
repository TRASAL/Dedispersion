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

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
#include <string>
using std::string;
#include <exception>
using std::exception;

#include <ArgumentList.hpp>
using isa::utils::ArgumentList;
#include <utils.hpp>
using isa::utils::toStringValue;
using isa::utils::replace;

void generatorPhi(const unsigned int nrSamplesPerThread, const unsigned int nrDMsPerThread);
void generatorAVX(const unsigned int nrSamplesPerThread, const unsigned int nrDMsPerThread);


int main(int argc, char * argv[]) {
	bool usePhi = false;
	unsigned int nrSamplesPerThread = 0;
	unsigned int nrDMsPerThread = 0;

	try {
		ArgumentList args(argc, argv);

		usePhi = args.getSwitch("-phi");
		nrSamplesPerThread = args.getSwitchArgument< unsigned int >("-spt");
		nrDMsPerThread = args.getSwitchArgument< unsigned int >("-dpt");
	} catch ( exception & err ) {
		cerr << err.what() << endl;
		return 1;
	}

	if ( usePhi ) {
		generatorPhi(nrSamplesPerThread, nrDMsPerThread);
	} else {
		generatorAVX(nrSamplesPerThread, nrDMsPerThread);
	}

	return 0;
}

void generatorPhi(const unsigned int nrSamplesPerThread, const unsigned int nrDMsPerThread) {
	string * code = new string();
	*code = "namespace PulsarSearch {\n"
		"template< typename T > void dedispersionPhi" + toStringValue< unsigned int >(nrSamplesPerThread) + "x" + toStringValue< unsigned int >(nrDMsPerThread) + "(const unsigned int nrSamplesPerChannel, const unsigned int nrDMs, const unsigned int nrSamplesPerSecond, const unsigned int nrChannels, const unsigned int nrSamplesPerPaddedSecond, const T  * const __restrict__ input, T * const __restrict__ output, const unsigned int * const __restrict__ shifts) {\n"
		"#pragma omp parallel for schedule(static)\n"
		"for ( unsigned int dm = 0; dm < nrDMs; dm += " + toStringValue< unsigned int >(nrDMsPerThread) + " ) {\n"
			"#pragma omp parallel for schedule(static)\n"
			"for ( unsigned int sample = 0; sample < nrSamplesPerSecond; sample += 16 * " + toStringValue< unsigned int >(nrSamplesPerThread) + ") {\n"
				"<%DEFS%>"
				"\n"
				"for ( unsigned int channel = 0; channel < nrChannels; channel++ ) {\n"
					"<%SHIFTS%>"
					"__m512 dispersedSample;\n"
					"\n"
					"<%SUMS%>"
				"}\n"
				"\n"
				"<%STORES%>"

			"}\n"
		"}\n"
	"}\n"
	"}";
	string defsTemplate = "__m512 dedispersedSample<%NUM%>DM<%DM_NUM%> = _mm512_setzero_ps();\n";
	string shiftsTemplate = "unsigned int shiftDM<%DM_NUM%> = shifts[((dm + <%DM_NUM%>) * nrChannels) + channel];";
	string sumsTemplate = "dispersedSample = _mm512_loadunpacklo_ps(dispersedSample, &(input[(channel * nrSamplesPerChannel) + ((sample + <%OFFSET%>) + shiftDM<%DM_NUM%>)]));\n"
		"dispersedSample = _mm512_loadunpackhi_ps(dispersedSample, &(input[(channel * nrSamplesPerChannel) + ((sample + <%OFFSET%>) + shiftDM<%DM_NUM%>)]) + 16);\n"
		"dedispersedSample<%NUM%>DM<%DM_NUM%> = _mm512_add_ps(dedispersedSample<%NUM%>DM<%DM_NUM%>, dispersedSample);\n";
	string storesTemplate = "_mm512_packstorelo_ps(&(output[((dm + <%DM_NUM%>) * nrSamplesPerPaddedSecond) + (sample + <%OFFSET%>)]), dedispersedSample<%NUM%>DM<%DM_NUM%>);\n"
		"_mm512_packstorehi_ps(&(output[((dm + <%DM_NUM%>) * nrSamplesPerPaddedSecond) + (sample + <%OFFSET%>)]) + 16, dedispersedSample<%NUM%>DM<%DM_NUM%>);\n";

	string * defs = new string();
	string * shifts = new string();
	string * sums = new string();
	string * stores = new string();
	for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
		string dm_s = toStringValue< unsigned int >(dm);
		string * temp = 0;

		temp = replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
		shifts->append(*temp);
		delete temp;
	}
	for ( unsigned int sample = 0; sample < nrSamplesPerThread; sample++ ) {
		string sample_s = toStringValue< unsigned int >(sample);
		string offset_s = toStringValue< unsigned int >(sample * 16);
		string * defsDM = new string();
		string * sumsDM = new string();
		string * storesDM = new string();

		for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
			string dm_s = toStringValue< unsigned int >(dm);
			string * temp = 0;

			temp = replace(&defsTemplate, "<%DM_NUM%>", dm_s);
			defsDM->append(*temp);
			delete temp;
			temp = replace(&sumsTemplate, "<%DM_NUM%>", dm_s);
			sumsDM->append(*temp);
			delete temp;
			temp = replace(&storesTemplate, "<%DM_NUM%>", dm_s);
			storesDM->append(*temp);
			delete temp;
		}
		defsDM = replace(defsDM, "<%NUM%>", sample_s, true);
		defs->append(*defsDM);
		delete defsDM;
		sumsDM = replace(sumsDM, "<%NUM%>", sample_s, true);
		sumsDM = replace(sumsDM, "<%OFFSET%>", offset_s, true);
		sums->append(*sumsDM);
		delete sumsDM;
		storesDM = replace(storesDM, "<%NUM%>", sample_s, true);
		storesDM = replace(storesDM, "<%OFFSET%>", offset_s, true);
		stores->append(*storesDM);
		delete storesDM;
	}
	code = replace(code, "<%DEFS%>", *defs, true);
	code = replace(code, "<%SHIFTS%>", *shifts, true);
	code = replace(code, "<%SUMS%>", *sums, true);
	code = replace(code, "<%STORES%>", *stores, true);
	delete defs;
	delete shifts;
	delete sums;
	delete stores;

	cout << *code << endl;
}

void generatorAVX(const unsigned int nrSamplesPerThread, const unsigned int nrDMsPerThread) {
	string * code = new string();
	*code = "namespace PulsarSearch {\n"
		"template< typename T > void dedispersionAVX" + toStringValue< unsigned int >(nrSamplesPerThread) + "x" + toStringValue< unsigned int >(nrDMsPerThread) + "(const unsigned int nrSamplesPerChannel, const unsigned int nrDMs, const unsigned int nrSamplesPerSecond, const unsigned int nrChannels, const unsigned int nrSamplesPerPaddedSecond, const T  * const __restrict__ input, T * const __restrict__ output, const unsigned int * const __restrict__ shifts) {\n"
		"#pragma omp parallel for schedule(static)\n"
		"for ( unsigned int dm = 0; dm < nrDMs; dm += " + toStringValue< unsigned int >(nrDMsPerThread) + " ) {\n"
			"#pragma omp parallel for schedule(static)\n"
			"for ( unsigned int sample = 0; sample < nrSamplesPerSecond; sample += 8 * " + toStringValue< unsigned int >(nrSamplesPerThread) + ") {\n"
				"<%DEFS%>"
				"\n"
				"for ( unsigned int channel = 0; channel < nrChannels; channel++ ) {\n"
					"<%SHIFTS%>"
					"__m256 dispersedSample;\n"
					"\n"
					"<%SUMS%>"
				"}\n"
				"\n"
				"<%STORES%>"

			"}\n"
		"}\n"
	"}\n"
	"}";
	string defsTemplate = "__m256 dedispersedSample<%NUM%>DM<%DM_NUM%> = _mm256_setzero_ps();\n";
	string shiftsTemplate = "unsigned int shiftDM<%DM_NUM%> = shifts[((dm + <%DM_NUM%>) * nrChannels) + channel];";
	string sumsTemplate = "dispersedSample = _mm256_loadu_ps(&(input[(channel * nrSamplesPerChannel) + ((sample + <%OFFSET%>) + shiftDM<%DM_NUM%>)]));\n"
		"dedispersedSample<%NUM%>DM<%DM_NUM%> = _mm256_add_ps(dedispersedSample<%NUM%>DM<%DM_NUM%>, dispersedSample);\n";
	string storesTemplate = "_mm256_store_ps(&(output[((dm + <%DM_NUM%>) * nrSamplesPerPaddedSecond) + (sample + <%OFFSET%>)]), dedispersedSample<%NUM%>DM<%DM_NUM%>);\n";

	string * defs = new string();
	string * shifts = new string();
	string * sums = new string();
	string * stores = new string();
	for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
		string dm_s = toStringValue< unsigned int >(dm);
		string * temp = 0;

		temp = replace(&shiftsTemplate, "<%DM_NUM%>", dm_s);
		shifts->append(*temp);
		delete temp;
	}
	for ( unsigned int sample = 0; sample < nrSamplesPerThread; sample++ ) {
		string sample_s = toStringValue< unsigned int >(sample);
		string offset_s = toStringValue< unsigned int >(sample * 16);
		string * defsDM = new string();
		string * sumsDM = new string();
		string * storesDM = new string();

		for ( unsigned int dm = 0; dm < nrDMsPerThread; dm++ ) {
			string dm_s = toStringValue< unsigned int >(dm);
			string * temp = 0;

			temp = replace(&defsTemplate, "<%DM_NUM%>", dm_s);
			defsDM->append(*temp);
			delete temp;
			temp = replace(&sumsTemplate, "<%DM_NUM%>", dm_s);
			sumsDM->append(*temp);
			delete temp;
			temp = replace(&storesTemplate, "<%DM_NUM%>", dm_s);
			storesDM->append(*temp);
			delete temp;
		}
		defsDM = replace(defsDM, "<%NUM%>", sample_s, true);
		defs->append(*defsDM);
		delete defsDM;
		sumsDM = replace(sumsDM, "<%NUM%>", sample_s, true);
		sumsDM = replace(sumsDM, "<%OFFSET%>", offset_s, true);
		sums->append(*sumsDM);
		delete sumsDM;
		storesDM = replace(storesDM, "<%NUM%>", sample_s, true);
		storesDM = replace(storesDM, "<%OFFSET%>", offset_s, true);
		stores->append(*storesDM);
		delete storesDM;
	}
	code = replace(code, "<%DEFS%>", *defs, true);
	code = replace(code, "<%SHIFTS%>", *shifts, true);
	code = replace(code, "<%SUMS%>", *sums, true);
	code = replace(code, "<%STORES%>", *stores, true);
	delete defs;
	delete shifts;
	delete sums;
	delete stores;

	cout << *code << endl;
}
