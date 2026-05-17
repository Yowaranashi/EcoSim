#include "core/app.h"

#include "modules/agent_behavoir.h"
#include "modules/scenario_runner.h"
#include "modules/simulation_world.h"
#include "modules/world_port.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace ecosim {

namespace {

bool hasParentTraversal(const std::filesystem::path &path) {
    for (const auto &part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size == buffer.size()) {
        return {};
    }
    buffer.resize(size);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__linux__)
    std::vector<char> buffer(4096, '\0');
    auto size = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (size <= 0) {
        return {};
    }
    buffer[static_cast<std::size_t>(size)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
#else
    return {};
#endif
}

bool hasRuntimeLayout(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::exists(path / "modules" / "simulation_world" / "manifest.toml", ec) && !ec;
}

std::filesystem::path findRuntimeRootFrom(std::filesystem::path current) {
    if (current.empty()) {
        return {};
    }
    current = std::filesystem::absolute(current).lexically_normal();
    for (int i = 0; i < 16; ++i) {
        if (hasRuntimeLayout(current)) {
            return current;
        }
        if (!current.has_parent_path()) {
            break;
        }
        auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

std::filesystem::path findRuntimeRoot() {
    const auto exe_dir = executableDirectory();
    if (auto from_exe = findRuntimeRootFrom(exe_dir); !from_exe.empty()) {
        return from_exe;
    }
    if (auto from_cwd = findRuntimeRootFrom(std::filesystem::current_path()); !from_cwd.empty()) {
        return from_cwd;
    }
    if (!exe_dir.empty()) {
        return std::filesystem::absolute(exe_dir).lexically_normal();
    }
    return std::filesystem::current_path();
}

std::filesystem::path clampRelativeInsideRuntime(const std::filesystem::path &path) {
    std::filesystem::path clamped;
    for (const auto &part : path.lexically_normal()) {
        if (part.empty() || part == ".") {
            continue;
        }
        if (part == "..") {
            if (!clamped.empty()) {
                clamped = clamped.parent_path();
            }
            continue;
        }
        clamped /= part;
    }
    return clamped;
}

bool pathExists(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

std::filesystem::path resolveConfigPath(const std::filesystem::path &requested,
                                        const std::filesystem::path &runtime_root) {
    if (requested.is_absolute()) {
        return requested.lexically_normal();
    }

    const auto runtime_candidate = (runtime_root / requested).lexically_normal();
    if (pathExists(runtime_candidate)) {
        return runtime_candidate;
    }

    const auto cwd_candidate = std::filesystem::absolute(requested).lexically_normal();
    if (pathExists(cwd_candidate)) {
        return cwd_candidate;
    }

    return runtime_candidate;
}

std::filesystem::path resolveRuntimeObjectPath(const std::string &configured,
                                               const std::filesystem::path &runtime_root,
                                               const std::filesystem::path &config_dir,
                                               const std::filesystem::path &default_relative,
                                               bool allow_existing_legacy_fallback) {
    if (configured.empty()) {
        return (runtime_root / default_relative).lexically_normal();
    }

    std::filesystem::path requested(configured);
    if (requested.is_absolute()) {
        return requested.lexically_normal();
    }

    auto clamped = clampRelativeInsideRuntime(requested);
    if (clamped.empty()) {
        clamped = default_relative;
    }
    const auto runtime_candidate = (runtime_root / clamped).lexically_normal();
    if (pathExists(runtime_candidate)) {
        return runtime_candidate;
    }

    if (allow_existing_legacy_fallback) {
        for (const auto &base : {config_dir, std::filesystem::current_path()}) {
            if (base.empty()) {
                continue;
            }
            auto legacy_candidate = (base / requested).lexically_normal();
            if (pathExists(legacy_candidate)) {
                return legacy_candidate;
            }
        }
    }

    return runtime_candidate;
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool hasTomlExtension(const std::filesystem::path &path) {
    return lowerCopy(path.extension().string()) == ".toml";
}

bool isWithinDirectory(const std::filesystem::path &path, const std::filesystem::path &directory) {
    auto normalized_path = std::filesystem::absolute(path).lexically_normal();
    auto normalized_dir = std::filesystem::absolute(directory).lexically_normal();
    auto path_it = normalized_path.begin();
    for (auto dir_it = normalized_dir.begin(); dir_it != normalized_dir.end(); ++dir_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *dir_it) {
            return false;
        }
    }
    return true;
}

std::string formatDouble(double value) {
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

std::string csvEscape(const std::string &value) {
    const bool needs_quotes = value.find_first_of(",\"\r\n") != std::string::npos;
    if (!needs_quotes) {
        return value;
    }

    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += '"';
    return escaped;
}

void writeCsvRow(std::ofstream &file, const std::vector<std::string> &values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            file << ',';
        }
        file << csvEscape(values[i]);
    }
    file << '\n';
}

std::optional<std::string> valueAfterOption(const std::vector<std::string> &args,
                                            const std::string &short_name,
                                            const std::string &long_name) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == short_name || args[i] == long_name) && i + 1 < args.size()) {
            return args[i + 1];
        }
    }
    return std::nullopt;
}

bool hasDanglingOption(const std::vector<std::string> &args,
                       const std::string &short_name,
                       const std::string &long_name) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == short_name || args[i] == long_name) && i + 1 >= args.size()) {
            return true;
        }
    }
    return false;
}

std::string quoteCommandArg(const std::filesystem::path &path) {
    std::string value = path.string();
    std::string quoted = "\"";
    for (char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += '"';
    return quoted;
}

#if defined(_WIN32)
std::wstring mutableCommandLine(const std::filesystem::path &exe_path,
                                const std::filesystem::path &csv_path) {
    return L"\"" + exe_path.wstring() + L"\" \"" + csv_path.wstring() + L"\"";
}
#endif

std::vector<std::string> splitDots(const std::string &value) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        auto end = value.find('.', start);
        if (end == std::string::npos) {
            parts.push_back(value.substr(start));
            break;
        }
        parts.push_back(value.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

std::set<std::string> scenarioSpeciesSet(const ScenarioConfig &scenario) {
    std::set<std::string> species;
    for (const auto &entry : scenario.model.species) {
        species.insert(entry.id);
    }
    for (const auto &entry : scenario.model.initial_state) {
        species.insert(entry.first);
    }
    return species;
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

bool applySensitivityParameter(ScenarioConfig &scenario,
                               const std::string &parameter,
                               double value,
                               std::string &error) {
    const auto model_id = canonicalModelId(scenario.model.model_id);
    if (model_id == "rosenzweig_macarthur") {
        static const std::set<std::string> allowed = {"r", "K", "a", "h", "e", "m"};
        if (allowed.count(parameter) == 0) {
            error = "Unsupported Rosenzweig-MacArthur sensitivity parameter: " + parameter;
            return false;
        }
        scenario.model.parameters[parameter] = value;
        return true;
    }

    if (model_id == "glv") {
        const auto species = scenarioSpeciesSet(scenario);
        const auto parts = splitDots(parameter);
        if (parts.size() == 2 &&
            (parts[0] == "growth" || parts[0] == "sensitivity" || parts[0] == "external_input")) {
            if (species.count(parts[1]) == 0) {
                error = "Unknown gLV species in sensitivity parameter: " + parts[1];
                return false;
            }
            scenario.model.parameters[parameter] = value;
            return true;
        }
        if (parts.size() == 3 && parts[0] == "interaction") {
            if (species.count(parts[1]) == 0 || species.count(parts[2]) == 0) {
                error = "Unknown gLV species in sensitivity parameter: " + parameter;
                return false;
            }
            scenario.model.parameters[parameter] = value;
            return true;
        }
        error = "Unsupported gLV sensitivity parameter: " + parameter;
        return false;
    }

    error = "Sensitivity analysis requires a supported model_id";
    return false;
}

void enqueueScenarioConfiguration(SimulationWorld &world, ScenarioConfig scenario) {
    world.enqueueCommand("world.reset", {{"seed", std::to_string(scenario.seed)}});

    WorldCommand configure;
    configure.command = "world.configure";
    configure.params["scenario_id"] = scenario.scenario_id;
    configure.params["model_id"] = scenario.model.model_id;
    configure.params["integrator"] = scenario.integrator.type;
    configure.numeric_params["dt"] = scenario.integrator.dt;
    if (scenario.log_tick_interval) {
        configure.numeric_params["log_tick_interval"] = static_cast<double>(*scenario.log_tick_interval);
    }
    if (scenario.log_tick_details) {
        configure.params["log_tick_details"] = *scenario.log_tick_details ? "true" : "false";
    }
    scenario.model.seed = scenario.seed;
    configure.model_config = scenario.model;
    configure.has_model_config = !scenario.model.model_id.empty() || !scenario.model.species.empty() ||
                                 !scenario.model.initial_state.empty() || !scenario.model.parameters.empty() ||
                                 !scenario.model.interaction_matrix.empty();
    world.enqueueCommand(configure);
    world.enqueueCommand("stop.at_tick", {{"value", std::to_string(scenario.stop_at_tick)}});
}

void dispatchScenarioAction(SimulationWorld &world, const ScenarioConfig::ScheduledAction &action) {
    if (action.command == "spawn" || action.command == "set_param" || action.command == "apply_shock" ||
        action.command == "stop.at_tick" || action.command == "stop_at_tick") {
        world.enqueueCommand(action.command, action.params);
    }
}

struct IsolatedRunResult {
    bool ok = false;
    ReadModel state;
    std::string error;
};

IsolatedRunResult runScenarioIsolated(const ScenarioConfig &scenario,
                                      const AppConfig &app_config,
                                      Logger &logger,
                                      int max_ticks) {
    EventBus event_bus;
    AppConfig local_config = app_config;
    local_config.dt = scenario.integrator.dt;
    ModuleContext context(logger, event_bus, local_config);
    ModuleInstanceConfig instance;
    instance.type_id = "simulation_world";
    SimulationWorld world(instance, context);
    ScenarioTimeline timeline(scenario);

    try {
        world.onInit();
        enqueueScenarioConfiguration(world, scenario);
        for (int loop_tick = 0; loop_tick < max_ticks; ++loop_tick) {
            world.onPreTick();
            const int next_tick = world.readModel().tick + 1;
            for (const auto &action : timeline.actionsForTick(next_tick)) {
                dispatchScenarioAction(world, action);
            }
            world.onTick();
            world.onPostTick();
            event_bus.deliverBuffered();
            world.onDeliverBufferedEvents();

            if (world.shouldStop()) {
                return {true, world.readModel(), ""};
            }
        }
        return {true, world.readModel(), ""};
    } catch (const std::exception &ex) {
        return {false, world.readModel(), ex.what()};
    }
}

} // namespace

Application::Application(Logger &logger)
    : logger_(logger), context_(logger_, event_bus_, app_config_), module_manager_(registry_, context_) {}

Application::~Application() {
    event_bus_.clear();
}

bool Application::initialize(const std::string &config_path) {
    runtime_root_ = findRuntimeRoot();
    config_path_ = resolveConfigPath(std::filesystem::path(config_path), runtime_root_);
    logger_.log(LogChannel::System, "Runtime root: " + runtime_root_.string());
    logger_.log(LogChannel::System, "Loading app config: " + config_path_.string());
    try {
        app_config_ = ConfigLoader::loadAppConfig(config_path_.string());
    } catch (const std::exception &ex) {
        logger_.log(LogChannel::System, std::string("Failed to load app config: ") + ex.what());
        return false;
    }

    config_dir_ = config_path_.parent_path();
    app_config_.runtime_root = runtime_root_.string();
    app_config_.modules_dir =
        resolveRuntimeObjectPath(app_config_.modules_dir, runtime_root_, config_dir_, "modules", true).string();
    app_config_.output_dir =
        resolveRuntimeObjectPath(app_config_.output_dir, runtime_root_, config_dir_, "output", false).string();
    if (!app_config_.recorder_output_path.empty()) {
        app_config_.recorder_output_path = resolveRunOutputPath(app_config_.recorder_output_path).string();
    }
    default_recorder_output_path_ = app_config_.recorder_output_path.empty()
                                        ? (std::filesystem::path(app_config_.output_dir) / "simulation.csv").lexically_normal()
                                        : std::filesystem::path(app_config_.recorder_output_path).lexically_normal();
#if defined(ECOSIM_OGRE_VIEWER_AVAILABLE) && ECOSIM_OGRE_VIEWER_AVAILABLE
    ogre_runtime_enabled_ = app_config_.ogre_visualization;
#else
    ogre_runtime_enabled_ = false;
    if (app_config_.ogre_visualization) {
        logger_.log(LogChannel::System, "OGRE visualization requested but viewer support is not available in this build");
    }
#endif
    scenarios_dir_ = (runtime_root_ / "scenarios").lexically_normal();

    logger_.log(LogChannel::System, "Loading manifests from: " + app_config_.modules_dir);
    try {
        registry_.loadManifests(app_config_.modules_dir);
    } catch (const std::exception &ex) {
        logger_.log(LogChannel::System, std::string("Failed to load module manifests: ") + ex.what());
        return false;
    }

    registry_.registerFactory("simulation_world", [](const ModuleInstanceConfig &instance, ModuleContext &context) {
        return std::make_unique<SimulationWorld>(instance, context);
    });
    registry_.registerFactory("scenario", [](const ModuleInstanceConfig &instance, ModuleContext &context) {
        return std::make_unique<ScenarioRunner>(instance, context);
    });
    registry_.registerFactory("agent_behavoir", [](const ModuleInstanceConfig &instance, ModuleContext &context) {
        return std::make_unique<AgentBehavoir>(instance, context);
    });
    if (!module_manager_.buildModules(app_config_.instances, app_config_.error_policy, logger_)) {
        return false;
    }

    auto world = dynamic_cast<IWorldPort *>(module_manager_.findModule("simulation_world"));
    auto scenario = dynamic_cast<ScenarioRunner *>(module_manager_.findModule("scenario"));
    if (scenario) {
        std::vector<std::string> types;
        for (auto module : module_manager_.modules()) {
            types.push_back(module->typeId());
        }
        scenario->setAvailableModules(types);
        scenario->setWorld(world);
    }

    registerCoreCommands();
    return true;
}

bool Application::startModules() {
    if (!module_manager_.startModules(app_config_.error_policy, logger_)) {
        return false;
    }

    return true;
}

bool Application::runHeadless() {
    if (!prepareScenario(app_config_.scenario_path, true)) {
        return false;
    }
    return runSimulationLoop();
}

bool Application::runSimulationLoop() {
    running_ = true;

    auto world = dynamic_cast<IWorldPort *>(module_manager_.findModule("simulation_world"));
    if (!world) {
        logger_.log(LogChannel::System, "simulation_world module is required for headless run");
        return false;
    }

    int max_ticks = app_config_.max_ticks.value_or(1000);
    try {
        for (int tick = 0; running_; ++tick) {
            for (auto module : module_manager_.modules()) {
                module->onPreTick();
            }
            for (auto module : module_manager_.modules()) {
                module->onTick();
            }
            for (auto module : module_manager_.modules()) {
                module->onPostTick();
            }
            event_bus_.deliverBuffered();
            for (auto module : module_manager_.modules()) {
                module->onDeliverBufferedEvents();
            }

            if (world->shouldStop()) {
                const auto &state = world->readModel();
                logger_.log(LogChannel::System,
                            "Simulation finished: tick=" + std::to_string(state.tick) +
                                " checksum=" + state.checksum +
                                " output=" + expectedRecorderCsvPath().string());
                running_ = false;
            }
            if (tick + 1 >= max_ticks) {
                logger_.log(LogChannel::System, "Reached max ticks");
                running_ = false;
            }
        }
    } catch (const std::exception &ex) {
        running_ = false;
        logger_.log(LogChannel::System, std::string("Simulation failed: ") + ex.what());
        return false;
    }
    if (ogre_runtime_enabled_) {
        launchOgreViewer(expectedRecorderCsvPath());
    }
    return true;
}

void Application::runConsoleLoop() {
    console_running_ = true;
    logger_.log(LogChannel::System, "Console ready. Type a command or sys.quit to exit.");
    std::string line;
    while (console_running_ && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        if (!console_.execute(line)) {
            logger_.log(LogChannel::System, "Unknown command: " + line);
        }
    }
}

void Application::shutdown() {
    module_manager_.stopModules();
}

void Application::registerCoreCommands() {
    console_.registerCommand("module.list", [this](const std::vector<std::string> &) {
        for (auto module : module_manager_.modules()) {
            logger_.log(LogChannel::System, "module " + module->typeId() + ":" + module->instanceId());
        }
    });
    console_.registerCommand("module.start", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "module.start is not supported in MVP (static modules)");
    });
    console_.registerCommand("module.stop", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "module.stop is not supported in MVP (static modules)");
    });
    console_.registerCommand("help", [this](const std::vector<std::string> &) {
        auto names = console_.commandNames();
        logger_.log(LogChannel::System, "Available commands:");
        for (const auto &name : names) {
            logger_.log(LogChannel::System, " - " + name);
        }
    });
    console_.registerCommand("scenario.list", [this](const std::vector<std::string> &) {
        listScenarios();
    });
    console_.registerCommand("sim.run", [this](const std::vector<std::string> &args) {
        executeSimRun(args);
    });
    console_.registerCommand("sim.sensitivity", [this](const std::vector<std::string> &args) {
        executeSensitivity(args);
    });
    console_.registerCommand("sim.pause", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "sim.pause is a no-op in headless MVP");
    });
    console_.registerCommand("sim.resume", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "sim.resume is a no-op in headless MVP");
    });
    console_.registerCommand("sys.quit", [this](const std::vector<std::string> &) {
        running_ = false;
        console_running_ = false;
        logger_.log(LogChannel::System, "sys.quit received");
    });
    console_.registerCommand("ogre.status", [this](const std::vector<std::string> &) {
        logOgreStatus();
    });
    console_.registerCommand("ogre.enable", [this](const std::vector<std::string> &) {
        setOgreRuntimeEnabled(true);
    });
    console_.registerCommand("ogre.disable", [this](const std::vector<std::string> &) {
        setOgreRuntimeEnabled(false);
    });
}

Application::ScenarioPathResult Application::resolveScenarioPath(
    const std::string &scenario_path,
    bool allow_existing_relative_fallback) const {
    if (scenario_path.empty()) {
        return {false, {}, "scenario_path is empty"};
    }

    auto validate = [](const std::filesystem::path &candidate) -> ScenarioPathResult {
        std::error_code ec;
        if (!hasTomlExtension(candidate)) {
            return {false, candidate, "Scenario file must have .toml extension: " + candidate.string()};
        }
        if (!std::filesystem::exists(candidate, ec)) {
            return {false, candidate, "Scenario file does not exist: " + candidate.string()};
        }
        if (ec) {
            return {false, candidate, "Cannot access scenario file: " + candidate.string() + " (" + ec.message() + ")"};
        }
        if (!std::filesystem::is_regular_file(candidate, ec)) {
            return {false, candidate, "Scenario path is not a regular file: " + candidate.string()};
        }
        if (ec) {
            return {false, candidate, "Cannot inspect scenario file: " + candidate.string() + " (" + ec.message() + ")"};
        }
        return {true, candidate, ""};
    };

    std::filesystem::path requested(scenario_path);
    if (requested.is_absolute()) {
        return validate(requested.lexically_normal());
    }

    const bool contains_parent_traversal = hasParentTraversal(requested);
    if (contains_parent_traversal && !allow_existing_relative_fallback) {
        return {false, {}, "Relative scenario paths cannot leave scenarios/: " + scenario_path};
    }

    auto scenarios_candidate = (scenarios_dir_ / requested).lexically_normal();
    if (!isWithinDirectory(scenarios_candidate, scenarios_dir_)) {
        if (!allow_existing_relative_fallback) {
            return {false, scenarios_candidate,
                    "Relative scenario paths cannot leave scenarios/: " + scenario_path};
        }
    } else {
        std::error_code ec;
        if (std::filesystem::exists(scenarios_candidate, ec)) {
            return validate(scenarios_candidate);
        }
    }

    if (allow_existing_relative_fallback) {
        std::vector<std::filesystem::path> fallbacks;
        auto clamped = clampRelativeInsideRuntime(requested);
        if (!clamped.empty()) {
            fallbacks.push_back((runtime_root_ / clamped).lexically_normal());
        }
        fallbacks.push_back((config_dir_ / requested).lexically_normal());
        fallbacks.push_back(std::filesystem::absolute(requested).lexically_normal());

        for (const auto &fallback : fallbacks) {
            if (fallback.empty()) {
                continue;
            }
            std::error_code ec;
            if (std::filesystem::exists(fallback, ec)) {
                logger_.log(LogChannel::System,
                            "Warning: using legacy scenario path outside scenarios/: " + fallback.string());
                return validate(fallback);
            }
        }
    }

    return {false, scenarios_candidate,
            "Scenario file does not exist: " + scenarios_candidate.string()};
}

bool Application::prepareScenario(const std::string &scenario_path,
                                  bool allow_existing_relative_fallback) {
    auto resolved = resolveScenarioPath(scenario_path, allow_existing_relative_fallback);
    if (!resolved.ok) {
        logger_.log(LogChannel::System, resolved.error);
        return false;
    }

    auto *scenario = dynamic_cast<ScenarioRunner *>(module_manager_.findModule("scenario"));
    if (!scenario) {
        logger_.log(LogChannel::System, "scenario module is required to load scenario files");
        return false;
    }
    return scenario->loadScenario(resolved.path);
}

bool Application::executeSimRun(const std::vector<std::string> &args) {
    if (hasDanglingOption(args, "-s", "--scenario") || hasDanglingOption(args, "-o", "--output")) {
        logger_.log(LogChannel::System, "sim.run has a missing option value");
        return false;
    }
    auto scenario = valueAfterOption(args, "-s", "--scenario");
    auto output = valueAfterOption(args, "-o", "--output");
    if (output) {
        auto output_path = resolveRunOutputPath(*output);
        if (output_path.empty()) {
            logger_.log(LogChannel::System, "sim.run --output requires a file name");
            return false;
        }
        configureRecorderOutput(output_path);
        logger_.log(LogChannel::System, "Recorder output selected: " + app_config_.recorder_output_path);
    } else {
        configureRecorderOutput(default_recorder_output_path_);
    }
    const auto scenario_name = scenario.value_or(app_config_.scenario_path);
    const bool allow_legacy_fallback = !scenario.has_value();
    if (!prepareScenario(scenario_name, allow_legacy_fallback)) {
        return false;
    }
    return runSimulationLoop();
}

std::filesystem::path Application::resolveRunOutputPath(const std::string &output_arg) const {
    if (output_arg.empty()) {
        return {};
    }

    std::filesystem::path requested(output_arg);
    if (requested.filename().empty()) {
        return {};
    }
    if (requested.extension().empty()) {
        requested.replace_extension(".csv");
    }

    if (requested.is_absolute()) {
        return requested.lexically_normal();
    }
    if (requested.has_parent_path()) {
        return (runtime_root_ / requested).lexically_normal();
    }
    return (std::filesystem::path(app_config_.output_dir) / requested).lexically_normal();
}

void Application::configureRecorderOutput(const std::filesystem::path &path) {
    app_config_.recorder_output_path = path.lexically_normal().string();

    SimulationEvent event;
    event.type = "recorder.configure";
    event.payload["path"] = app_config_.recorder_output_path;
    event_bus_.emit(event);
    event_bus_.deliverBuffered();
}

void Application::launchOgreViewer(const std::filesystem::path &csv_path) const {
#if defined(ECOSIM_OGRE_VIEWER_AVAILABLE) && ECOSIM_OGRE_VIEWER_AVAILABLE
#if defined(_WIN32)
    const auto viewer_path = (runtime_root_ / "ecosim_ogre_viewer.exe").lexically_normal();
#else
    const auto viewer_path = (runtime_root_ / "ecosim_ogre_viewer").lexically_normal();
#endif
    std::error_code ec;
    if (!std::filesystem::exists(viewer_path, ec)) {
        logger_.log(LogChannel::System, "OGRE viewer executable not found: " + viewer_path.string());
        return;
    }
    if (!std::filesystem::exists(csv_path, ec)) {
        logger_.log(LogChannel::System, "OGRE viewer CSV not found: " + csv_path.string());
        return;
    }

    logger_.log(LogChannel::System, "Launching OGRE viewer: " + viewer_path.string() + " " + csv_path.string());
#if defined(_WIN32)
    auto command_line = mutableCommandLine(viewer_path, csv_path);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    auto working_dir = viewer_path.parent_path().wstring();
    if (!CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        working_dir.c_str(), &startup, &process)) {
        logger_.log(LogChannel::System, "Failed to launch OGRE viewer");
        return;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exit_code != 0) {
        logger_.log(LogChannel::System, "OGRE viewer exited with code " + std::to_string(exit_code));
    }
#else
    const auto command = "cd " + quoteCommandArg(viewer_path.parent_path()) + " && " +
                         quoteCommandArg(viewer_path) + " " + quoteCommandArg(csv_path);
    const int exit_code = std::system(command.c_str());
    if (exit_code != 0) {
        logger_.log(LogChannel::System, "OGRE viewer exited with code " + std::to_string(exit_code));
    }
#endif
#else
    (void)csv_path;
    logger_.log(LogChannel::System, "OGRE viewer support is not available in this build");
#endif
}

void Application::listScenarios() const {
    std::error_code ec;
    if (!std::filesystem::exists(scenarios_dir_, ec)) {
        logger_.log(LogChannel::System, "No scenarios directory found: " + scenarios_dir_.string());
        return;
    }
    if (!std::filesystem::is_directory(scenarios_dir_, ec)) {
        logger_.log(LogChannel::System, "Scenarios path is not a directory: " + scenarios_dir_.string());
        return;
    }

    std::vector<std::string> names;
    std::filesystem::directory_iterator it(scenarios_dir_, ec);
    if (ec) {
        logger_.log(LogChannel::System, "Cannot list scenarios: " + ec.message());
        return;
    }
    for (const auto &entry : it) {
        std::error_code entry_ec;
        if (entry.is_regular_file(entry_ec) && hasTomlExtension(entry.path())) {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        logger_.log(LogChannel::System, "No .toml scenarios found in " + scenarios_dir_.string());
        return;
    }
    logger_.log(LogChannel::System, "Scenarios in " + scenarios_dir_.string() + ":");
    for (const auto &name : names) {
        logger_.log(LogChannel::System, " - " + name);
    }
}

std::filesystem::path Application::expectedRecorderCsvPath() const {
    if (!app_config_.recorder_output_path.empty()) {
        return std::filesystem::path(app_config_.recorder_output_path).lexically_normal();
    }
    return (std::filesystem::path(app_config_.output_dir) / "simulation.csv").lexically_normal();
}

void Application::logOgreStatus() const {
#if defined(ECOSIM_OGRE_VIEWER_AVAILABLE) && ECOSIM_OGRE_VIEWER_AVAILABLE
    const bool ogre_available = true;
#else
    const bool ogre_available = false;
#endif
    logger_.log(LogChannel::System,
                std::string("OGRE viewer support: ") + (ogre_available ? "available" : "not built"));
    logger_.log(LogChannel::System,
                std::string("OGRE runtime visualization flag: ") + (ogre_runtime_enabled_ ? "enabled" : "disabled"));
    logger_.log(LogChannel::System, "Expected recorder CSV: " + expectedRecorderCsvPath().string());
}

void Application::setOgreRuntimeEnabled(bool enabled) {
#if defined(ECOSIM_OGRE_VIEWER_AVAILABLE) && ECOSIM_OGRE_VIEWER_AVAILABLE
    ogre_runtime_enabled_ = enabled;
    logger_.log(LogChannel::System,
                std::string("OGRE runtime visualization flag ") + (enabled ? "enabled" : "disabled"));
#else
    if (enabled) {
        ogre_runtime_enabled_ = false;
        logger_.log(LogChannel::System, "OGRE viewer support is not available in this build");
    } else {
        ogre_runtime_enabled_ = false;
        logger_.log(LogChannel::System, "OGRE runtime visualization flag disabled");
    }
#endif
}

bool Application::executeSensitivity(const std::vector<std::string> &args) {
    if (hasDanglingOption(args, "-s", "--scenario") || hasDanglingOption(args, "-p", "--parameter") ||
        hasDanglingOption(args, "--from", "--from") || hasDanglingOption(args, "--to", "--to") ||
        hasDanglingOption(args, "--samples", "--samples") || hasDanglingOption(args, "--output", "--output")) {
        logger_.log(LogChannel::System, "sim.sensitivity has a missing option value");
        return false;
    }

    auto scenario_arg = valueAfterOption(args, "-s", "--scenario");
    auto parameter_arg = valueAfterOption(args, "-p", "--parameter");
    auto from_arg = valueAfterOption(args, "--from", "--from");
    auto to_arg = valueAfterOption(args, "--to", "--to");
    auto samples_arg = valueAfterOption(args, "--samples", "--samples");
    auto output_arg = valueAfterOption(args, "--output", "--output");

    if (!scenario_arg || !parameter_arg || !from_arg || !to_arg || !samples_arg || !output_arg) {
        logger_.log(LogChannel::System,
                    "Usage: sim.sensitivity -s <scenario.toml> -p <parameter> --from <value> --to <value> --samples <n> --output <name>");
        return false;
    }

    double from = 0.0;
    double to = 0.0;
    int samples = 0;
    try {
        from = std::stod(*from_arg);
        to = std::stod(*to_arg);
        samples = std::stoi(*samples_arg);
    } catch (const std::exception &) {
        logger_.log(LogChannel::System, "sim.sensitivity numeric arguments are invalid");
        return false;
    }
    if (samples < 1) {
        logger_.log(LogChannel::System, "sim.sensitivity requires --samples >= 1");
        return false;
    }

    auto resolved = resolveScenarioPath(*scenario_arg, false);
    if (!resolved.ok) {
        logger_.log(LogChannel::System, resolved.error);
        return false;
    }

    ScenarioConfig base_scenario;
    try {
        base_scenario = ConfigLoader::loadScenario(resolved.path.string());
    } catch (const std::exception &ex) {
        logger_.log(LogChannel::System, std::string("Failed to load sensitivity scenario: ") + ex.what());
        return false;
    }

    std::vector<std::pair<double, ReadModel>> rows;
    rows.reserve(static_cast<std::size_t>(samples));
    const int max_ticks = app_config_.max_ticks.value_or(base_scenario.stop_at_tick > 0 ? base_scenario.stop_at_tick + 1 : 1000);

    for (int i = 0; i < samples; ++i) {
        const double alpha = samples == 1 ? 0.0 : static_cast<double>(i) / static_cast<double>(samples - 1);
        const double value = from + (to - from) * alpha;
        auto scenario = base_scenario;
        std::string error;
        if (!applySensitivityParameter(scenario, *parameter_arg, value, error)) {
            logger_.log(LogChannel::System, error);
            return false;
        }
        auto result = runScenarioIsolated(scenario, app_config_, logger_, max_ticks);
        if (!result.ok) {
            logger_.log(LogChannel::System, "Sensitivity sample failed: " + result.error);
            return false;
        }
        rows.push_back({value, result.state});
    }

    auto output_name = std::filesystem::path(*output_arg).filename();
    output_name.replace_extension(".csv");
    auto output_path = (std::filesystem::path(app_config_.output_dir) / output_name).lexically_normal();
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
    if (ec) {
        logger_.log(LogChannel::System, "Cannot create sensitivity output directory: " + ec.message());
        return false;
    }

    std::ofstream file(output_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        logger_.log(LogChannel::System, "Cannot open sensitivity output: " + output_path.string());
        return false;
    }

    std::vector<std::string> header = {"sample_index", "parameter_name", "parameter_value", "scenario_id",
                                       "model_id", "seed", "final_tick", "checksum"};
    std::vector<std::string> species_columns;
    std::vector<std::string> metric_columns;
    if (!rows.empty()) {
        species_columns = rows.front().second.species_names;
        for (const auto &metric : rows.front().second.metrics) {
            metric_columns.push_back(metric.first);
        }
    }
    for (const auto &species : species_columns) {
        header.push_back("state." + species);
    }
    for (const auto &metric : metric_columns) {
        header.push_back("metric." + metric);
    }
    writeCsvRow(file, header);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &state = rows[i].second;
        std::vector<std::string> values = {
            std::to_string(i),
            *parameter_arg,
            formatDouble(rows[i].first),
            state.scenario_id,
            state.model_id,
            std::to_string(state.seed),
            std::to_string(state.tick),
            state.checksum,
        };
        std::map<std::string, double> state_by_species;
        for (std::size_t j = 0; j < state.species_names.size() && j < state.state_vector.size(); ++j) {
            state_by_species[state.species_names[j]] = state.state_vector[j];
        }
        for (const auto &species : species_columns) {
            auto it = state_by_species.find(species);
            values.push_back(it != state_by_species.end() ? formatDouble(it->second) : "");
        }
        for (const auto &metric : metric_columns) {
            auto it = state.metrics.find(metric);
            values.push_back(it != state.metrics.end() ? formatDouble(it->second) : "");
        }
        writeCsvRow(file, values);
    }

    logger_.log(LogChannel::System, "Sensitivity CSV written: " + output_path.string());
    return true;
}

} // namespace ecosim
