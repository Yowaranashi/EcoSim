#pragma once

#include <map>
#include <string>

namespace ecosim {

struct ReadModel {
    int tick = 0;
    int seed = 0;
    std::map<std::string, int> population_by_species;
    int energy_total = 0;
};

class IWorldPort {
public:
    virtual ~IWorldPort() = default;

    virtual void enqueueCommand(const std::string &command,
                                const std::map<std::string, std::string> &params) = 0;
    virtual const ReadModel &readModel() const = 0;
    virtual bool shouldStop() const = 0;
};

} // namespace ecosim
