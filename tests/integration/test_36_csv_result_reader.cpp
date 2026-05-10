#include "integration/test_framework.h"

#include "viewer/csv_result_reader.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace ecosim_integration {

namespace {
bool near(double lhs, double rhs) {
    return std::abs(lhs - rhs) < 1e-12;
}
} // namespace

class CsvResultReaderExtendedCsvTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.43 csv result reader extended csv";
        const auto path = repoRoot() / "output" / "csv_result_reader_test.csv";
        std::filesystem::create_directories(path.parent_path());

        {
            std::ofstream file(path, std::ios::out | std::ios::trunc);
            file << "tick,time,dt,scenario_id,model_id,seed,integrator,checksum,flags,"
                    "state.rabbit,state.fox,metric.biomass_total,ignored\n";
            file << "7,0.35,0.05,\"scenario,quoted\",glv,42,rk4,abc123,"
                    "shock.rabbit|math_model_connected,11.5,3,14.5,unused\n";
            file << "8,0.4,0.05,\"scenario,quoted\",glv,42,rk4,def456,,12,4,16,unused\n";
        }

        ecosim::viewer::CsvResultReader reader;
        const auto frames = reader.read(path.string());
        std::filesystem::remove(path);

        if (frames.size() != 2) {
            return {name, false, "expected two parsed frames"};
        }

        const auto &first = frames.front();
        if (first.tick != 7 || !near(first.time, 0.35) || !near(first.dt, 0.05) ||
            first.scenario_id != "scenario,quoted" || first.model_id != "glv" ||
            first.integrator != "rk4" || first.checksum != "abc123") {
            return {name, false, "required scalar columns were not parsed correctly"};
        }

        if (first.species_names.size() != 2 || first.species_names[0] != "rabbit" ||
            first.species_names[1] != "fox" || first.state_values.size() != 2 ||
            !near(first.state_values[0], 11.5) || !near(first.state_values[1], 3.0)) {
            return {name, false, "state.* columns were not parsed as species state"};
        }

        auto metric = first.metrics.find("biomass_total");
        if (metric == first.metrics.end() || !near(metric->second, 14.5)) {
            return {name, false, "metric.biomass_total was not parsed"};
        }

        if (first.flags.size() != 2 || first.flags[0] != "shock.rabbit" ||
            first.flags[1] != "math_model_connected") {
            return {name, false, "pipe-separated flags were not parsed"};
        }

        if (!frames[1].flags.empty() || !near(frames[1].state_values[0], 12.0) ||
            !near(frames[1].metrics.at("biomass_total"), 16.0)) {
            return {name, false, "empty flag cells or later rows were not handled"};
        }

        return {name, true, "extended RecorderCsv output can be replayed by the viewer reader"};
    }
};

std::unique_ptr<IIntegrationTest> makeCsvResultReaderExtendedCsvTest() {
    return std::make_unique<CsvResultReaderExtendedCsvTest>();
}

} // namespace ecosim_integration
