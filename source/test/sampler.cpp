/*
     This file is part of carolina.

    carolina is free software: you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free Software
   Foundation, either version 3 of the License, or (at your option) any later
   version.

    carolina is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    carolina. If not, see <https://www.gnu.org/licenses/>.
*/

#include <array>

using std::array;

#include <iostream>

using std::cout;
using std::endl;

#include <memory>

using std::dynamic_pointer_cast;
using std::make_unique;
using std::unique_ptr;

#include <numeric>

using std::accumulate;

#include <random>

using std::mt19937;
using std::random_device;
using std::uniform_real_distribution;

#include <boost/program_options.hpp>

namespace po = boost::program_options;

#include "TFile.h"
#include "TGraph.h"
#include "TTree.h"

#include "analysis.hpp"
#include "inverse_calibration.hpp"
#include "progress_printer.hpp"
#include "sampler.hpp"

TGraph invert_energy_calibration(const size_t n_detector,
                                 const size_t n_channel) {
    function<double(const double, const long long)> calibration =
        analysis.energy_sensitive_detectors[n_detector]
            ->channels[n_channel]
            .energy_calibration;
    return invert_calibration<1000>(
        [&calibration](const double amplitude) {
            return calibration(amplitude, 0);
        },
        dynamic_pointer_cast<EnergySensitiveDetectorGroup>(
            analysis.energy_sensitive_detectors[n_detector]->group)
            ->raw_histogram_properties.lower_edge_of_first_bin,
        dynamic_pointer_cast<EnergySensitiveDetectorGroup>(
            analysis.energy_sensitive_detectors[n_detector]->group)
            ->raw_histogram_properties.upper_edge_of_last_bin);
}

TGraph invert_time_calibration(const size_t n_detector, const size_t n_channel,
                               const double tdc_resolution) {
    function<double(const double)> calibration =
        analysis.energy_sensitive_detectors[n_detector]
            ->channels[n_channel]
            .time_calibration;
    return invert_calibration<1000>(
        [&calibration, &tdc_resolution](const double energy) {
            return calibration(energy) / tdc_resolution;
        },
        dynamic_pointer_cast<EnergySensitiveDetectorGroup>(
            analysis.energy_sensitive_detectors[n_detector]->group)
            ->raw_histogram_properties.lower_edge_of_first_bin,
        dynamic_pointer_cast<EnergySensitiveDetectorGroup>(
            analysis.energy_sensitive_detectors[n_detector]->group)
            ->raw_histogram_properties.upper_edge_of_last_bin);
}

void increment_timestamp() {
    for (auto module : analysis.digitizer_modules) {
        module->set_timestamp(module->get_timestamp() + 1.);
    }
}

vector<vector<TGraph>> invert_energy_calibrations() {
    vector<vector<TGraph>> inverse_calibrations;
    for (size_t n_detector = 0;
         n_detector < analysis.energy_sensitive_detectors.size();
         ++n_detector) {
        inverse_calibrations.push_back(vector<TGraph>());
        for (size_t n_channel = 0;
             n_channel <
             analysis.energy_sensitive_detectors[n_detector]->channels.size();
             ++n_channel) {
            inverse_calibrations[n_detector].push_back(
                invert_energy_calibration(n_detector, n_channel));
        }
    }

    return inverse_calibrations;
}

vector<vector<TGraph>> invert_time_calibrations() {
    vector<vector<TGraph>> inverse_calibrations;
    for (size_t n_detector = 0;
         n_detector < analysis.energy_sensitive_detectors.size();
         ++n_detector) {
        inverse_calibrations.push_back(vector<TGraph>());
        for (size_t n_channel = 0;
             n_channel <
             analysis.energy_sensitive_detectors[n_detector]->channels.size();
             ++n_channel) {
            inverse_calibrations[n_detector].push_back(invert_time_calibration(
                n_detector, n_channel,
                analysis.get_tdc_resolution(n_detector, n_channel)));
        }
    }

    return inverse_calibrations;
}

vector<double> split_up_energy(const double energy,
                               const unsigned int n_channels,
                               const vector<double> uniform_random_numbers) {
    if (n_channels == 1) {
        return {energy};
    }
    vector<double> energies(n_channels);

    const double uniform_random_numbers_normalization =
        1. / accumulate(uniform_random_numbers.begin(),
                        uniform_random_numbers.end(), 0.);
    for (unsigned int n_channel = 0; n_channel < n_channels; ++n_channel) {
        energies[n_channel] = energy * uniform_random_numbers[n_channel] *
                              uniform_random_numbers_normalization;
    }
    return energies;
}

void create_counter_event(const size_t n_detector, const size_t n_channel,
                          const long long counter_increment) {
    analysis.add_counts(
        n_detector, n_channel,
        counter_increment /
            dynamic_pointer_cast<ScalerModule>(
                analysis.modules[analysis.counter_detectors[n_detector]
                                     ->channels[n_channel]
                                     .module])
                ->trigger_frequency);
}

void create_single_event(const size_t n_detector, const size_t n_channel,
                         const double gamma_energy,
                         const vector<vector<TGraph>> energy_calibration,
                         const double gamma_time,
                         const vector<vector<TGraph>> time_calibration) {
    analysis.set_amplitude(
        n_detector, n_channel,
        energy_calibration[n_detector][n_channel].Eval(gamma_energy));
    analysis.set_time(n_detector, n_channel,
                      time_calibration[n_detector][n_channel].Eval(gamma_time));
}

void create_single_event_with_addback(
    const size_t n_detector, const double gamma_energy,
    const vector<vector<TGraph>> energy_calibration, const double gamma_time,
    const vector<vector<TGraph>> time_calibration,
    const vector<double> uniform_random_numbers) {
    random_device ran_dev;
    vector<double> energy_depositions = split_up_energy(
        gamma_energy,
        analysis.energy_sensitive_detectors[n_detector]->channels.size(),
        uniform_random_numbers);
    for (size_t n_channel = 0;
         n_channel <
         analysis.energy_sensitive_detectors[n_detector]->channels.size();
         ++n_channel) {
        analysis.set_amplitude(n_detector, n_channel,
                               energy_calibration[n_detector][n_channel].Eval(
                                   energy_depositions[n_channel]));
        analysis.set_time(
            n_detector, n_channel,
            time_calibration[n_detector][n_channel].Eval(gamma_time));
    }
}

double
sample_background_gamma_time(const pair<double, double> range_min_max,
                             const pair<double, double> excluded_range_min_max,
                             const array<double, 2> uniform_random_numbers) {
    const double inverse_range =
        (excluded_range_min_max.first - range_min_max.first) +
        (range_min_max.second - excluded_range_min_max.second);
    if (uniform_random_numbers[0] <
        (excluded_range_min_max.first - range_min_max.first) * inverse_range) {
        return range_min_max.first +
               uniform_random_numbers[1] *
                   (excluded_range_min_max.first - range_min_max.first);
    }
    return excluded_range_min_max.second +
           uniform_random_numbers[1] *
               (range_min_max.second - excluded_range_min_max.second);
}

void set_reference_time(const double reference_time) {
    for (auto module : analysis.digitizer_modules) {
        module->set_reference_time(reference_time / module->tdc_resolution);
    }
}

int main(int argc, char **argv) {
    po::options_description desc(
        "Using the current analysis configuration, fill a TTree with test data "
        "and write it to a ROOT file.");
    desc.add_options()("help", "Produce help message.")(
        "n", po::value<unsigned int>()->default_value(1))(
        "output", po::value<string>()->default_value("test.root"));
    ;

    po::variables_map vm;

    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << endl;
        return 0;
    }

    TFile *output_file =
        new TFile(vm["output"].as<string>().c_str(), "RECREATE");
    TTree *tree = new TTree("test", "test");

    analysis.set_up_raw_branches_for_writing(tree);
    analysis.reset_raw_leaves();

    vector<vector<TGraph>> inverse_time_calibrations =
        invert_time_calibrations();
    vector<vector<TGraph>> inverse_energy_calibrations =
        invert_energy_calibrations();

    const double background_gamma_energy(500.), counter_increment(1000.),
        gamma_energy(1000.), gamma_time(20.), reference_time(10.);

    const unsigned int n_max = vm["n"].as<unsigned int>();
    ProgressPrinter progress_printer(0, n_max - 1, 0.01, "set of events",
                                     "sets of events");

    mt19937 random_engine(0);
    uniform_real_distribution<double> uniform_distribution;
    // Create vectors to store the random numbers.
    // This avoids recreating or resizing vectors in the event loop.
    vector<vector<double>> uniform_random_numbers;
    for (auto detector : analysis.energy_sensitive_detectors) {
        uniform_random_numbers.push_back(
            vector<double>(detector->channels.size()));
    }

    for (unsigned int n = 0; n < n_max; ++n) {
        for (size_t n_detector_1 = 0;
             n_detector_1 < analysis.energy_sensitive_detectors.size();
             ++n_detector_1) {
            // Sample random numbers.
            for (size_t n_channel = 0;
                 n_channel < analysis.energy_sensitive_detectors[n_detector_1]
                                 ->channels.size();
                 ++n_channel) {
                uniform_random_numbers[n_detector_1][n_channel] =
                    uniform_distribution(random_engine);
            }
            // Generate single events that require addback.
            create_single_event_with_addback(
                n_detector_1, gamma_energy, inverse_energy_calibrations,
                gamma_time, inverse_time_calibrations,
                uniform_random_numbers[n_detector_1]);
            set_reference_time(reference_time);
            tree->Fill();
            analysis.reset_raw_leaves();
            increment_timestamp();
            // Generate events in which the entire gamma-ray energy is
            // deposited in a single crystal.
            for (size_t n_channel = 0;
                 n_channel < analysis.energy_sensitive_detectors[n_detector_1]
                                 ->channels.size();
                 ++n_channel) {
                create_single_event(n_detector_1, n_channel, gamma_energy,
                                    inverse_energy_calibrations, gamma_time,
                                    inverse_time_calibrations);
                set_reference_time(reference_time);
                tree->Fill();
                analysis.reset_raw_leaves();
                increment_timestamp();

                create_single_event(n_detector_1, n_channel,
                                    background_gamma_energy,
                                    inverse_energy_calibrations,
                                    sample_background_gamma_time(
                                        {-100., 100.}, {-10., 10.},
                                        {uniform_distribution(random_engine),
                                         uniform_distribution(random_engine)}),
                                    inverse_time_calibrations);
                set_reference_time(reference_time);
                tree->Fill();
                analysis.reset_raw_leaves();
                increment_timestamp();
            }

            for (size_t n_detector_2 = n_detector_1 + 1;
                 n_detector_2 < analysis.energy_sensitive_detectors.size();
                 ++n_detector_2) {
                // Sample random numbers.
                for (size_t n_channel = 0;
                     n_channel <
                     analysis.energy_sensitive_detectors[n_detector_2]
                         ->channels.size();
                     ++n_channel) {
                    uniform_random_numbers[n_detector_2][n_channel] =
                        uniform_distribution(random_engine);
                }
                if (dynamic_pointer_cast<EnergySensitiveDetector>(
                        analysis.detectors[n_detector_2])) {
                    // Create a coincident event in two detectors
                    // that
                    // requires addback.
                    create_single_event_with_addback(
                        n_detector_1, gamma_energy, inverse_energy_calibrations,
                        gamma_time, inverse_time_calibrations,
                        uniform_random_numbers[n_detector_1]);
                    create_single_event_with_addback(
                        n_detector_2, gamma_energy, inverse_energy_calibrations,
                        gamma_time, inverse_time_calibrations,
                        uniform_random_numbers[n_detector_2]);
                    set_reference_time(reference_time);
                    tree->Fill();
                    analysis.reset_raw_leaves();
                    increment_timestamp();
                }
            }
        }
        for (size_t n_detector = 0;
             n_detector < analysis.counter_detectors.size(); ++n_detector) {
            for (size_t n_channel = 0;
                 n_channel <
                 analysis.counter_detectors[n_detector]->channels.size();
                 ++n_channel) {
                create_counter_event(n_detector, n_channel, counter_increment);
            }
            tree->Fill();
            analysis.reset_raw_leaves();
        }
        progress_printer(n);
    }

    tree->Write();
    output_file->Close();
    cout << "Created output file '" << vm["output"].as<string>() << "'."
         << endl;
}