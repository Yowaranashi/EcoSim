#pragma once

#include "core/module.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ecosim {

class RecorderCsv : public IModule {
public:
    RecorderCsv(const ModuleInstanceConfig &instance, ModuleContext &context);

    const std::string &typeId() const override { return type_id_; }
    const std::string &instanceId() const override { return instance_id_; }

    void onStart() override;
    void onStop() override;

    const std::vector<SimulationEvent> &events() const { return events_; }
    const std::vector<std::string> &csvLines() const { return csv_lines_; }

private:
    std::filesystem::path resolveOutputPath() const;
    void openOutputFile(bool reset_file);
    bool isFirstTickOfRun(const SimulationEvent &event) const;
    void handleEvent(const SimulationEvent &event);
    void writeCsvForEvent(const SimulationEvent &event);
    void initializeCsvHeader(const SimulationEvent &event);
    std::string buildCsvRow(const SimulationEvent &event) const;
    std::string buildLegacyCsvRow(const SimulationEvent &event) const;
    void appendCsvLine(const std::string &line);

    std::string type_id_;
    std::string instance_id_;
    ModuleContext &context_;
    std::string output_path_;
    std::filesystem::path active_output_path_;
    bool memory_only_ = false;
    std::ofstream file_;
    std::vector<SimulationEvent> events_;
    std::vector<std::string> csv_lines_;
    std::vector<std::string> csv_header_;
    std::vector<std::string> state_species_;
    std::vector<std::string> metric_names_;
    bool csv_header_written_ = false;
    bool legacy_csv_format_ = false;
};

} // namespace ecosim
