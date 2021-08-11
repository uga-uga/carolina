#include <iostream>

using std::cout;
using std::endl;

#include "TChain.h"
#include "TFile.h"
#include "TH1D.h"

#include "command_line_parser.hpp"
#include "histograms_1d.hpp"
#include "progress_printer.hpp"

int main(int argc, char** argv){
    CommandLineParser command_line_parser;
    command_line_parser(argc, argv);
    po::variables_map vm = command_line_parser.get_variables_map();

    TChain *tree = new TChain(vm["tree"].as<string>().c_str());
    vector<string> input_files = vm["input_file"].as<vector<string>>();
    for(auto input_file: input_files){
        cout << "Adding '" << input_file.c_str() << "' to TChain." << endl;
        tree->Add(input_file.c_str());
    }

    const int first = vm["first"].as<int>();
    const int last = vm["last"].as<int>() == 0 ? tree->GetEntries() : vm["last"].as<int>();

    if(first > last){
        cout << "Error: first entry (" << first << ") is larger or equal to last entry (" << last << "). Aborting ..." << endl;
	    return 0;
    }

    ProgressPrinter progress_printer(last-first+1, 0.001);

    for(size_t i = 0; i < modules.size(); ++i){
        modules[i].register_branches(tree);
    }

    vector<TH1D*> addback_histograms;
    vector<vector<TH1D*>> energy_histograms;
    vector<vector<TH1D*>> energy_raw_histograms;
    vector<vector<vector<vector<TH1D*>>>> time_difference_histograms;
    string histogram_name;

    for(size_t n_detector_1 = 0; n_detector_1 < detectors.size(); ++n_detector_1){
        if(detectors[n_detector_1].channels.size() > 1){
            histogram_name = detectors[n_detector_1].name + "_addback";
            addback_histograms.push_back(new TH1D(histogram_name.c_str(), histogram_name.c_str(), detectors[n_detector_1].group.energy_histogram_properties.n_bins, detectors[n_detector_1].group.energy_histogram_properties.minimum, detectors[n_detector_1].group.energy_histogram_properties.maximum));
        } else {
            addback_histograms.push_back(nullptr);
        }
        energy_histograms.push_back(vector<TH1D*>());
        energy_raw_histograms.push_back(vector<TH1D*>());
        time_difference_histograms.push_back(vector<vector<vector<TH1D*>>>());
        for(size_t n_channel_1 = 0; n_channel_1 < detectors[n_detector_1].channels.size(); ++n_channel_1){
            time_difference_histograms[n_detector_1].push_back(vector<vector<TH1D*>>());
            histogram_name = detectors[n_detector_1].name + "_" + detectors[n_detector_1].channels[n_channel_1].name;
            energy_histograms[n_detector_1].push_back(new TH1D(histogram_name.c_str(), histogram_name.c_str(), detectors[n_detector_1].group.energy_histogram_properties.n_bins, detectors[n_detector_1].group.energy_histogram_properties.minimum, detectors[n_detector_1].group.energy_histogram_properties.maximum));

            histogram_name = histogram_name + "_raw";
            energy_raw_histograms[n_detector_1].push_back(new TH1D(histogram_name.c_str(), histogram_name.c_str(), detectors[n_detector_1].group.energy_raw_histogram_properties.n_bins, detectors[n_detector_1].group.energy_raw_histogram_properties.minimum, detectors[n_detector_1].group.energy_raw_histogram_properties.maximum));

            for(size_t n_detector_2 = n_detector_1 + 1; n_detector_2 < detectors.size(); ++n_detector_2){
                time_difference_histograms[n_detector_1][n_channel_1].push_back(vector<TH1D*>());
                for(size_t n_channel_2 = 0; n_channel_2 < detectors[n_detector_2].channels.size(); ++n_channel_2){
                    histogram_name = detectors[n_detector_1].name + "_" + detectors[n_detector_1].channels[n_channel_1].name + "_" + detectors[n_detector_2].name + "_" + detectors[n_detector_2].channels[n_channel_2].name + "_tdiff";
                    time_difference_histograms[n_detector_1][n_channel_1][n_detector_2-n_detector_1-1].push_back(new TH1D(histogram_name.c_str(), histogram_name.c_str(), detectors[n_detector_1].group.time_difference_histogram_properties.n_bins, detectors[n_detector_1].group.time_difference_histogram_properties.minimum, detectors[n_detector_1].group.time_difference_histogram_properties.maximum));
                }
            }
        }
    }

    for(int i = first; i <= last; ++i){
        progress_printer(i - first);

        tree->GetEntry(i);

        for(size_t n_detector_1 = 0; n_detector_1 < detectors.size(); ++n_detector_1){
            for(size_t n_channel_1 = 0; n_channel_1 < detectors[n_detector_1].channels.size(); ++n_channel_1){
                if(detectors[n_detector_1].channels[n_channel_1].get_amplitude() > detectors[n_detector_1].channels[n_channel_1].amplitude_threshold){
                    energy_raw_histograms[n_detector_1][n_channel_1]->Fill(detectors[n_detector_1].channels[n_channel_1].get_amplitude());
                    detectors[n_detector_1].channels[n_channel_1].calibrate(i);
                    if(detectors[n_detector_1].channels[n_channel_1].energy_calibrated){
                        energy_histograms[n_detector_1][n_channel_1]->Fill(detectors[n_detector_1].channels[n_channel_1].energy_calibrated);
                    }
                    for(size_t n_detector_2 = n_detector_1+1; n_detector_2 < detectors.size(); ++n_detector_2){
                        for(size_t n_channel_2 = 0; n_channel_2 < detectors[n_detector_2].channels.size(); ++n_channel_2){
                            if(detectors[n_detector_2].channels[n_channel_2].get_amplitude() > detectors[n_detector_2].channels[n_channel_2].amplitude_threshold){
                                time_difference_histograms[n_detector_1][n_channel_1][n_detector_2-n_detector_1-1][n_channel_2]->Fill(detectors[n_detector_1].channels[n_channel_1].time_calibrated-detectors[n_detector_2].channels[n_channel_2].time_calibrated);
                            }
                        }
                    }
                }
            }
            if(detectors[n_detector_1].channels.size() > 1){
                detectors[n_detector_1].addback();
                for(size_t n_channel_1 = 0; n_channel_1 < detectors[n_detector_1].channels.size(); ++n_channel_1){
                    if(detectors[n_detector_1].addback_energies[n_channel_1] > detectors[n_detector_1].channels[n_channel_1].amplitude_threshold){
                        addback_histograms[n_detector_1]->Fill(detectors[n_detector_1].addback_energies[n_channel_1]);
                    }
                }
            }
        }
    }

    TFile output_file(vm["output_file"].as<string>().c_str(), "RECREATE");

    for(size_t n_detector_1 = 0; n_detector_1 < detectors.size(); ++n_detector_1){
        if(detectors[n_detector_1].channels.size() > 1){
            addback_histograms[n_detector_1]->Write();
        }
        for(size_t n_channel_1 = 0; n_channel_1 < detectors[n_detector_1].channels.size(); ++n_channel_1){
            energy_histograms[n_detector_1][n_channel_1]->Write();
            energy_raw_histograms[n_detector_1][n_channel_1]->Write();
            for(size_t n_detector_2 = n_detector_1 + 1; n_detector_2 < detectors.size(); ++n_detector_2){
                for(size_t n_channel_2 = 0; n_channel_2 < detectors[n_detector_1].channels.size(); ++n_channel_2){
                    time_difference_histograms[n_detector_1][n_channel_1][n_detector_2-n_detector_1-1][n_channel_2]->Write();
                }
            }
        }
    }

    output_file.Close();
    cout << "Created output file '" << vm["output_file"].as<string>() << "'." << endl;
}