#include "viewer/ogre_ecosystem_viewer.h"

#include "core/utils/string_utils.h"

#include <Ogre.h>
#include <OgreFontManager.h>
#include <OgreOverlay.h>
#include <OgreOverlayContainer.h>
#include <OgreOverlayManager.h>
#include <OgreOverlaySystem.h>
#include <OgreTextAreaOverlayElement.h>
#include <OgreWindowEventUtilities.h>
#include <OgreResourceGroupManager.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ecosim::viewer
{

    namespace
    {

        constexpr float kChartLeft = -9.0f;
        constexpr float kChartRight = 4.2f;
        constexpr float kChartBottom = -4.4f;
        constexpr float kChartTop = 4.5f;
        constexpr int kTicks = 5;

        std::string lowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
                           { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        bool isRmModel(const std::string &model_id)
        {
            const auto model = lowerCopy(model_id);
            return model == "rm" || model == "rosenzweig_macarthur" || model.find("rosenzweig") != std::string::npos;
        }

        std::string formatValue(double value)
        {
            std::ostringstream out;
            const double abs_value = std::abs(value);
            out << std::fixed << std::setprecision(abs_value >= 100.0 ? 1 : 2) << value;
            return out.str();
        }

        std::string joinFlags(const std::vector<std::string> &flags)
        {
            std::ostringstream out;
            for (std::size_t i = 0; i < flags.size(); ++i)
            {
                if (i != 0)
                {
                    out << ", ";
                }
                out << flags[i];
            }
            return out.str();
        }

        void addLine(Ogre::ManualObject &object, const Ogre::Vector3 &from, const Ogre::Vector3 &to)
        {
            object.position(from);
            object.position(to);
        }

        void addColoredLine(Ogre::ManualObject &object,
                            const Ogre::Vector3 &from,
                            const Ogre::Vector3 &to,
                            const Ogre::ColourValue &color)
        {
            object.position(from);
            object.colour(color);
            object.position(to);
            object.colour(color);
        }

        void addCross(Ogre::ManualObject &object, const Ogre::Vector3 &center, float radius)
        {
            addLine(object,
                    Ogre::Vector3(center.x - radius, center.y, center.z),
                    Ogre::Vector3(center.x + radius, center.y, center.z));
            addLine(object,
                    Ogre::Vector3(center.x, center.y - radius, center.z),
                    Ogre::Vector3(center.x, center.y + radius, center.z));
        }

        void addColoredCross(Ogre::ManualObject &object,
                             const Ogre::Vector3 &center,
                             float radius,
                             const Ogre::ColourValue &color)
        {
            addColoredLine(object,
                           Ogre::Vector3(center.x - radius, center.y, center.z),
                           Ogre::Vector3(center.x + radius, center.y, center.z),
                           color);
            addColoredLine(object,
                           Ogre::Vector3(center.x, center.y - radius, center.z),
                           Ogre::Vector3(center.x, center.y + radius, center.z),
                           color);
        }

        std::string createMaterial(const std::string &name, const Ogre::ColourValue &color)
        {
            auto material = Ogre::MaterialManager::getSingleton().getByName(name);
            if (material.isNull())
            {
                material = Ogre::MaterialManager::getSingleton().create(
                    name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
            }
            material->setAmbient(color);
            material->setDiffuse(color);
            material->setSelfIllumination(color);
            material->setLightingEnabled(false);
            material->setReceiveShadows(false);
            return name;
        }

        std::string createVertexColorMaterial()
        {
            const std::string name = "ecosim.viewer.vertex_color";
            auto material = Ogre::MaterialManager::getSingleton().getByName(name);
            if (material.isNull())
            {
                material = Ogre::MaterialManager::getSingleton().create(
                    name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
            }
            auto *pass = material->getTechnique(0)->getPass(0);
            pass->setLightingEnabled(false);
            pass->setAmbient(Ogre::ColourValue::White);
            pass->setDiffuse(Ogre::ColourValue::White);
            pass->setSelfIllumination(Ogre::ColourValue::White);
            pass->setVertexColourTracking(
                static_cast<Ogre::TrackVertexColourEnum>(Ogre::TVC_AMBIENT | Ogre::TVC_DIFFUSE | Ogre::TVC_EMISSIVE));
            material->setSelfIllumination(Ogre::ColourValue::White);
            material->setReceiveShadows(false);
            return name;
        }

        double frameXValue(const SimulationFrame &frame, bool use_time)
        {
            return use_time ? frame.time : static_cast<double>(frame.tick);
        }

    } // namespace

    class CsvSimulationData
    {
    public:
        bool load(std::vector<SimulationFrame> input_frames)
        {
            frames_ = std::move(input_frames);
            if (frames_.empty())
            {
                error_ = "CSV is empty or contains no frames";
                return false;
            }

            for (const auto &frame : frames_)
            {
                for (const auto &species : frame.species_names)
                {
                    if (std::find(species_.begin(), species_.end(), species) == species_.end())
                    {
                        species_.push_back(species);
                    }
                }
                for (const auto &metric : frame.metrics)
                {
                    metric_names_.insert(metric.first);
                }
                if (std::abs(frame.time) > 1e-12)
                {
                    use_time_ = true;
                }
            }

            if (species_.empty())
            {
                error_ = "CSV does not contain state.<species> columns";
                return false;
            }

            x_min_ = frameXValue(frames_.front(), use_time_);
            x_max_ = x_min_;
            y_max_ = 0.0;

            for (const auto &frame : frames_)
            {
                const double x = frameXValue(frame, use_time_);
                x_min_ = std::min(x_min_, x);
                x_max_ = std::max(x_max_, x);
                for (const auto &species : species_)
                {
                    const double value = stateValue(frame, species);
                    y_max_ = std::max(y_max_, std::max(0.0, value));
                    species_max_[species] = std::max(species_max_[species], std::max(0.0, value));
                }
            }

            if (std::abs(x_max_ - x_min_) < 1e-12)
            {
                x_max_ = x_min_ + 1.0;
            }
            if (y_max_ <= 0.0)
            {
                y_max_ = 1.0;
            }
            return true;
        }

        const std::vector<SimulationFrame> &frames() const { return frames_; }
        const std::vector<std::string> &species() const { return species_; }
        const std::set<std::string> &metricNames() const { return metric_names_; }
        const std::string &error() const { return error_; }
        bool useTime() const { return use_time_; }
        double xMin() const { return x_min_; }
        double xMax() const { return x_max_; }
        double yMax() const { return y_max_; }

        double speciesMax(const std::string &species) const
        {
            auto it = species_max_.find(species);
            return it == species_max_.end() ? 0.0 : it->second;
        }

        double stateValue(const SimulationFrame &frame, const std::string &species) const
        {
            auto it = frame.state_by_species.find(species);
            if (it != frame.state_by_species.end())
            {
                return it->second;
            }
            for (std::size_t i = 0; i < frame.species_names.size() && i < frame.state_values.size(); ++i)
            {
                if (frame.species_names[i] == species)
                {
                    return frame.state_values[i];
                }
            }
            return 0.0;
        }

    private:
        std::vector<SimulationFrame> frames_;
        std::vector<std::string> species_;
        std::set<std::string> metric_names_;
        std::map<std::string, double> species_max_;
        std::string error_;
        bool use_time_ = false;
        double x_min_ = 0.0;
        double x_max_ = 1.0;
        double y_max_ = 1.0;
    };

    class SpeciesPalette
    {
    public:
        Ogre::ColourValue colorFor(const std::string &species)
        {
            auto it = colors_.find(species);
            if (it != colors_.end())
            {
                return it->second;
            }
            const auto color = colorFromName(species);
            colors_[species] = color;
            return color;
        }

    private:
        static std::uint32_t fnv1a(const std::string &value)
        {
            std::uint32_t hash = 2166136261u;
            for (unsigned char ch : value)
            {
                hash ^= ch;
                hash *= 16777619u;
            }
            return hash;
        }

        static Ogre::ColourValue colorFromName(const std::string &species)
        {
            const auto hash = fnv1a(species);
            const double hue = static_cast<double>(hash % 360u) / 360.0;
            constexpr double saturation = 0.78;
            constexpr double value = 0.96;
            const double c = value * saturation;
            const double h = hue * 6.0;
            const double x = c * (1.0 - std::abs(std::fmod(h, 2.0) - 1.0));
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;
            if (h < 1.0)
            {
                r = c;
                g = x;
            }
            else if (h < 2.0)
            {
                r = x;
                g = c;
            }
            else if (h < 3.0)
            {
                g = c;
                b = x;
            }
            else if (h < 4.0)
            {
                g = x;
                b = c;
            }
            else if (h < 5.0)
            {
                r = x;
                b = c;
            }
            else
            {
                r = c;
                b = x;
            }
            const double m = value - c;
            return Ogre::ColourValue(static_cast<float>(r + m),
                                     static_cast<float>(g + m),
                                     static_cast<float>(b + m));
        }

        std::map<std::string, Ogre::ColourValue> colors_;
    };

    class TextBlock
    {
    public:
        TextBlock(Ogre::OverlayContainer *root,
                  const std::string &name,
                  float x,
                  float y,
                  float width,
                  float height,
                  float char_height,
                  const Ogre::ColourValue &color,
                  const std::string &font_name)
        {
            auto &manager = Ogre::OverlayManager::getSingleton();
            element_ = static_cast<Ogre::TextAreaOverlayElement *>(manager.createOverlayElement("TextArea", name));
            element_->setMetricsMode(Ogre::GMM_PIXELS);
            element_->setPosition(x, y);
            element_->setDimensions(width, height);
            element_->setCharHeight(char_height);
            element_->setFontName(font_name);
            element_->setColour(color);
            root->addChild(element_);
        }

        void setText(const std::string &text)
        {
            element_->setCaption(text);
        }

        void setVisible(bool visible)
        {
            element_->setEnabled(visible);
        }

    private:
        Ogre::TextAreaOverlayElement *element_ = nullptr;
    };

    class LegendOverlay
    {
    public:
        LegendOverlay(Ogre::OverlayContainer *root, const std::string &font_name)
            : text_(root,
                    "EcoSimLegendText",
                    870.0f,
                    548.0f,
                    310.0f,
                    190.0f,
                    13.0f,
                    Ogre::ColourValue(0.92f, 0.94f, 0.90f),
                    font_name) {}

        void update(const CsvSimulationData &data, const SimulationFrame &frame)
        {
            std::ostringstream out;
            out << "Легенда\n";
            for (const auto &species : data.species())
            {
                out << species << ": текущее " << formatValue(data.stateValue(frame, species))
                    << ", макс " << formatValue(data.speciesMax(species)) << "\n";
            }
            text_.setText(out.str());
            text_.setVisible(true);
        }

    private:
        TextBlock text_;
    };

    class InfoOverlay
    {
    public:
        InfoOverlay(Ogre::OverlayContainer *root, const std::string &font_name)
            : title_(root,
                     "EcoSimTitleText",
                     24.0f,
                     18.0f,
                     760.0f,
                     34.0f,
                     22.0f,
                     Ogre::ColourValue(1.0f, 1.0f, 1.0f),
                     font_name),
              axis_(root,
                    "EcoSimAxisText",
                    64.0f,
                    690.0f,
                    800.0f,
                    58.0f,
                    13.0f,
                    Ogre::ColourValue(0.78f, 0.82f, 0.78f),
                    font_name),
              info_(root,
                    "EcoSimInfoText",
                    870.0f,
                    24.0f,
                    310.0f,
                    500.0f,
                    13.0f,
                    Ogre::ColourValue(0.92f, 0.94f, 0.90f),
                    font_name)
        {
            title_.setText("Просмотр EcoSim");
        }

        void updateAxes(const CsvSimulationData &data)
        {
            std::ostringstream out;
            out << (data.useTime() ? "Ось X: время" : "Ось X: тик")
                << "    Ось Y: популяция / биомасса\n";
            out << "X: ";
            for (int i = 0; i < kTicks; ++i)
            {
                const double t = static_cast<double>(i) / static_cast<double>(kTicks - 1);
                const double value = data.xMin() + (data.xMax() - data.xMin()) * t;
                if (i != 0)
                {
                    out << " | ";
                }
                out << formatValue(value);
            }
            out << "\nY: ";
            for (int i = 0; i < kTicks; ++i)
            {
                const double t = static_cast<double>(i) / static_cast<double>(kTicks - 1);
                if (i != 0)
                {
                    out << " | ";
                }
                out << formatValue(data.yMax() * t);
            }
            axis_.setText(out.str());
        }

        void updateInfo(const CsvSimulationData &data,
                        const SimulationFrame &frame,
                        std::size_t frame_index,
                        double speed,
                        bool paused)
        {
            std::ostringstream out;
            out << "Данные кадра\n";
            out << "Сценарий: " << frame.scenario_id << "\n";
            out << "Модель: " << frame.model_id << "\n";
            out << "Интегратор: " << frame.integrator << "\n";
            out << "Тик: " << frame.tick << "/" << data.frames().back().tick << "\n";
            out << "Время: " << formatValue(frame.time) << "\n";
            out << "Кадр: " << (frame_index + 1) << "/" << data.frames().size() << "\n";
            out << "Скорость: " << formatValue(speed) << "x " << (paused ? "(пауза)" : "(проигр.)") << "\n";
            if (!frame.checksum.empty())
            {
                out << "Checksum: " << frame.checksum << "\n";
            }

            out << "\nТекущие значения:\n";
            for (const auto &species : data.species())
            {
                out << species << ": " << formatValue(data.stateValue(frame, species)) << "\n";
            }

            if (!frame.metrics.empty())
            {
                out << "\nМетрики:\n";
                int metric_count = 0;
                for (const auto &metric : frame.metrics)
                {
                    if (metric_count >= 7)
                    {
                        out << "... еще " << (frame.metrics.size() - static_cast<std::size_t>(metric_count)) << "\n";
                        break;
                    }
                    out << metric.first << ": " << formatValue(metric.second) << "\n";
                    ++metric_count;
                }
            }

            if (!frame.flags.empty())
            {
                out << "\nФлаги:\n"
                    << joinFlags(frame.flags) << "\n";
            }
            info_.setText(out.str());
        }

    private:
        TextBlock title_;
        TextBlock axis_;
        TextBlock info_;
    };

    class PlaybackController
    {
    public:
        void updateInput(std::size_t frame_count)
        {
#if defined(_WIN32)
            if (pressed(VK_SPACE))
            {
                paused_ = !paused_;
            }
            if (pressed(VK_RIGHT))
            {
                paused_ = true;
                current_index_ = std::min(current_index_ + 1, frame_count - 1);
            }
            if (pressed(VK_LEFT))
            {
                paused_ = true;
                current_index_ = current_index_ == 0 ? 0 : current_index_ - 1;
            }
            if (pressed(VK_UP))
            {
                speed_ = std::min(16.0, speed_ * 1.4);
            }
            if (pressed(VK_DOWN))
            {
                speed_ = std::max(0.125, speed_ / 1.4);
            }
            if (pressed('R'))
            {
                current_index_ = 0;
                accumulator_ = 0.0;
                paused_ = false;
            }
#else
            (void)frame_count;
#endif
        }

        void advance(double elapsed_seconds, std::size_t frame_count)
        {
            if (paused_ || frame_count == 0)
            {
                return;
            }
            accumulator_ += elapsed_seconds * base_fps_ * speed_;
            while (accumulator_ >= 1.0 && current_index_ + 1 < frame_count)
            {
                ++current_index_;
                accumulator_ -= 1.0;
            }
            if (current_index_ + 1 >= frame_count)
            {
                paused_ = true;
                accumulator_ = 0.0;
            }
        }

        std::size_t index() const { return current_index_; }
        double speed() const { return speed_; }
        bool paused() const { return paused_; }

    private:
#if defined(_WIN32)
        bool pressed(int key)
        {
            const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
            const bool was_down = key_down_[key];
            key_down_[key] = down;
            return down && !was_down;
        }

        std::array<bool, 256> key_down_{};
#endif
        std::size_t current_index_ = 0;
        double speed_ = 1.0;
        double accumulator_ = 0.0;
        const double base_fps_ = 10.0;
        bool paused_ = false;
    };

    class TimeSeriesChartRenderer
    {
    public:
        TimeSeriesChartRenderer(Ogre::SceneManager &scene, const CsvSimulationData &data, SpeciesPalette &palette)
            : scene_(scene), data_(data), palette_(palette)
        {
            createMaterial("ecosim.viewer.grid", Ogre::ColourValue(0.22f, 0.26f, 0.28f));
            createMaterial("ecosim.viewer.axis", Ogre::ColourValue(0.78f, 0.82f, 0.78f));
            createMaterial("ecosim.viewer.marker", Ogre::ColourValue(1.0f, 0.95f, 0.20f));
            createMaterial("ecosim.viewer.phase", Ogre::ColourValue(0.52f, 0.78f, 1.0f));
            vertex_color_material_ = createVertexColorMaterial();

            grid_ = scene_.createManualObject("EcoSimChartGrid");
            series_ = scene_.createManualObject("EcoSimChartSeries");
            marker_ = scene_.createManualObject("EcoSimChartMarker");
            phase_ = scene_.createManualObject("EcoSimPhasePlot");
            grid_->setDynamic(false);
            series_->setDynamic(true);
            marker_->setDynamic(true);
            phase_->setDynamic(true);
            scene_.getRootSceneNode()->attachObject(grid_);
            scene_.getRootSceneNode()->attachObject(series_);
            scene_.getRootSceneNode()->attachObject(marker_);
            scene_.getRootSceneNode()->attachObject(phase_);

            renderGrid();
        }

        void updateCurrent(std::size_t index)
        {
            if (index >= data_.frames().size())
            {
                return;
            }

            const auto &frame = data_.frames()[index];
            renderSeries(index);
            const float x = chartX(frameXValue(frame, data_.useTime()));
            marker_->clear();
            marker_->begin(vertex_color_material_, Ogre::RenderOperation::OT_LINE_LIST);
            addColoredLine(*marker_,
                           Ogre::Vector3(x, kChartBottom, 0.08f),
                           Ogre::Vector3(x, kChartTop, 0.08f),
                           Ogre::ColourValue(1.0f, 0.92f, 0.18f));
            for (const auto &species : data_.species())
            {
                addColoredCross(*marker_,
                                chartPoint(frameXValue(frame, data_.useTime()), data_.stateValue(frame, species), 0.12f),
                                0.10f,
                                palette_.colorFor(species));
            }
            marker_->end();

            renderPhasePlot(index);
        }

    private:
        float chartX(double value) const
        {
            const double t = (value - data_.xMin()) / (data_.xMax() - data_.xMin());
            return kChartLeft + static_cast<float>(std::clamp(t, 0.0, 1.0)) * (kChartRight - kChartLeft);
        }

        float chartY(double value) const
        {
            const double t = std::max(0.0, value) / data_.yMax();
            return kChartBottom + static_cast<float>(std::clamp(t, 0.0, 1.0)) * (kChartTop - kChartBottom);
        }

        Ogre::Vector3 chartPoint(double x, double y, float z) const
        {
            return Ogre::Vector3(chartX(x), chartY(y), z);
        }

        void renderGrid()
        {
            grid_->clear();
            grid_->begin("ecosim.viewer.grid", Ogre::RenderOperation::OT_LINE_LIST);
            for (int i = 0; i < kTicks; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(kTicks - 1);
                const float x = kChartLeft + t * (kChartRight - kChartLeft);
                const float y = kChartBottom + t * (kChartTop - kChartBottom);
                addLine(*grid_, Ogre::Vector3(x, kChartBottom, 0.0f), Ogre::Vector3(x, kChartTop, 0.0f));
                addLine(*grid_, Ogre::Vector3(kChartLeft, y, 0.0f), Ogre::Vector3(kChartRight, y, 0.0f));
            }
            grid_->end();

            grid_->begin("ecosim.viewer.axis", Ogre::RenderOperation::OT_LINE_LIST);
            addLine(*grid_, Ogre::Vector3(kChartLeft, kChartBottom, 0.02f), Ogre::Vector3(kChartRight, kChartBottom, 0.02f));
            addLine(*grid_, Ogre::Vector3(kChartLeft, kChartBottom, 0.02f), Ogre::Vector3(kChartLeft, kChartTop, 0.02f));
            grid_->end();
        }

        void renderSeries(std::size_t current_index)
        {
            series_->clear();
            const auto &frames = data_.frames();
            if (frames.size() < 2 || current_index == 0)
            {
                return;
            }

            series_->begin(vertex_color_material_, Ogre::RenderOperation::OT_LINE_LIST);
            const std::size_t last = std::min(current_index, frames.size() - 1);
            for (const auto &species : data_.species())
            {
                const auto color = palette_.colorFor(species);
                for (std::size_t i = 1; i < frames.size(); ++i)
                {
                    if (i > last)
                    {
                        break;
                    }
                    const auto &previous = frames[i - 1];
                    const auto &current = frames[i];
                    addColoredLine(*series_,
                                   chartPoint(frameXValue(previous, data_.useTime()), data_.stateValue(previous, species), 0.05f),
                                   chartPoint(frameXValue(current, data_.useTime()), data_.stateValue(current, species), 0.05f),
                                   color);
                }
            }
            series_->end();
        }

        void renderPhasePlot(std::size_t index)
        {
            phase_->clear();
            const auto &frame = data_.frames()[index];
            if (!isRmModel(frame.model_id))
            {
                return;
            }
            if (frame.state_by_species.count("prey") == 0 || frame.state_by_species.count("predator") == 0)
            {
                return;
            }

            const float left = 5.15f;
            const float right = 8.85f;
            const float bottom = -4.25f;
            const float top = -1.05f;
            double max_prey = 1.0;
            double max_predator = 1.0;
            for (const auto &candidate : data_.frames())
            {
                max_prey = std::max(max_prey, data_.stateValue(candidate, "prey"));
                max_predator = std::max(max_predator, data_.stateValue(candidate, "predator"));
            }

            auto phasePoint = [&](const SimulationFrame &candidate)
            {
                const float x = left + static_cast<float>(data_.stateValue(candidate, "prey") / max_prey) * (right - left);
                const float y = bottom + static_cast<float>(data_.stateValue(candidate, "predator") / max_predator) * (top - bottom);
                return Ogre::Vector3(x, y, 0.04f);
            };

            phase_->begin("ecosim.viewer.grid", Ogre::RenderOperation::OT_LINE_LIST);
            addLine(*phase_, Ogre::Vector3(left, bottom, 0.0f), Ogre::Vector3(right, bottom, 0.0f));
            addLine(*phase_, Ogre::Vector3(left, bottom, 0.0f), Ogre::Vector3(left, top, 0.0f));
            phase_->end();

            phase_->begin(vertex_color_material_, Ogre::RenderOperation::OT_LINE_LIST);
            for (std::size_t i = 1; i <= index && i < data_.frames().size(); ++i)
            {
                addColoredLine(*phase_,
                               phasePoint(data_.frames()[i - 1]),
                               phasePoint(data_.frames()[i]),
                               Ogre::ColourValue(0.52f, 0.78f, 1.0f));
            }
            addColoredCross(*phase_, phasePoint(frame), 0.08f, Ogre::ColourValue(1.0f, 0.92f, 0.18f));
            phase_->end();
        }

        Ogre::SceneManager &scene_;
        const CsvSimulationData &data_;
        SpeciesPalette &palette_;
        Ogre::ManualObject *grid_ = nullptr;
        Ogre::ManualObject *series_ = nullptr;
        Ogre::ManualObject *marker_ = nullptr;
        Ogre::ManualObject *phase_ = nullptr;
        std::string vertex_color_material_;
    };

    struct OgreEcosystemViewer::Impl
    {
        std::unique_ptr<Ogre::Root> root_owner;
        std::unique_ptr<Ogre::OverlaySystem> overlay_system;
        std::unique_ptr<CsvSimulationData> data;
        std::unique_ptr<SpeciesPalette> palette;
        std::unique_ptr<TimeSeriesChartRenderer> chart;
        std::unique_ptr<LegendOverlay> legend;
        std::unique_ptr<InfoOverlay> info;
        PlaybackController playback;

        Ogre::Root *root = nullptr;
        Ogre::RenderWindow *window = nullptr;
        Ogre::SceneManager *scene = nullptr;
        Ogre::Camera *camera = nullptr;
        Ogre::Overlay *overlay = nullptr;
        Ogre::OverlayContainer *overlay_root = nullptr;
        std::string font_name = "EcoSimViewerFont";

        bool initialize()
        {
            try
            {
                root_owner = std::make_unique<Ogre::Root>();
                root = root_owner.get();

                const auto &renderers = root->getAvailableRenderers();
                if (renderers.empty())
                {
                    std::cerr << "Failed to initialize OGRE viewer: no render systems available" << std::endl;
                    return false;
                }

                Ogre::RenderSystem *selected = renderers.front();
                for (auto *renderer : renderers)
                {
                    if (renderer && renderer->getName().find("OpenGL Rendering Subsystem") != Ogre::String::npos)
                    {
                        selected = renderer;
                        break;
                    }
                }
                root->setRenderSystem(selected);
                root->initialise(false);

                Ogre::NameValuePairList params;
                params["FSAA"] = "1";
                params["vsync"] = "Yes";
                window = root->createRenderWindow("EcoSim Viewer", 1200, 760, false, &params);
                createScene();
                return true;
            }
            catch (const std::exception &ex)
            {
                std::cerr << "Failed to initialize OGRE viewer: " << ex.what() << std::endl;
                return false;
            }
        }

        void shutdown()
        {
            if (root_owner)
            {
                root_owner->shutdown();
                root_owner.reset();
            }
            root = nullptr;
            window = nullptr;
            scene = nullptr;
            camera = nullptr;
            overlay = nullptr;
            overlay_root = nullptr;
        }

        void createScene()
        {
            scene = root->createSceneManager();
            overlay_system = std::make_unique<Ogre::OverlaySystem>();
            scene->addRenderQueueListener(overlay_system.get());
            scene->setAmbientLight(Ogre::ColourValue(1.0f, 1.0f, 1.0f));

            camera = scene->createCamera("EcoSimChartCamera");
            camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
            camera->setOrthoWindow(20.0f, 12.6667f);
            camera->setNearClipDistance(0.1f);
            camera->setFarClipDistance(1000.0f);
            auto *camera_node = scene->getRootSceneNode()->createChildSceneNode();
            camera_node->setPosition(0.0f, 0.0f, 100.0f);
            camera_node->lookAt(Ogre::Vector3(0.0f, 0.0f, 0.0f), Ogre::Node::TS_WORLD);
            camera_node->attachObject(camera);

            auto *viewport = window->addViewport(camera);
            viewport->setBackgroundColour(Ogre::ColourValue(0.025f, 0.030f, 0.035f));

            createFont();
            createOverlay();
        }

        void createFont()
        {
            const auto group = Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;

#if defined(_WIN32)
            auto &resources = Ogre::ResourceGroupManager::getSingleton();

            // Подключаем стандартную папку шрифтов Windows.
            // Без этого OGRE может не найти arial.ttf.
            try
            {
                resources.addResourceLocation("C:/Windows/Fonts", "FileSystem", group);
            }
            catch (...)
            {
                // Если папка уже была добавлена, просто продолжаем.
            }

            // Обновляем список ресурсов после добавления папки.
            try
            {
                resources.initialiseResourceGroup(group);
            }
            catch (...)
            {
                // Если группа уже инициализирована, это не критично.
            }
#endif

            auto font = Ogre::FontManager::getSingleton().getByName(font_name, group);
            if (font.isNull())
            {
                font = Ogre::FontManager::getSingleton().create(font_name, group);
            }

            if (font->isLoaded())
            {
                font->unload();
            }

            font->setType(Ogre::FT_TRUETYPE);

#if defined(_WIN32)
            font->setSource("arial.ttf");
#else
            font->setSource("arial.ttf");
#endif

            font->setTrueTypeSize(18);
            font->setTrueTypeResolution(96);
            font->clearCodePointRanges();
            font->addCodePointRange(Ogre::Font::CodePointRange(32, 126));
            font->addCodePointRange(Ogre::Font::CodePointRange(0x0400, 0x052F));
            font->addCodePointRange(Ogre::Font::CodePointRange(0x2010, 0x201F));
            font->addCodePointRange(Ogre::Font::CodePointRange(0x2116, 0x2116));
            font->load();
        }

        void createOverlay()
        {
            auto &manager = Ogre::OverlayManager::getSingleton();
            overlay = manager.create("EcoSimViewerOverlay");
            overlay_root = static_cast<Ogre::OverlayContainer *>(manager.createOverlayElement("Panel", "EcoSimOverlayRoot"));
            overlay_root->setMetricsMode(Ogre::GMM_PIXELS);
            overlay_root->setPosition(0.0f, 0.0f);
            overlay_root->setDimensions(1200.0f, 760.0f);
            overlay->add2D(overlay_root);
            overlay->show();

            legend = std::make_unique<LegendOverlay>(overlay_root, font_name);
            info = std::make_unique<InfoOverlay>(overlay_root, font_name);
        }

        void run(std::vector<SimulationFrame> input_frames)
        {
            data = std::make_unique<CsvSimulationData>();
            if (!data->load(std::move(input_frames)))
            {
                std::cerr << data->error() << std::endl;
                return;
            }

            palette = std::make_unique<SpeciesPalette>();
            chart = std::make_unique<TimeSeriesChartRenderer>(*scene, *data, *palette);
            info->updateAxes(*data);

            auto last_time = std::chrono::steady_clock::now();
            while (window && !window->isClosed())
            {
                Ogre::WindowEventUtilities::messagePump();
                const auto now = std::chrono::steady_clock::now();
                const std::chrono::duration<double> elapsed = now - last_time;
                last_time = now;

                playback.updateInput(data->frames().size());
                playback.advance(elapsed.count(), data->frames().size());

                const auto index = playback.index();
                const auto &frame = data->frames()[index];
                chart->updateCurrent(index);
                legend->update(*data, frame);
                info->updateInfo(*data, frame, index, playback.speed(), playback.paused());
                writeConsoleStatus(frame);

                if (!root->renderOneFrame())
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            std::cout << std::endl;
        }

        void writeConsoleStatus(const SimulationFrame &frame) const
        {
            std::cout << '\r' << "tick=" << frame.tick << " model=" << frame.model_id
                      << " scenario=" << frame.scenario_id << " checksum=" << frame.checksum << "        " << std::flush;
        }
    };

    OgreEcosystemViewer::OgreEcosystemViewer() : impl_(std::make_unique<Impl>()) {}

    OgreEcosystemViewer::~OgreEcosystemViewer() = default;

    bool OgreEcosystemViewer::initialize()
    {
        return impl_->initialize();
    }

    void OgreEcosystemViewer::run(const std::vector<SimulationFrame> &frames)
    {
        impl_->run(frames);
    }

    void OgreEcosystemViewer::shutdown()
    {
        impl_->shutdown();
    }

} // namespace ecosim::viewer
