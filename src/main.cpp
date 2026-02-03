#include "core/app.h"
#include "core/logger.h"

#include <iostream>

int main(int argc, char **argv) {
    std::string config_path = "configs/app.toml";
    if (argc > 1) {
        config_path = argv[1];
    }

    ecosim::Logger logger(std::cout);
    ecosim::Application app(logger);

    if (!app.initialize(config_path)) {
        logger.log(ecosim::LogChannel::System, "Failed to initialize application");
        return 1;
    }
    if (!app.startModules()) {
        logger.log(ecosim::LogChannel::System, "Failed to start modules");
        return 1;
    }

    app.runConsoleLoop();
    app.shutdown();
    return 0;
}
