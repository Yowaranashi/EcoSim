#include "viewer/csv_result_reader.h"
#include "viewer/ogre_ecosystem_viewer.h"

#include <algorithm>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    const std::string csv_path = argc > 1 ? argv[1] : "output/simulation.csv";

    ecosim::viewer::CsvResultReader reader;
    const auto frames = reader.read(csv_path);
    if (frames.empty()) {
        std::cerr << "No frames loaded from " << csv_path << std::endl;
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
