#pragma once

#include "viewer/simulation_frame.h"

#include <memory>
#include <vector>

namespace ecosim::viewer {

class OgreEcosystemViewer {
public:
    OgreEcosystemViewer();
    ~OgreEcosystemViewer();

    bool initialize();
    void run(const std::vector<SimulationFrame> &frames);
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ecosim::viewer
