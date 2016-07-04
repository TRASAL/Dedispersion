// Copyright 2016 Alessio Sclocco <a.sclocco@vu.nl>
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
#include <vector>
#include <ctime>
#include <algorithm>

#include <configuration.hpp>

#include <Dedispersion.hpp>
#include <ArgumentList.hpp>
#include <Observation.hpp>
#include <utils.hpp>
#include <Shifts.hpp>
#include <ReadData.hpp>
#include <utils.hpp>


int main(int argc, char * argv[]) {
  unsigned int padding = 0;
  uint8_t inputBits = 0;
  bool printResults = false;
  uint64_t wrongSamples = 0;
  std::string channelsFile;
  AstroData::Observation observation_c;
  AstroData::Observation observation;

  try {
    isa::utils::ArgumentList args(argc, argv);
    printResults = args.getSwitch("-print_results");
    inputBits = args.getSwitchArgument< unsigned int >("-input_bits");
    padding = args.getSwitchArgument< unsigned int >("-padding");
    channelsFile = args.getSwitchArgument< std::string >("-zapped_channels");
    observation.setNrBeams(args.getSwitchArgument< unsigned int >("-beams"));
    observation.setNrSyntheticBeams(args.getSwitchArgument< unsigned int >("-synthetic_beams"));
    observation.setFrequencyRange(args.getSwitchArgument< unsigned int >("-subbands"), args.getSwitchArgument< unsigned int >("-channels"), args.getSwitchArgument< float >("-min_freq"), args.getSwitchArgument< float >("-channel_bandwidth"));
    observation.setNrSamplesPerBatch(args.getSwitchArgument< unsigned int >("-samples"));
    observation.setDMSubbandingRange(args.getSwitchArgument< unsigned int >("-subbanding_dms"), args.getSwitchArgument< float >("-subbanding_dm_first"), args.getSwitchArgument< float >("-subbanding_dm_step"));
    observation.setDMRange(args.getSwitchArgument< unsigned int >("-dms"), args.getSwitchArgument< float >("-dm_first"), args.getSwitchArgument< float >("-dm_step"));
  } catch  ( isa::utils::SwitchNotFound & err ) {
    std::cerr << err.what() << std::endl;
    return 1;
  } catch ( std::exception & err ) {
    std::cerr << "Usage: " << argv[0] << " [-print_results] -input_bits ... -padding ... -zapped_channels ... -beams ... -synthetic_beams ... -min_freq ... -channel_bandwidth ... -samples ... -subbands ... -channels ... -subbanding_dms ... -dms ... -subbanding_dm_first ... -dm_first ... -subbanding_dm_step ... -dm_step ..." << std::endl;
    return 1;
  }
  observation_c = observation;
  observation_c.setDMRange(observation.getNrDMsSubbanding() * observation.getNrDMs(), observation.getFirstDMSubbanding() + observation.getFirstDM(), observation.getDMStep());

  // Allocate memory
  std::vector< inputDataType > dispersedData;
  std::vector< outputDataType > subbandedData;
  std::vector< outputDataType > dedispersedData;
  std::vector< outputDataType > dedispersedData_c;
  std::vector< uint8_t > zappedChannels(observation_c.getNrPaddedChannels(padding / sizeof(uint8_t)));
  std::vector< uint8_t > beamDriver(observation_c.getNrSyntheticBeams() * observation_c.getNrPaddedChannels(padding / sizeof(uint8_t)));
  std::vector< float > * shifts = PulsarSearch::getShifts(observation_c, padding);

  AstroData::readZappedChannels(observation_c, channelsFile, zappedChannels);
  observation_c.setNrSamplesPerDispersedChannel(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation_c.getFirstDM() + ((observation_c.getNrDMs() - 1) * observation_c.getDMStep()))));
  observation.setNrSamplesPerBatchSubbanding(observation.getNrSamplesPerBatch() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDM() + ((observation.getNrDMs() - 1) * observation.getDMStep()))));
  observation.setNrSamplesPerSubbandingDispersedChannel(observation.getNrSamplesPerBatchSubbanding() + static_cast< unsigned int >(shifts->at(0) * (observation.getFirstDMSubbanding() + ((observation.getNrDMsSubbanding() - 1) * observation.getDMSubbandingStep()))));
  if ( inputBits >= 8 ) {
    dispersedData.resize(observation_c.getNrBeams() * observation_c.getNrChannels() * observation_c.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType)));
    subbandedData.resize(observation.getNrBeams() * observation.getNrSubbands() * observation.getNrSamplesPerPaddedSubbandingDispersedChannel(padding / sizeof(inputDataType)));
    dedispersedData.resize(observation.getNrSyntheticBeams() * (observation.getNrDMsSubbanding() * observation.getNrDMs()) * observation.getNrSamplesPerPaddedBatch(padding / sizeof(inputDataType)));
    dedispersedData_c.resize(observation_c.getNrSyntheticBeams() * observation_c.getNrDMs() * observation_c.getNrSamplesPerPaddedBatch(padding / sizeof(inputDataType)));
  } else {
    dispersedData.resize(observation_c.getNrBeams() * observation_c.getNrChannels() * isa::utils::pad(observation_c.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType)));
    subbandedData.resize(observation.getNrBeams() * observation.getNrSubbands() * isa::utils::pad(observation.getNrSamplesPerSubbandingDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType)));
    dedispersedData.resize(observation.getNrSyntheticBeams() * (observation.getNrDMsSubbanding() * observation.getNrDMs()) * isa::utils::pad(observation.getNrSamplesPerBatch() / (8 / inputBits), padding / sizeof(inputDataType)));
    dedispersedData_c.resize(observation_c.getNrSyntheticBeams() * observation_c.getNrDMs() * isa::utils::pad(observation_c.getNrSamplesPerBatch() / (8 / inputBits), padding / sizeof(inputDataType)));
  }

  // Generate data
  srand(time(0));
  for ( unsigned int beam = 0; beam < observation_c.getNrBeams(); beam++ ) {
    for ( unsigned int channel = 0; channel < observation_c.getNrChannels(); channel++ ) {
      for ( unsigned int sample = 0; sample < observation_c.getNrSamplesPerDispersedChannel(); sample++ ) {
        if ( inputBits >= 8 ) {
          dispersedData[(beam * observation_c.getNrChannels() * observation_c.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType))) + (channel * observation_c.getNrSamplesPerPaddedDispersedChannel(padding / sizeof(inputDataType))) + sample] = rand() % observation_c.getNrChannels();
        } else {
          unsigned int byte = 0;
          uint8_t firstBit = 0;
          uint8_t value = rand() % inputBits;
          uint8_t buffer = 0;

          byte = sample / (8 / inputBits);
          firstBit = (sample % (8 / inputBits)) * inputBits;
          buffer = dispersedData[(beam * observation_c.getNrChannels() * isa::utils::pad(observation_c.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation_c.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + byte];
          for ( unsigned int bit = 0; bit < inputBits; bit++ ) {
            isa::utils::setBit(buffer, isa::utils::getBit(value, bit), firstBit + bit);
          }
          dispersedData[(beam * observation_c.getNrChannels() * isa::utils::pad(observation_c.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + (channel * isa::utils::pad(observation_c.getNrSamplesPerDispersedChannel() / (8 / inputBits), padding / sizeof(inputDataType))) + byte] = buffer;
        }
      }
    }
  }
  for ( unsigned int beam = 0; beam < observation_c.getNrSyntheticBeams(); beam++ ) {
    for ( unsigned int channel = 0; channel < observation_c.getNrChannels(); channel++ ) {
      beamDriver[(beam * observation_c.getNrPaddedChannels(padding / sizeof(uint8_t))) + channel] = rand() % observation_c.getNrBeams();
    }
  }
  std::fill(subbandedData.begin(), subbandedData.end(), 0);
  std::fill(dedispersedData.begin(), dedispersedData.end(), 0);
  std::fill(dedispersedData_c.begin(), dedispersedData_c.end(), 0);

  // Execute dedispersion
  PulsarSearch::dedispersion< inputDataType, intermediateDataType, outputDataType >(observation_c, zappedChannels, beamDriver, dispersedData, dedispersedData_c, *shifts, padding, inputBits);
  PulsarSearch::subbandDedispersionStepOne< inputDataType, intermediateDataType, outputDataType >(observation, zappedChannels, beamDriver, dispersedData, subbandedData, *shifts, padding, inputBits);
  PulsarSearch::subbandDedispersionStepTwo< outputDataType, intermediateDataType, outputDataType >(observation, zappedChannels, beamDriver, subbandedData, dedispersedData, *shifts, padding, inputBits);

  for ( unsigned int beam = 0; beam < observation_c.getNrSyntheticBeams(); beam++ ) {
    for ( unsigned int dm = 0; dm < observation_c.getNrDMs(); dm++ ) {
      for ( unsigned int sample = 0; sample < observation_c.getNrSamplesPerBatch(); sample++ ) {
        if ( inputBits >= 8 ) {
          if ( !same(dedispersedData[], dedispersedData_c[]) ) {
            wrongSamples++;
          }
          if ( printResults ) {
          }
        } else {
        }
      }
    }
  }

  if ( wrongSamples > 0 ) {
    std::cout << "Wrong samples: " << wrongSamples << " (" << (wrongSamples * 100.0) / (static_cast< uint64_t >(observation_c.getNrSyntheticBeams()) * observation_c.getNrDMs() * observation_c.getNrSamplesPerBatch()) << "%)." << std::endl;
  } else {
    std::cout << "TEST PASSED." << std::endl;
  }

  return 0;
}

