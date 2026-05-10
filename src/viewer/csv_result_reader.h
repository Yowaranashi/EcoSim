#pragma once

#include "viewer/simulation_frame.h"

#include <string>
#include <vector>

namespace ecosim::viewer {

class CsvResultReader {
public:
    std::vector<SimulationFrame> read(const std::string &path) const;
};

} // namespace ecosim::viewer
