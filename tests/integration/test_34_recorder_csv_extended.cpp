#include "integration/test_framework.h"

#include "modules/recorder_csv.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace ecosim_integration {

namespace {
ecosim::SimulationEvent makeExtendedEvent(int tick, double rabbit, double fox) {
    ecosim::SimulationEvent event;
    event.type = "world.tick";
    event.tick = tick;
    event.payload["scenario_id"] = "recorder-scenario";
    event.payload["model_id"] = "glv";
    event.payload["integrator"] = "rk4";
    event.payload["checksum"] = "abc123";
    event.numeric_payload["tick"] = static_cast<double>(tick);
    event.numeric_payload["time"] = tick * 0.25;
    event.numeric_payload["dt"] = 0.25;
    event.numeric_payload["seed"] = 42.0;
    event.string_list_payload["species_names"] = {"rabbit", "fox"};
    event.numeric_vector_payload["state_vector"] = {rabbit, fox};
    event.metrics["biomass_total"] = rabbit + fox;
    event.metrics["species_count"] = 2.0;
    event.metrics["zeta"] = 9.0;
    return event;
}

std::unique_ptr<ecosim::RecorderCsv> makeRecorder(ecosim::EventBus &event_bus,
                                                  ecosim::AppConfig &app_config,
                                                  std::ostringstream &log_stream) {
    auto logger = std::make_unique<ecosim::Logger>(log_stream);
    auto context = std::make_unique<ecosim::ModuleContext>(*logger, event_bus, app_config);
    ecosim::ModuleInstanceConfig instance;
    instance.type_id = "recorder";
    instance.instance_id = "csv";
    instance.params["sink"] = "memory";

    struct Holder : ecosim::RecorderCsv {
        Holder(const ecosim::ModuleInstanceConfig &instance,
               ecosim::ModuleContext &context,
               std::unique_ptr<ecosim::Logger> logger,
               std::unique_ptr<ecosim::ModuleContext> context_owner)
            : ecosim::RecorderCsv(instance, context), logger_owner(std::move(logger)),
              module_context_owner(std::move(context_owner)) {}

        std::unique_ptr<ecosim::Logger> logger_owner;
        std::unique_ptr<ecosim::ModuleContext> module_context_owner;
    };

    auto &context_ref = *context;
    return std::make_unique<Holder>(instance, context_ref, std::move(logger), std::move(context));
}

std::vector<std::string> splitSimpleCsv(const std::string &line) {
    std::vector<std::string> result;
    std::string current;
    for (char ch : line) {
        if (ch == ',') {
            result.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    result.push_back(current);
    return result;
}

bool emitAndDeliver(ecosim::EventBus &event_bus, const ecosim::SimulationEvent &event) {
    event_bus.emit(event);
    event_bus.deliverBuffered();
    return true;
}
} // namespace

class RecorderCsvExtendedHeaderTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.38 recorder csv extended header";
        std::ostringstream log_stream;
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        auto recorder = makeRecorder(event_bus, app_config, log_stream);
        recorder->onStart();

        emitAndDeliver(event_bus, makeExtendedEvent(1, 10.0, 2.0));
        const auto &lines = recorder->csvLines();
        const std::string expected =
            "tick,time,dt,scenario_id,model_id,seed,integrator,checksum,flags,state.rabbit,state.fox,"
            "metric.biomass_total,metric.species_count,metric.zeta";
        if (lines.empty() || lines[0] != expected) {
            return {name, false, "extended CSV header is not in the expected order"};
        }

        return {name, true, "extended CSV header includes required, state and metric columns"};
    }
};

class RecorderCsvExtendedRowTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.39 recorder csv extended row";
        std::ostringstream log_stream;
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        auto recorder = makeRecorder(event_bus, app_config, log_stream);
        recorder->onStart();

        emitAndDeliver(event_bus, makeExtendedEvent(2, 11.5, 3.0));
        const auto &lines = recorder->csvLines();
        if (lines.size() != 2) {
            return {name, false, "expected header and one data row"};
        }

        const auto header = splitSimpleCsv(lines[0]);
        const auto row = splitSimpleCsv(lines[1]);
        if (header.size() != row.size()) {
            return {name, false, "row column count does not match header"};
        }

        auto valueAt = [&](const std::string &column) -> std::string {
            for (std::size_t i = 0; i < header.size(); ++i) {
                if (header[i] == column) {
                    return row[i];
                }
            }
            return {};
        };

        if (valueAt("tick") != "2" || valueAt("time") != "0.5" || valueAt("model_id") != "glv" ||
            valueAt("checksum") != "abc123" || valueAt("state.rabbit") != "11.5" ||
            valueAt("metric.biomass_total") != "14.5") {
            return {name, false, "extended row did not serialize the expected typed payload values"};
        }

        return {name, true, "extended CSV row serializes typed payload values"};
    }
};

class RecorderCsvFlagsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.40 recorder csv flags";
        std::ostringstream log_stream;
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        auto recorder = makeRecorder(event_bus, app_config, log_stream);
        recorder->onStart();

        auto event = makeExtendedEvent(1, 10.0, 2.0);
        event.flags = {"shock.rabbit", "param_changed.growth.rabbit"};
        emitAndDeliver(event_bus, event);

        const auto row = splitSimpleCsv(recorder->csvLines()[1]);
        if (row.size() < 9 || row[8] != "shock.rabbit|param_changed.growth.rabbit") {
            return {name, false, "flags were not joined with pipe separators"};
        }

        return {name, true, "flags are written as a single pipe-separated CSV field"};
    }
};

class RecorderCsvLegacyEventTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.41 recorder csv legacy event";
        std::ostringstream log_stream;
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        auto recorder = makeRecorder(event_bus, app_config, log_stream);
        recorder->onStart();

        ecosim::SimulationEvent event;
        event.type = "world.tick";
        event.tick = 4;
        event.payload["seed"] = "9";
        event.payload["energy_total"] = "18";
        emitAndDeliver(event_bus, event);

        const auto &lines = recorder->csvLines();
        if (lines.size() != 2 || lines[0] != "tick,seed,energy_total" || lines[1] != "4,9,18") {
            return {name, false, "legacy event did not produce the old minimal CSV shape"};
        }

        return {name, true, "legacy events still serialize tick, seed and energy_total"};
    }
};

class RecorderCsvStableColumnsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.42 recorder csv stable columns";
        std::ostringstream log_stream;
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        auto recorder = makeRecorder(event_bus, app_config, log_stream);
        recorder->onStart();

        emitAndDeliver(event_bus, makeExtendedEvent(1, 10.0, 2.0));
        emitAndDeliver(event_bus, makeExtendedEvent(2, 11.0, 3.0));

        const auto header = splitSimpleCsv(recorder->csvLines()[0]);
        const auto first = splitSimpleCsv(recorder->csvLines()[1]);
        const auto second = splitSimpleCsv(recorder->csvLines()[2]);
        auto indexOf = [&](const std::string &column) -> std::size_t {
            for (std::size_t i = 0; i < header.size(); ++i) {
                if (header[i] == column) {
                    return i;
                }
            }
            return header.size();
        };

        const auto rabbit = indexOf("state.rabbit");
        const auto fox = indexOf("state.fox");
        if (rabbit >= header.size() || fox >= header.size() || first[rabbit] != "10" || first[fox] != "2" ||
            second[rabbit] != "11" || second[fox] != "3") {
            return {name, false, "state columns were not stable across events"};
        }

        return {name, true, "state values stay under stable species columns"};
    }
};

std::unique_ptr<IIntegrationTest> makeRecorderCsvExtendedHeaderTest() {
    return std::make_unique<RecorderCsvExtendedHeaderTest>();
}

std::unique_ptr<IIntegrationTest> makeRecorderCsvExtendedRowTest() {
    return std::make_unique<RecorderCsvExtendedRowTest>();
}

std::unique_ptr<IIntegrationTest> makeRecorderCsvFlagsTest() {
    return std::make_unique<RecorderCsvFlagsTest>();
}

std::unique_ptr<IIntegrationTest> makeRecorderCsvLegacyEventTest() {
    return std::make_unique<RecorderCsvLegacyEventTest>();
}

std::unique_ptr<IIntegrationTest> makeRecorderCsvStableColumnsTest() {
    return std::make_unique<RecorderCsvStableColumnsTest>();
}

} // namespace ecosim_integration
