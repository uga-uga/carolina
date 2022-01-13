#include <iostream>

using std::cout;
using std::endl;

#include <memory>

using std::dynamic_pointer_cast;

#include "TChain.h"
#include "TFile.h"
#include "TH2D.h"

#include "command_line_parser.hpp"
#include "energy_sensitive_detector.hpp"
#include "energy_sensitive_detector_channel.hpp"
#include "energy_vs_time.hpp"
#include "progress_printer.hpp"
#include "tfile_utilities.hpp"

int main(int argc, char **argv) {
    CommandLineParser command_line_parser;
    command_line_parser.desc.add_options()(
        "rebin_time", po::value<unsigned int>()->default_value(1),
        "Reduce the number of bins in time histograms by this factor (default: "
        "1, i.e. no reduction).")(
        "rebin_energy", po::value<unsigned int>()->default_value(16),
        "Reduce the number of bins in energy histograms by this factor "
        "(default: 16, i.e. compress 16 bins into 1).");
    int command_line_parser_status;
    command_line_parser(argc, argv, command_line_parser_status);
    if (command_line_parser_status) {
        return 0;
    }
    po::variables_map vm = command_line_parser.get_variables_map();

    long long first, last;
    TChain *tree =
        command_line_parser.set_up_tree(first, last, vm.count("list"));

    ProgressPrinter progress_printer(first, last);

    analysis.set_up_calibrated_branches_for_reading(tree);

    vector<vector<TH2D *>> energy_vs_time_histograms;
    string histogram_name;

    for (size_t n_detector = 0; n_detector < analysis.detectors.size();
         ++n_detector) {
        energy_vs_time_histograms.push_back(vector<TH2D *>());
        if (analysis.detectors[n_detector]->type == energy_sensitive) {
            for (auto channel : analysis.detectors[n_detector]->channels) {
                histogram_name =
                    analysis.detectors[n_detector]->name + "_" + channel->name;
                energy_vs_time_histograms[n_detector].push_back(
                    new TH2D(histogram_name.c_str(), histogram_name.c_str(),
                             analysis.get_group(n_detector)
                                     ->histogram_properties.n_bins /
                                 vm["rebin_energy"].as<unsigned int>(),
                             analysis.get_group(n_detector)
                                 ->histogram_properties.minimum,
                             analysis.get_group(n_detector)
                                 ->histogram_properties.maximum,
                             dynamic_pointer_cast<EnergySensitiveDetectorGroup>(
                                 analysis.get_group(n_detector))
                                     ->time_histogram_properties.n_bins /
                                 vm["rebin_time"].as<unsigned int>(),
                             dynamic_pointer_cast<EnergySensitiveDetectorGroup>(
                                 analysis.get_group(n_detector))
                                 ->time_histogram_properties.n_bins,
                             dynamic_pointer_cast<EnergySensitiveDetectorGroup>(
                                 analysis.get_group(n_detector))
                                 ->time_histogram_properties.n_bins));
            }
        }
    }

    for (long long i = first; i <= last; ++i) {
        tree->GetEntry(i);

        for (size_t n_detector = 0; n_detector < analysis.detectors.size();
             ++n_detector) {
            if (analysis.detectors[n_detector]->type == energy_sensitive) {
                for (size_t n_channel = 0;
                     n_channel <
                     analysis.detectors[n_detector]->channels.size();
                     ++n_channel) {
                    if (!isnan(dynamic_pointer_cast<
                                   EnergySensitiveDetectorChannel>(
                                   analysis.detectors[n_detector]
                                       ->channels[n_channel])
                                   ->energy_calibrated) &&
                        dynamic_pointer_cast<EnergySensitiveDetectorChannel>(
                            analysis.detectors[n_detector]->channels[n_channel])
                            ->time_vs_time_RF_gate(
                                dynamic_pointer_cast<
                                    EnergySensitiveDetectorChannel>(
                                    analysis.detectors[n_detector]
                                        ->channels[n_channel])
                                    ->time_vs_time_RF_calibrated)) {
                        energy_vs_time_histograms[n_detector][n_channel]->Fill(
                            dynamic_pointer_cast<
                                EnergySensitiveDetectorChannel>(
                                analysis.detectors[n_detector]
                                    ->channels[n_channel])
                                ->energy_calibrated,
                            dynamic_pointer_cast<
                                EnergySensitiveDetectorChannel>(
                                analysis.detectors[n_detector]
                                    ->channels[n_channel])
                                ->time_calibrated);
                    }
                }
            }
        }
        progress_printer(i);
    }

    TFile output_file(vm["output"].as<string>().c_str(), "RECREATE");

    for (size_t n_detector = 0; n_detector < analysis.detectors.size();
         ++n_detector) {
        if (analysis.detectors[n_detector]->type == energy_sensitive) {
            for (size_t n_channel = 0;
                 n_channel < analysis.detectors[n_detector]->channels.size();
                 ++n_channel) {
                energy_vs_time_histograms[n_detector][n_channel]->Write();
            }
        }
    }

    output_file.Close();
    cout << "Created output file '" << vm["output"].as<string>() << "'."
         << endl;
}