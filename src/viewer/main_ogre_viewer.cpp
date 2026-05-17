#include "viewer/csv_result_reader.h"
#include "viewer/ogre_ecosystem_viewer.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    std::optional<std::string> csv_path;
    std::vector<std::string> args(argv + 1, argv + argc);
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            std::cout << "Usage: ecosim_ogre_viewer [output/simulation.csv]\n"
                      << "       ecosim_ogre_viewer --input <csv>\n"
                      << "       ecosim_ogre_viewer --csv <csv>\n";
            return 0;
        }
        if (args[i] == "--input" || args[i] == "--csv" || args[i] == "--file" || args[i] == "-i") {
            if (i + 1 >= args.size()) {
                std::cerr << args[i] << " requires a CSV path" << std::endl;
                return 1;
            }
            csv_path = args[++i];
            continue;
        }
        if (!args[i].empty() && args[i][0] != '-') {
            csv_path = args[i];
        }
    }
    const std::string selected_csv_path = csv_path.value_or("output/simulation.csv");

    ecosim::viewer::CsvResultReader reader;
    const auto frames = reader.read(selected_csv_path);
    if (frames.empty()) {
        std::cerr << "No frames loaded from " << selected_csv_path << std::endl;
        return 1;
    }

    const bool has_state_columns = std::any_of(frames.begin(), frames.end(), [](const auto &frame) {
        return !frame.species_names.empty() && frame.species_names.size() == frame.state_values.size();
    });
    if (!has_state_columns) {
        std::cerr << "CSV does not contain extended state.<species> columns. "
                  << "The OGRE viewer needs RecorderCsv extended output, not legacy tick/seed/energy_total CSV."
                  << std::endl;
        return 1;
    }

    ecosim::viewer::OgreEcosystemViewer viewer;
    if (!viewer.initialize()) {
        return 1;
    }

    viewer.run(frames);
    viewer.shutdown();
    return 0;
}
