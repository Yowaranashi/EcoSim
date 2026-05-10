#include "modules/simulation_world.h"

#include "core/logger.h"
#include "models/model_dynamics_factory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ecosim {

namespace {
std::string trim(const std::string &value) {
    const char *spaces = " \t\n\r";
    auto start = value.find_first_not_of(spaces);
    if (start == std::string::npos) {
        return "";
    }
    auto end = value.find_last_not_of(spaces);
    return value.substr(start, end - start + 1);
}

bool startsWith(const std::string &value, const std::string &prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string getStringParam(const WorldCommand &command, const std::string &name, const std::string &fallback = "") {
    auto it = command.params.find(name);
    return it != command.params.end() ? it->second : fallback;
}

bool hasNumberParam(const WorldCommand &command, const std::string &name) {
    return command.numeric_params.find(name) != command.numeric_params.end() ||
           command.params.find(name) != command.params.end();
}

double getNumberParam(const WorldCommand &command, const std::string &name, double fallback = 0.0) {
    auto numeric_it = command.numeric_params.find(name);
    if (numeric_it != command.numeric_params.end()) {
        return numeric_it->second;
    }
    auto text_it = command.params.find(name);
    if (text_it != command.params.end()) {
        return std::stod(text_it->second);
    }
    return fallback;
}

std::vector<std::string> splitList(const std::string &value, char delimiter = ',') {
    std::vector<std::string> result;
    std::istringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        auto cleaned = trim(item);
        if (!cleaned.empty()) {
            result.push_back(cleaned);
        }
    }
    return result;
}

std::string joinList(const std::vector<std::string> &values) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << values[i];
    }
    return out.str();
}

std::string stripBrackets(std::string value) {
    value = trim(value);
    while (!value.empty() && (value.front() == '[' || value.front() == '{')) {
        value.erase(value.begin());
        value = trim(value);
    }
    while (!value.empty() && (value.back() == ']' || value.back() == '}')) {
        value.pop_back();
        value = trim(value);
    }
    return value;
}

std::map<std::string, double> parseNumberMap(const std::string &value) {
    std::map<std::string, double> result;
    for (const auto &entry : splitList(stripBrackets(value))) {
        const auto equals = entry.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const auto key = trim(entry.substr(0, equals));
        const auto number = trim(entry.substr(equals + 1));
        if (!key.empty() && !number.empty()) {
            result[key] = std::stod(number);
        }
    }
    return result;
}

std::vector<std::vector<double>> parseMatrix(const std::string &value) {
    std::vector<std::vector<double>> matrix;
    for (const auto &row_text : splitList(stripBrackets(value), ';')) {
        std::vector<double> row;
        for (const auto &cell : splitList(stripBrackets(row_text))) {
            row.push_back(std::stod(cell));
        }
        if (!row.empty()) {
            matrix.push_back(row);
        }
    }
    return matrix;
}

WorldCommand makeCommand(const std::string &command, const std::map<std::string, std::string> &params) {
    WorldCommand result;
    result.command = command;
    result.params = params;
    for (const auto &pair : params) {
        try {
            std::size_t parsed = 0;
            double value = std::stod(pair.second, &parsed);
            if (parsed == pair.second.size()) {
                result.numeric_params[pair.first] = value;
            }
        } catch (...) {
        }
    }
    return result;
}

IntegrationMethod parseIntegrationMethod(const std::string &integrator) {
    std::string value;
    value.reserve(integrator.size());
    for (char ch : integrator) {
        value.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return value == "rk4" ? IntegrationMethod::RK4 : IntegrationMethod::Euler;
}

std::string integrationMethodName(IntegrationMethod method) {
    return method == IntegrationMethod::RK4 ? "rk4" : "euler";
}

std::string canonicalModelId(const std::string &model_id) {
    if (model_id == "rm") {
        return "rosenzweig_macarthur";
    }
    if (model_id == "generalized_lotka_volterra") {
        return "glv";
    }
    return model_id;
}

std::size_t speciesIndex(const std::vector<SpeciesConfig> &species, const std::string &name) {
    auto it = std::find_if(species.begin(), species.end(), [&](const SpeciesConfig &entry) {
        return entry.id == name;
    });
    if (it == species.end()) {
        throw std::runtime_error("Unknown species in model configuration: " + name);
    }
    return static_cast<std::size_t>(std::distance(species.begin(), it));
}

std::size_t speciesIndexOrOrdinal(const std::vector<SpeciesConfig> &species, const std::string &name) {
    if (!name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char ch) { return std::isdigit(ch); })) {
        return static_cast<std::size_t>(std::stoul(name));
    }
    return speciesIndex(species, name);
}

void ensureSpecies(ModelConfig &config, const std::string &name) {
    if (name.empty()) {
        return;
    }
    auto it = std::find_if(config.species.begin(), config.species.end(), [&](const SpeciesConfig &species) {
        return species.id == name;
    });
    if (it == config.species.end()) {
        SpeciesConfig species;
        species.id = name;
        if (auto initial = config.initial_state.find(name); initial != config.initial_state.end()) {
            species.initial_state = initial->second;
        }
        config.species.push_back(species);
    }
}

void setSpeciesParameter(ModelConfig &config,
                         const std::string &species_name,
                         const std::string &parameter_name,
                         double value) {
    ensureSpecies(config, species_name);
    for (auto &species : config.species) {
        if (species.id == species_name) {
            species.parameters[parameter_name] = value;
            return;
        }
    }
}

void applyInitialState(ModelConfig &config, const std::string &species_name, double value) {
    config.initial_state[species_name] = value;
    ensureSpecies(config, species_name);
    for (auto &species : config.species) {
        if (species.id == species_name) {
            species.initial_state = value;
            return;
        }
    }
}

ModelConfig buildModelConfig(const WorldCommand &command, const std::string &model_id) {
    ModelConfig config = command.has_model_config ? command.model_config : ModelConfig{};
    config.model_id = canonicalModelId(model_id);

    auto species_text = getStringParam(command, "species", getStringParam(command, "species_names"));
    for (const auto &name : splitList(species_text)) {
        ensureSpecies(config, name);
    }

    if (auto it = command.params.find("initial_state"); it != command.params.end()) {
        for (const auto &entry : parseNumberMap(it->second)) {
            applyInitialState(config, entry.first, entry.second);
        }
    }
    if (auto it = command.params.find("parameters"); it != command.params.end()) {
        for (const auto &entry : parseNumberMap(it->second)) {
            config.parameters[entry.first] = entry.second;
        }
    }
    if (auto it = command.params.find("interaction_matrix"); it != command.params.end()) {
        config.interaction_matrix = parseMatrix(it->second);
    }

    for (const auto &entry : command.numeric_params) {
        if (startsWith(entry.first, "initial.")) {
            applyInitialState(config, entry.first.substr(8), entry.second);
        } else if (startsWith(entry.first, "initial_state.")) {
            applyInitialState(config, entry.first.substr(14), entry.second);
        } else if (startsWith(entry.first, "param.")) {
            config.parameters[entry.first.substr(6)] = entry.second;
        } else if (startsWith(entry.first, "species.")) {
            const auto rest = entry.first.substr(8);
            const auto dot = rest.find('.');
            if (dot != std::string::npos) {
                setSpeciesParameter(config, rest.substr(0, dot), rest.substr(dot + 1), entry.second);
            }
        }
    }

    if (!config.species.empty() && config.interaction_matrix.empty()) {
        config.interaction_matrix.assign(config.species.size(), std::vector<double>(config.species.size(), 0.0));
    }

    for (const auto &entry : command.numeric_params) {
        if (!startsWith(entry.first, "interaction.")) {
            continue;
        }
        const auto rest = entry.first.substr(12);
        const auto dot = rest.find('.');
        if (dot == std::string::npos) {
            continue;
        }
        const auto row_key = rest.substr(0, dot);
        const auto col_key = rest.substr(dot + 1);
        const auto row = speciesIndexOrOrdinal(config.species, row_key);
        const auto col = speciesIndexOrOrdinal(config.species, col_key);
        if (config.interaction_matrix.size() < config.species.size()) {
            config.interaction_matrix.resize(config.species.size());
        }
        for (auto &matrix_row : config.interaction_matrix) {
            matrix_row.resize(config.species.size(), 0.0);
        }
        if (row < config.interaction_matrix.size() && col < config.interaction_matrix[row].size()) {
            config.interaction_matrix[row][col] = entry.second;
        }
    }

    return config;
}

bool hasUsableDynamicsConfig(const WorldCommand &command, const ModelConfig &config) {
    return command.has_model_config || !config.species.empty() || !config.initial_state.empty();
}

double sumValues(const std::vector<double> &values) {
    double total = 0.0;
    for (double value : values) {
        total += value;
    }
    return total;
}

std::uint64_t fnv1a64(const std::string &text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}
} // namespace

SimulationWorld::SimulationWorld(const ModuleInstanceConfig &instance, ModuleContext &context)
    : type_id_(instance.type_id), instance_id_(instance.instance_id), context_(context) {}

void SimulationWorld::onInit() {
    read_model_ = ReadModel{};
    dt_ = context_.config().dt;
    read_model_.dt = dt_;
    seed_ = 0;
    stop_at_tick_ = -1;
    scenario_id_.clear();
    model_id_.clear();
    integrator_.clear();
    flags_.clear();
    params_.clear();
    species_order_.clear();
    dynamics_.reset();
    syncLegacyDerivedState();
}

void SimulationWorld::enqueueCommand(const std::string &command, const std::map<std::string, std::string> &params) {
    enqueueCommand(makeCommand(command, params));
}

void SimulationWorld::enqueueCommand(const WorldCommand &command) {
    pending_commands_.push_back(command);
}

void SimulationWorld::onPreTick() {
    for (const auto &entry : pending_commands_) {
        applyCommand(entry);
    }
    pending_commands_.clear();
    updateReadModel();
}

void SimulationWorld::onTick() {
    read_model_.tick += 1;
    read_model_.time = read_model_.tick * dt_;

    if (dynamics_) {
        dynamics_->advanceStep(dt_, integration_method_);
        updateReadModel();
        emitTickEvent();
        return;
    }

    for (const auto &species : species_order_) {
        auto &count = read_model_.population_by_species[species];
        count += 1;
    }
    syncLegacyDerivedState();
    emitTickEvent();
}

void SimulationWorld::applyCommand(const WorldCommand &command) {
    if (command.command == "world.reset") {
        read_model_ = ReadModel{};
        seed_ = static_cast<int>(getNumberParam(command, "seed", 0.0));
        read_model_.seed = seed_;
        dt_ = context_.config().dt;
        read_model_.dt = dt_;
        stop_at_tick_ = -1;
        scenario_id_.clear();
        model_id_.clear();
        integrator_.clear();
        flags_.clear();
        params_.clear();
        species_order_.clear();
        dynamics_.reset();
        context_.logger().log(LogChannel::System, "World reset with seed " + std::to_string(seed_));
    } else if (command.command == "world.configure" || command.command == "configure_model") {
        auto scenario_id = getStringParam(command, "scenario_id");
        if (!scenario_id.empty()) {
            scenario_id_ = scenario_id;
        }

        auto model_id = getStringParam(command, "model_id", getStringParam(command, "model"));
        if (!model_id.empty()) {
            model_id_ = model_id;
        }

        auto integrator = getStringParam(command, "integrator");
        if (!integrator.empty()) {
            integration_method_ = parseIntegrationMethod(integrator);
            integrator_ = integrationMethodName(integration_method_);
        } else if (integrator_.empty()) {
            integrator_ = integrationMethodName(integration_method_);
        }

        if (hasNumberParam(command, "dt")) {
            dt_ = getNumberParam(command, "dt", dt_);
        }

        dynamics_.reset();
        if (!model_id_.empty() && isSupportedModelId(model_id_)) {
            auto config = buildModelConfig(command, model_id_);
            if (hasUsableDynamicsConfig(command, config)) {
                try {
                    dynamics_ = createModelDynamics(model_id_);
                    dynamics_->configure(config);
                    species_order_ = dynamics_->getSpeciesNames();
                    flags_.push_back("math_model_connected");
                } catch (const std::exception &ex) {
                    dynamics_.reset();
                    flags_.push_back("model_config_error");
                    context_.logger().log(LogChannel::System,
                                          std::string("Failed to configure model dynamics: ") + ex.what());
                }
            }
        }
    } else if (command.command == "stop.at_tick" || command.command == "stop_at_tick") {
        if (hasNumberParam(command, "value")) {
            stop_at_tick_ = static_cast<int>(getNumberParam(command, "value"));
        } else if (hasNumberParam(command, "tick")) {
            stop_at_tick_ = static_cast<int>(getNumberParam(command, "tick"));
        }
    } else if (command.command == "set_param") {
        auto name = getStringParam(command, "name");
        if (!name.empty() && hasNumberParam(command, "value")) {
            const double value = getNumberParam(command, "value");
            params_[name] = value;
            if (dynamics_) {
                dynamics_->setParameter(name, value);
            }
            flags_.push_back("param_changed." + name);
        }
    } else if (command.command == "apply_shock") {
        auto target = getStringParam(command, "target", getStringParam(command, "species"));
        double strength =
            getNumberParam(command, "strength", getNumberParam(command, "amount", getNumberParam(command, "value", 0.0)));
        if (dynamics_) {
            dynamics_->applyShock(target, strength);
        } else if (!target.empty()) {
            auto &count = read_model_.population_by_species[target];
            if (std::find(species_order_.begin(), species_order_.end(), target) == species_order_.end()) {
                species_order_.push_back(target);
            }
            count = std::max(0, count + static_cast<int>(std::lround(strength)));
        } else {
            for (const auto &species : species_order_) {
                auto &count = read_model_.population_by_species[species];
                count = std::max(0, static_cast<int>(count * (1.0 - strength)));
            }
        }
        flags_.push_back("shock." + target);
    } else if (command.command == "spawn") {
        auto species = getStringParam(command, "species", getStringParam(command, "target"));
        double amount = getNumberParam(command, "count", getNumberParam(command, "amount", 0.0));
        if (!species.empty()) {
            if (dynamics_) {
                dynamics_->applyShock(species, amount);
            } else {
                auto &count = read_model_.population_by_species[species];
                if (std::find(species_order_.begin(), species_order_.end(), species) == species_order_.end()) {
                    species_order_.push_back(species);
                }
                count += static_cast<int>(std::lround(amount));
            }
            flags_.push_back("spawn." + species);
        }
    }
}

void SimulationWorld::updateReadModel() {
    if (!dynamics_) {
        syncLegacyDerivedState();
        return;
    }

    read_model_.dt = dt_;
    read_model_.seed = seed_;
    read_model_.scenario_id = scenario_id_;
    read_model_.model_id = model_id_;
    read_model_.integrator = integrator_;
    read_model_.species_names = dynamics_->getSpeciesNames();
    read_model_.state_vector = dynamics_->getStateVector();
    read_model_.metrics = dynamics_->getMetrics();
    read_model_.flags = flags_;

    read_model_.population_by_species.clear();
    for (std::size_t i = 0; i < read_model_.species_names.size() && i < read_model_.state_vector.size(); ++i) {
        read_model_.population_by_species[read_model_.species_names[i]] =
            static_cast<int>(std::lround(read_model_.state_vector[i]));
    }

    double energy = sumValues(read_model_.state_vector);
    if (auto biomass = read_model_.metrics.find("biomass_total"); biomass != read_model_.metrics.end()) {
        energy = biomass->second;
    }
    read_model_.energy_total = static_cast<int>(std::lround(energy));
    read_model_.checksum = checksum();
}

void SimulationWorld::syncLegacyDerivedState() {
    read_model_.dt = dt_;
    read_model_.seed = seed_;
    read_model_.scenario_id = scenario_id_;
    read_model_.model_id = model_id_;
    read_model_.integrator = integrator_;
    read_model_.species_names = species_order_;
    read_model_.state_vector.clear();
    read_model_.energy_total = 0;

    int total_population = 0;
    for (const auto &species : species_order_) {
        int count = read_model_.population_by_species[species];
        read_model_.state_vector.push_back(static_cast<double>(count));
        total_population += count;
        read_model_.energy_total += count * 2;
    }

    read_model_.metrics.clear();
    read_model_.metrics["energy_total"] = static_cast<double>(read_model_.energy_total);
    read_model_.metrics["species_count"] = static_cast<double>(read_model_.species_names.size());
    read_model_.metrics["total_population"] = static_cast<double>(total_population);
    read_model_.metrics["param_count"] = static_cast<double>(params_.size());

    read_model_.flags = flags_;
    read_model_.flags.push_back("math_model_not_connected");
    read_model_.flags.push_back("placeholder_population_dynamics");
    if (read_model_.model_id.empty()) {
        read_model_.flags.push_back("model_id_empty");
    }

    read_model_.checksum = legacyChecksum();
}

void SimulationWorld::emitTickEvent() {
    SimulationEvent event;
    event.type = "world.tick";
    event.tick = read_model_.tick;
    event.payload["seed"] = std::to_string(read_model_.seed);
    event.payload["tick"] = std::to_string(read_model_.tick);
    event.payload["time"] = std::to_string(read_model_.time);
    event.payload["dt"] = std::to_string(read_model_.dt);
    event.payload["scenario_id"] = read_model_.scenario_id;
    event.payload["model_id"] = read_model_.model_id;
    event.payload["integrator"] = read_model_.integrator;
    event.payload["energy_total"] = std::to_string(read_model_.energy_total);
    event.payload["checksum"] = read_model_.checksum;
    event.payload["species"] = joinList(read_model_.species_names);
    event.payload["species_names"] = joinList(read_model_.species_names);
    event.payload["flags"] = joinList(read_model_.flags);

    event.numeric_payload["tick"] = static_cast<double>(read_model_.tick);
    event.numeric_payload["time"] = read_model_.time;
    event.numeric_payload["dt"] = read_model_.dt;
    event.numeric_payload["seed"] = static_cast<double>(read_model_.seed);
    event.numeric_payload["energy_total"] = static_cast<double>(read_model_.energy_total);
    event.string_list_payload["species"] = read_model_.species_names;
    event.string_list_payload["species_names"] = read_model_.species_names;
    event.numeric_vector_payload["state"] = read_model_.state_vector;
    event.numeric_vector_payload["state_vector"] = read_model_.state_vector;
    event.metrics = read_model_.metrics;
    event.flags = read_model_.flags;

    for (std::size_t i = 0; i < read_model_.species_names.size(); ++i) {
        event.payload["species." + std::to_string(i)] = read_model_.species_names[i];
        event.payload["state." + std::to_string(i)] = std::to_string(read_model_.state_vector[i]);
    }
    for (const auto &pair : read_model_.population_by_species) {
        event.payload["population." + pair.first] = std::to_string(pair.second);
        event.numeric_payload["population." + pair.first] = static_cast<double>(pair.second);
    }
    for (const auto &pair : read_model_.metrics) {
        event.payload["metrics." + pair.first] = std::to_string(pair.second);
        event.numeric_payload["metrics." + pair.first] = pair.second;
    }

    context_.eventBus().emit(event);
    context_.logger().log(LogChannel::Simulation,
                          "Tick " + std::to_string(read_model_.tick) + " population=" +
                              std::to_string(read_model_.population_by_species.size()));
}

bool SimulationWorld::shouldStop() const {
    return stop_at_tick_ >= 0 && read_model_.tick >= stop_at_tick_;
}

std::string SimulationWorld::checksum() const {
    if (!dynamics_) {
        return legacyChecksum();
    }

    std::ostringstream canonical;
    canonical << std::setprecision(17);
    canonical << "tick=" << read_model_.tick << ';';
    canonical << "time=" << read_model_.time << ';';
    canonical << "seed=" << seed_ << ';';
    canonical << "scenario=" << scenario_id_ << ';';
    canonical << "model=" << model_id_ << ';';
    canonical << "integrator=" << integrator_ << ';';
    canonical << "dt=" << dt_ << ';';
    canonical << "dynamics=" << dynamics_->checksum();

    std::ostringstream output;
    output << std::hex << fnv1a64(canonical.str());
    return output.str();
}

std::string SimulationWorld::legacyChecksum() const {
    long long total = 0;
    total = total * 31 + read_model_.tick;
    total = total * 31 + seed_;
    for (double value : read_model_.state_vector) {
        total = total * 31 + static_cast<long long>(value * 1000000.0);
    }
    total = total * 31 + read_model_.energy_total;
    total = total * 31 + static_cast<long long>(read_model_.time * 1000000.0);
    return std::to_string(total);
}

} // namespace ecosim
