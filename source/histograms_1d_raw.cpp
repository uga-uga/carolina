#include <iostream>

using std::cout;
using std::endl;

#include "TChain.h"
#include "TFile.h"
#include "TH1D.h"

#include "command_line_parser.hpp"
#include "histograms_1d.hpp"
#include "progress_printer.hpp"
#include "tfile_utilities.hpp"

int main(int argc, char **argv) {
    CommandLineParser command_line_parser;
    int command_line_parser_status;
    command_line_parser(argc, argv, command_line_parser_status);
    if (command_line_parser_status) {
        return 0;
    }
    po::variables_map vm = command_line_parser.get_variables_map();

    TChain *tree =
        new TChain(find_tree_in_file(vm["input_file"].as<vector<string>>()[0],
                                     vm["tree"].as<string>())
                       .c_str());
    vector<string> input_files = vm["input_file"].as<vector<string>>();
    for (auto input_file : input_files) {
        cout << "Adding '" << input_file.c_str() << "' to TChain." << endl;
        tree->Add(input_file.c_str());
    }

    const int first = vm["first"].as<int>();
    const int last =
        vm["last"].as<int>() == 0 ? tree->GetEntries() : vm["last"].as<int>();

    if (first > last) {
        cout << "Error: first entry (" << first
             << ") is larger or equal to last entry (" << last
             << "). Aborting ..." << endl;
        return 0;
    }

    ProgressPrinter progress_printer(last - first + 1, 0.001);

    for (size_t i = 0; i < modules.size(); ++i) {
        modules[i]->activate_branches(tree);
        modules[i]->register_branches(tree);
    }

    vector<vector<TH1D *>> energy_raw_histograms;
    string histogram_name;

    for (size_t n_detector_1 = 0;
         n_detector_1 < detector_setup.detectors.size(); ++n_detector_1) {
        energy_raw_histograms.push_back(vector<TH1D *>());
        for (size_t n_channel_1 = 0;
             n_channel_1 <
             detector_setup.detectors[n_detector_1].channels.size();
             ++n_channel_1) {

            histogram_name = detector_setup.detectors[n_detector_1].name + "_" +
                             detector_setup.detectors[n_detector_1]
                                 .channels[n_channel_1]
                                 .name;
            energy_raw_histograms[n_detector_1].push_back(
                new TH1D(histogram_name.c_str(), histogram_name.c_str(),
                         detector_setup.detectors[n_detector_1]
                             .group.energy_raw_histogram_properties.n_bins,
                         detector_setup.detectors[n_detector_1]
                             .group.energy_raw_histogram_properties.minimum,
                         detector_setup.detectors[n_detector_1]
                             .group.energy_raw_histogram_properties.maximum));
        }
    }

    for (int i = first; i <= last; ++i) {
        progress_printer(i - first);

        tree->GetEntry(i);

        for (size_t n_detector_1 = 0;
             n_detector_1 < detector_setup.detectors.size(); ++n_detector_1) {
            for (size_t n_channel_1 = 0;
                 n_channel_1 <
                 detector_setup.detectors[n_detector_1].channels.size();
                 ++n_channel_1) {
                if (detector_setup.detectors[n_detector_1]
                            .channels[n_channel_1]
                            .get_amplitude() >
                        detector_setup.detectors[n_detector_1]
                            .channels[n_channel_1]
                            .amplitude_threshold &&
                    detector_setup.detectors[n_detector_1]
                            .channels[n_channel_1]
                            .get_time() > 0.) {
                    energy_raw_histograms[n_detector_1][n_channel_1]->Fill(
                        detector_setup.detectors[n_detector_1]
                            .channels[n_channel_1]
                            .get_amplitude());
                }
            }
        }
    }

    TFile output_file(vm["output_file"].as<string>().c_str(), "RECREATE");

    for (size_t n_detector_1 = 0;
         n_detector_1 < detector_setup.detectors.size(); ++n_detector_1) {
        for (size_t n_channel_1 = 0;
             n_channel_1 <
             detector_setup.detectors[n_detector_1].channels.size();
             ++n_channel_1) {
            energy_raw_histograms[n_detector_1][n_channel_1]->Write();
        }
    }

    output_file.Close();
    cout << "Created output file '" << vm["output_file"].as<string>() << "'."
         << endl;
}