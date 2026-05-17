#include "modules/recorder_csv.h"

#include "core/module_registry.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace ecosim {

namespace {
const std::vector<std::string> kRequiredColumns = {
    "tick", "time", "dt", "scenario_id", "model_id", "seed", "integrator", "checksum", "flags",
};

const std::vector<std::string> kKnownMetrics = {
    "biomass_total",
    "min_population",
    "max_population",
    "dominant_species_index",
    "species_count",
    "prey",
    "predator",
    "predation_flow",
    "phase_x",
    "phase_y",
};

std::string formatDouble(double value) {
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

std::string csvEscape(const std::string &value) {
    const bool needs_quotes =
        value.find_first_of(",\"\r\n") != std::string::npos;
    if (!needs_quotes) {
        return value;
    }

    std::string escaped = "\"";
    escaped.reserve(value.size() + 2 + static_cast<std::size_t>(std::count(value.begin(), value.end(), '"')));
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

std::string joinCsv(const std::vector<std::string> &values) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << csvEscape(values[i]);
    }
    return out.str();
}

std::string joinPipe(const std::vector<std::string> &values) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << '|';
        }
        out << values[i];
    }
    return out.str();
}

std::string getPayloadString(const SimulationEvent &event, const std::string &key) {
    auto string_it = event.payload.find(key);
    if (string_it != event.payload.end()) {
        return string_it->second;
    }
    auto numeric_it = event.numeric_payload.find(key);
    if (numeric_it != event.numeric_payload.end()) {
        return formatDouble(numeric_it->second);
    }
    return "";
}

std::string getRequiredValue(const SimulationEvent &event, const std::string &column) {
    if (column == "tick") {
        auto numeric_it = event.numeric_payload.find("tick");
        if (numeric_it != event.numeric_payload.end()) {
            return formatDouble(numeric_it->second);
        }
        auto payload_it = event.payload.find("tick");
        if (payload_it != event.payload.end()) {
            return payload_it->second;
        }
        return std::to_string(event.tick);
    }
    if (column == "flags") {
        if (!event.flags.empty()) {
            return joinPipe(event.flags);
        }
        return getPayloadString(event, "flags");
    }
    return getPayloadString(event, column);
}

const std::vector<std::string> &emptyStringVector() {
    static const std::vector<std::string> empty;
    return empty;
}

const std::vector<double> &emptyDoubleVector() {
    static const std::vector<double> empty;
    return empty;
}

const std::vector<std::string> &getSpeciesNames(const SimulationEvent &event) {
    auto list_it = event.string_list_payload.find("species_names");
    if (list_it != event.string_list_payload.end()) {
        return list_it->second;
    }
    list_it = event.string_list_payload.find("species");
    if (list_it != event.string_list_payload.end()) {
        return list_it->second;
    }
    return emptyStringVector();
}

const std::vector<double> &getStateVector(const SimulationEvent &event) {
    auto vector_it = event.numeric_vector_payload.find("state_vector");
    if (vector_it != event.numeric_vector_payload.end()) {
        return vector_it->second;
    }
    vector_it = event.numeric_vector_payload.find("state");
    if (vector_it != event.numeric_vector_payload.end()) {
        return vector_it->second;
    }
    return emptyDoubleVector();
}

bool hasExtendedPayload(const SimulationEvent &event) {
    return event.payload.count("scenario_id") != 0 || event.payload.count("model_id") != 0 ||
           event.payload.count("integrator") != 0 || event.payload.count("checksum") != 0 ||
           event.numeric_payload.count("time") != 0 || event.numeric_payload.count("dt") != 0 ||
           !getSpeciesNames(event).empty() || !getStateVector(event).empty() || !event.metrics.empty() ||
           !event.flags.empty();
}
} // namespace

RecorderCsv::RecorderCsv(const ModuleInstanceConfig &instance, ModuleContext &context)
    : type_id_(instance.type_id), instance_id_(instance.instance_id), context_(context) {
    auto sink_it = instance.params.find("sink");
    if (sink_it != instance.params.end() && sink_it->second == "memory") {
        memory_only_ = true;
    }
    auto path_it = instance.params.find("path");
    if (path_it != instance.params.end()) {
        output_path_ = path_it->second;
    }
}

void RecorderCsv::onStart() {
    openOutputFile(true);
    context_.eventBus().subscribe("recorder.configure", [this](const SimulationEvent &event) {
        auto path = event.payload.find("path");
        if (path != event.payload.end()) {
            output_path_ = path->second;
            openOutputFile(true);
        }
    });
    context_.eventBus().subscribe("world.tick", [this](const SimulationEvent &event) { handleEvent(event); });
}

std::filesystem::path RecorderCsv::resolveOutputPath() const {
    std::filesystem::path resolved_output_path;
    if (!output_path_.empty()) {
        resolved_output_path = std::filesystem::path(output_path_);
    } else if (!context_.config().recorder_output_path.empty()) {
        resolved_output_path = std::filesystem::path(context_.config().recorder_output_path);
    } else {
        resolved_output_path = std::filesystem::path(context_.config().output_dir) / "simulation.csv";
    }

    if (resolved_output_path.is_relative()) {
        const auto &runtime_root = context_.config().runtime_root;
        auto base = runtime_root.empty() ? std::filesystem::current_path() : std::filesystem::path(runtime_root);
        resolved_output_path = base / resolved_output_path;
    }
    return resolved_output_path.lexically_normal();
}

void RecorderCsv::openOutputFile(bool reset_file) {
    if (file_.is_open()) {
        file_.close();
    }
    if (reset_file) {
        events_.clear();
        csv_lines_.clear();
        csv_header_.clear();
        state_species_.clear();
        metric_names_.clear();
        csv_header_written_ = false;
        legacy_csv_format_ = false;
    }
    if (!memory_only_) {
        active_output_path_ = resolveOutputPath();
        output_path_ = active_output_path_.string();
        auto parent_path = active_output_path_.parent_path();
        if (!parent_path.empty()) {
            std::filesystem::create_directories(parent_path);
        }
        file_.open(active_output_path_, std::ios::out | std::ios::trunc);
    }
}

bool RecorderCsv::isFirstTickOfRun(const SimulationEvent &event) const {
    auto tick_it = event.numeric_payload.find("tick");
    if (tick_it != event.numeric_payload.end()) {
        return tick_it->second <= 1.0;
    }
    return event.tick <= 1;
}

void RecorderCsv::onStop() {
    if (file_.is_open()) {
        file_.close();
    }
}

void RecorderCsv::handleEvent(const SimulationEvent &event) {
    if (!events_.empty() && isFirstTickOfRun(event)) {
        openOutputFile(true);
    }
    events_.push_back(event);
    writeCsvForEvent(event);
}

void RecorderCsv::writeCsvForEvent(const SimulationEvent &event) {
    if (!csv_header_written_) {
        initializeCsvHeader(event);
        appendCsvLine(joinCsv(csv_header_));
    }

    appendCsvLine(legacy_csv_format_ ? buildLegacyCsvRow(event) : buildCsvRow(event));
}

void RecorderCsv::initializeCsvHeader(const SimulationEvent &event) {
    legacy_csv_format_ = !hasExtendedPayload(event);
    csv_header_.clear();
    state_species_.clear();
    metric_names_.clear();

    if (legacy_csv_format_) {
        csv_header_ = {"tick", "seed", "energy_total"};
        csv_header_written_ = true;
        return;
    }

    csv_header_ = kRequiredColumns;
    state_species_ = getSpeciesNames(event);
    for (const auto &species : state_species_) {
        csv_header_.push_back("state." + species);
    }

    for (const auto &metric : kKnownMetrics) {
        if (event.metrics.count(metric) != 0) {
            metric_names_.push_back(metric);
        }
    }
    for (const auto &metric : event.metrics) {
        if (std::find(metric_names_.begin(), metric_names_.end(), metric.first) == metric_names_.end()) {
            metric_names_.push_back(metric.first);
        }
    }
    for (const auto &metric : metric_names_) {
        csv_header_.push_back("metric." + metric);
    }

    csv_header_written_ = true;
}

std::string RecorderCsv::buildCsvRow(const SimulationEvent &event) const {
    std::vector<std::string> row;
    row.reserve(csv_header_.size());

    for (const auto &column : kRequiredColumns) {
        row.push_back(getRequiredValue(event, column));
    }

    const auto &species_names = getSpeciesNames(event);
    const auto &state_vector = getStateVector(event);
    if (state_species_ == species_names) {
        for (std::size_t i = 0; i < state_species_.size(); ++i) {
            row.push_back(i < state_vector.size() ? formatDouble(state_vector[i]) : "");
        }
        for (const auto &metric : metric_names_) {
            auto it = event.metrics.find(metric);
            row.push_back(it != event.metrics.end() ? formatDouble(it->second) : "");
        }

        return joinCsv(row);
    }

    std::unordered_map<std::string, double> state_by_species;
    state_by_species.reserve(species_names.size());
    for (std::size_t i = 0; i < species_names.size() && i < state_vector.size(); ++i) {
        state_by_species[species_names[i]] = state_vector[i];
    }
    for (const auto &species : state_species_) {
        auto it = state_by_species.find(species);
        row.push_back(it != state_by_species.end() ? formatDouble(it->second) : "");
    }

    for (const auto &metric : metric_names_) {
        auto it = event.metrics.find(metric);
        row.push_back(it != event.metrics.end() ? formatDouble(it->second) : "");
    }

    return joinCsv(row);
}

std::string RecorderCsv::buildLegacyCsvRow(const SimulationEvent &event) const {
    std::vector<std::string> row;
    auto tick = getPayloadString(event, "tick");
    row.push_back(tick.empty() ? std::to_string(event.tick) : tick);
    row.push_back(getPayloadString(event, "seed"));
    row.push_back(getPayloadString(event, "energy_total"));
    return joinCsv(row);
}

void RecorderCsv::appendCsvLine(const std::string &line) {
    csv_lines_.push_back(line);
    if (!memory_only_ && file_.is_open()) {
        file_ << line << '\n';
        file_.flush();
    }
}

} // namespace ecosim

#if defined(_WIN32)
#define ECOSIM_MODULE_EXPORT __declspec(dllexport)
#else
#define ECOSIM_MODULE_EXPORT
#endif

extern "C" ECOSIM_MODULE_EXPORT void ecosimRegisterModule(ecosim::ModuleRegistry &registry) {
    registry.registerFactory("recorder",
                             [](const ecosim::ModuleInstanceConfig &instance, ecosim::ModuleContext &context) {
                                 return std::make_unique<ecosim::RecorderCsv>(instance, context);
                             });
}

#undef ECOSIM_MODULE_EXPORT
