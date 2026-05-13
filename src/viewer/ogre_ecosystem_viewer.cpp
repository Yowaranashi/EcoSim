#include "viewer/ogre_ecosystem_viewer.h"

#include "core/utils/string_utils.h"

#include <Ogre.h>
#include <OgreWindowEventUtilities.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <thread>

namespace ecosim::viewer {

namespace {
constexpr double kPi = 3.14159265358979323846;

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isRmModel(const SimulationFrame &frame) {
    const auto model = lowerCopy(frame.model_id);
    return model == "rm" || model == "rosenzweig_macarthur" || model.find("rosenzweig") != std::string::npos;
}

double metricOrStateSum(const SimulationFrame &frame, const std::string &metric_name) {
    auto metric = frame.metrics.find(metric_name);
    if (metric != frame.metrics.end()) {
        return metric->second;
    }

    double total = 0.0;
    for (double value : frame.state_values) {
        total += std::max(0.0, value);
    }
    return total;
}

std::size_t findSpeciesIndex(const SimulationFrame &frame, const std::string &name) {
    const auto name_lower = lowerCopy(name);
    for (std::size_t i = 0; i < frame.species_names.size(); ++i) {
        if (lowerCopy(frame.species_names[i]) == name_lower) {
            return i;
        }
    }
    return frame.species_names.size();
}

std::pair<double, double> preyPredatorValues(const SimulationFrame &frame) {
    auto prey_index = findSpeciesIndex(frame, "prey");
    auto predator_index = findSpeciesIndex(frame, "predator");
    if (prey_index < frame.state_values.size() && predator_index < frame.state_values.size()) {
        return {frame.state_values[prey_index], frame.state_values[predator_index]};
    }
    if (frame.state_values.size() >= 2) {
        return {frame.state_values[0], frame.state_values[1]};
    }
    return {0.0, 0.0};
}

std::string createMaterial(const std::string &name,
                           const Ogre::ColourValue &color,
                           bool lighting_enabled = true) {
    auto material = Ogre::MaterialManager::getSingleton().getByName(name);
    if (material.isNull()) {
        material = Ogre::MaterialManager::getSingleton().create(
            name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    }
    material->setAmbient(color);
    material->setDiffuse(color);
    material->setSpecular(Ogre::ColourValue(0.08f, 0.08f, 0.08f));
    material->setSelfIllumination(lighting_enabled ? Ogre::ColourValue::Black : color);
    material->setLightingEnabled(lighting_enabled);
    material->setReceiveShadows(false);
    return name;
}

void addBox(Ogre::ManualObject &object,
            float x0,
            float x1,
            float z0,
            float z1,
            float y0,
            float y1) {
    const auto base = static_cast<unsigned int>(object.getCurrentVertexCount());
    object.position(x0, y0, z0);
    object.position(x1, y0, z0);
    object.position(x1, y0, z1);
    object.position(x0, y0, z1);
    object.position(x0, y1, z0);
    object.position(x1, y1, z0);
    object.position(x1, y1, z1);
    object.position(x0, y1, z1);

    object.triangle(base + 4, base + 5, base + 6);
    object.triangle(base + 4, base + 6, base + 7);
    object.triangle(base + 0, base + 2, base + 1);
    object.triangle(base + 0, base + 3, base + 2);
    object.triangle(base + 0, base + 4, base + 7);
    object.triangle(base + 0, base + 7, base + 3);
    object.triangle(base + 1, base + 2, base + 6);
    object.triangle(base + 1, base + 6, base + 5);
    object.triangle(base + 3, base + 7, base + 6);
    object.triangle(base + 3, base + 6, base + 2);
    object.triangle(base + 0, base + 1, base + 5);
    object.triangle(base + 0, base + 5, base + 4);
}

void addLine(Ogre::ManualObject &object, const Ogre::Vector3 &from, const Ogre::Vector3 &to) {
    object.position(from);
    object.position(to);
}

} // namespace

struct OgreEcosystemViewer::Impl {
    static constexpr int kGridSize = 20;
    static constexpr float kCellSpacing = 1.0f;

    std::unique_ptr<Ogre::Root> root_owner;
    Ogre::Root *root = nullptr;
    Ogre::RenderWindow *window = nullptr;
    Ogre::SceneManager *scene = nullptr;
    Ogre::Camera *camera = nullptr;
    Ogre::SceneNode *glv_node = nullptr;
    Ogre::SceneNode *rm_node = nullptr;
    Ogre::SceneNode *shock_node = nullptr;
    Ogre::ManualObject *glv_surface = nullptr;
    Ogre::ManualObject *rm_trajectory = nullptr;
    Ogre::ManualObject *rm_current = nullptr;
    Ogre::ManualObject *rm_bars = nullptr;
    Ogre::ManualObject *rm_axes = nullptr;
    Ogre::ManualObject *shock_object = nullptr;

    const std::vector<SimulationFrame> *frames = nullptr;
    std::size_t current_frame_index = 0;
    std::map<std::string, Ogre::Vector3> anchors;
    std::vector<std::string> species_materials;
    std::vector<std::string> last_species_names;
    Ogre::Vector3 current_rm_point = Ogre::Vector3::ZERO;

    double glv_height_scale = 0.2;
    double rm_scale_x = 0.2;
    double rm_scale_z = 0.2;
    double rm_scale_y = 0.05;

    struct ShockEffect {
        Ogre::Vector3 position = Ogre::Vector3::ZERO;
        double remaining = 0.0;
        double duration = 0.0;
    };
    std::vector<ShockEffect> shocks;

    bool initialize() {
        try {
            root_owner = std::make_unique<Ogre::Root>();
            root = root_owner.get();

            if (!root->restoreConfig()) {
                const auto &renderers = root->getAvailableRenderers();
                if (renderers.empty()) {
                    std::cerr << "Failed to initialize OGRE viewer: no render systems available" << std::endl;
                    return false;
                }
                root->setRenderSystem(renderers.front());
            }

            window = root->initialise(true, "EcoSim OGRE Viewer");
            createScene();
            return true;
        } catch (const std::exception &ex) {
            std::cerr << "Failed to initialize OGRE viewer: " << ex.what() << std::endl;
            return false;
        }
    }

    void shutdown() {
        if (root_owner) {
            root_owner->shutdown();
            root_owner.reset();
        }
        root = nullptr;
        window = nullptr;
        scene = nullptr;
        camera = nullptr;
        glv_node = nullptr;
        rm_node = nullptr;
        shock_node = nullptr;
        glv_surface = nullptr;
        rm_trajectory = nullptr;
        rm_current = nullptr;
        rm_bars = nullptr;
        rm_axes = nullptr;
        shock_object = nullptr;
    }

    void createScene() {
        scene = root->createSceneManager(Ogre::ST_GENERIC);
        scene->setAmbientLight(Ogre::ColourValue(0.35f, 0.35f, 0.38f));

        auto *light = scene->createLight("EcoSimKeyLight");
        light->setType(Ogre::Light::LT_DIRECTIONAL);
        light->setDiffuseColour(Ogre::ColourValue(0.9f, 0.9f, 0.82f));
        auto *light_node = scene->getRootSceneNode()->createChildSceneNode();
        light_node->setDirection(Ogre::Vector3(-0.4f, -1.0f, -0.35f));
        light_node->attachObject(light);

        camera = scene->createCamera("EcoSimCamera");
        camera->setNearClipDistance(0.1f);
        camera->setFarClipDistance(1000.0f);
        camera->setAutoAspectRatio(true);
        auto *camera_node = scene->getRootSceneNode()->createChildSceneNode();
        camera_node->setPosition(16.0f, 18.0f, 26.0f);
        camera_node->lookAt(Ogre::Vector3(0.0f, 1.5f, 0.0f), Ogre::Node::TS_WORLD);
        camera_node->attachObject(camera);

        auto *viewport = window->addViewport(camera);
        viewport->setBackgroundColour(Ogre::ColourValue(0.03f, 0.04f, 0.05f));

        createMaterial("ecosim.viewer.neutral", Ogre::ColourValue(0.34f, 0.38f, 0.36f));
        createMaterial("ecosim.viewer.trajectory", Ogre::ColourValue(0.95f, 0.88f, 0.35f), false);
        createMaterial("ecosim.viewer.current", Ogre::ColourValue(1.0f, 0.22f, 0.16f), false);
        createMaterial("ecosim.viewer.prey", Ogre::ColourValue(0.25f, 0.72f, 0.48f));
        createMaterial("ecosim.viewer.predator", Ogre::ColourValue(0.78f, 0.26f, 0.24f));
        createMaterial("ecosim.viewer.axis_x", Ogre::ColourValue(0.8f, 0.18f, 0.18f), false);
        createMaterial("ecosim.viewer.axis_y", Ogre::ColourValue(0.22f, 0.78f, 0.32f), false);
        createMaterial("ecosim.viewer.axis_z", Ogre::ColourValue(0.22f, 0.38f, 0.9f), false);
        createMaterial("ecosim.viewer.shock", Ogre::ColourValue(1.0f, 0.92f, 0.18f), false);

        glv_node = scene->getRootSceneNode()->createChildSceneNode("EcoSimGlvNode");
        rm_node = scene->getRootSceneNode()->createChildSceneNode("EcoSimRmNode");
        shock_node = scene->getRootSceneNode()->createChildSceneNode("EcoSimShockNode");

        glv_surface = scene->createManualObject("EcoSimGlvSurface");
        glv_surface->setDynamic(true);
        glv_node->attachObject(glv_surface);

        rm_trajectory = scene->createManualObject("EcoSimRmTrajectory");
        rm_trajectory->setDynamic(true);
        rm_node->attachObject(rm_trajectory);

        rm_current = scene->createManualObject("EcoSimRmCurrent");
        rm_current->setDynamic(true);
        rm_node->attachObject(rm_current);

        rm_bars = scene->createManualObject("EcoSimRmBars");
        rm_bars->setDynamic(true);
        rm_node->attachObject(rm_bars);

        rm_axes = scene->createManualObject("EcoSimRmAxes");
        rm_axes->setDynamic(true);
        rm_node->attachObject(rm_axes);
        createRmAxes();

        shock_object = scene->createManualObject("EcoSimShockEffects");
        shock_object->setDynamic(true);
        shock_node->attachObject(shock_object);

        rm_node->setVisible(false);
    }

    void createRmAxes() {
        rm_axes->clear();
        rm_axes->begin("ecosim.viewer.axis_x", Ogre::RenderOperation::OT_LINE_LIST);
        addLine(*rm_axes, Ogre::Vector3(0.0f, 0.0f, 0.0f), Ogre::Vector3(12.0f, 0.0f, 0.0f));
        rm_axes->end();
        rm_axes->begin("ecosim.viewer.axis_y", Ogre::RenderOperation::OT_LINE_LIST);
        addLine(*rm_axes, Ogre::Vector3(0.0f, 0.0f, 0.0f), Ogre::Vector3(0.0f, 9.0f, 0.0f));
        rm_axes->end();
        rm_axes->begin("ecosim.viewer.axis_z", Ogre::RenderOperation::OT_LINE_LIST);
        addLine(*rm_axes, Ogre::Vector3(0.0f, 0.0f, 0.0f), Ogre::Vector3(0.0f, 0.0f, 12.0f));
        rm_axes->end();
    }

    void prepareScales(const std::vector<SimulationFrame> &input_frames) {
        double max_biomass = 0.0;
        double max_prey = 0.0;
        double max_predator = 0.0;
        double max_time = 0.0;

        for (const auto &frame : input_frames) {
            max_biomass = std::max(max_biomass, metricOrStateSum(frame, "biomass_total"));
            const auto values = preyPredatorValues(frame);
            max_prey = std::max(max_prey, std::max(0.0, values.first));
            max_predator = std::max(max_predator, std::max(0.0, values.second));
            max_time = std::max(max_time, frame.time > 0.0 ? frame.time : static_cast<double>(frame.tick));
        }

        glv_height_scale = max_biomass > 1e-9 ? 4.0 / max_biomass : 0.2;
        rm_scale_x = max_prey > 1e-9 ? 10.0 / max_prey : 0.2;
        rm_scale_z = max_predator > 1e-9 ? 10.0 / max_predator : 0.2;
        rm_scale_y = max_time > 1e-9 ? 8.0 / max_time : 0.05;
    }

    void run(const std::vector<SimulationFrame> &input_frames) {
        frames = &input_frames;
        prepareScales(input_frames);

        const auto frame_delay = std::chrono::milliseconds(100);
        auto last_render = std::chrono::steady_clock::now();

        for (current_frame_index = 0; current_frame_index < input_frames.size(); ++current_frame_index) {
            if (!window || window->isClosed()) {
                break;
            }

            updateFrame(input_frames[current_frame_index]);
            const auto frame_until = std::chrono::steady_clock::now() + frame_delay;
            while (std::chrono::steady_clock::now() < frame_until) {
                Ogre::WindowEventUtilities::messagePump();
                if (!window || window->isClosed()) {
                    break;
                }

                const auto now = std::chrono::steady_clock::now();
                const std::chrono::duration<double> elapsed = now - last_render;
                last_render = now;
                renderShockEffects(elapsed.count());

                if (!root->renderOneFrame()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        std::cout << std::endl;
    }

    void updateFrame(const SimulationFrame &frame) {
        if (isRmModel(frame)) {
            glv_node->setVisible(false);
            rm_node->setVisible(true);
            updateRosenzweigMacArthurView(frame);
        } else {
            rm_node->setVisible(false);
            glv_node->setVisible(true);
            updateGlvMap(frame);
        }

        updateShockEffects(frame);
        updateWindowInfo(frame);
    }

    void updateWindowInfo(const SimulationFrame &frame) {
        std::ostringstream out;
        out << "tick=" << frame.tick << " model=" << frame.model_id << " integrator=" << frame.integrator
            << " scenario=" << frame.scenario_id << " biomass_total=" << std::fixed << std::setprecision(3)
            << metricOrStateSum(frame, "biomass_total") << " checksum=" << frame.checksum;

        const auto text = out.str();
        if (window) {
            window->setDebugText(text);
        }
        std::cout << '\r' << text << "        " << std::flush;
    }

    void ensureSpeciesVisuals(const SimulationFrame &frame) {
        if (frame.species_names == last_species_names) {
            return;
        }

        last_species_names = frame.species_names;
        anchors.clear();
        species_materials.clear();

        const std::vector<Ogre::ColourValue> palette = {
            Ogre::ColourValue(0.25f, 0.68f, 0.43f),
            Ogre::ColourValue(0.88f, 0.58f, 0.20f),
            Ogre::ColourValue(0.36f, 0.55f, 0.92f),
            Ogre::ColourValue(0.78f, 0.28f, 0.32f),
            Ogre::ColourValue(0.45f, 0.74f, 0.82f),
            Ogre::ColourValue(0.70f, 0.46f, 0.78f),
            Ogre::ColourValue(0.80f, 0.80f, 0.32f),
            Ogre::ColourValue(0.48f, 0.58f, 0.36f),
        };

        const auto species_count = std::max<std::size_t>(1, frame.species_names.size());
        const float radius = kGridSize * kCellSpacing * 0.38f;
        for (std::size_t i = 0; i < frame.species_names.size(); ++i) {
            const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(species_count);
            anchors[frame.species_names[i]] = Ogre::Vector3(static_cast<float>(radius * std::cos(angle)),
                                                            0.0f,
                                                            static_cast<float>(radius * std::sin(angle)));

            const auto material_name = "ecosim.viewer.species." + std::to_string(i);
            species_materials.push_back(createMaterial(material_name, palette[i % palette.size()]));
        }
    }

    void updateGlvMap(const SimulationFrame &frame) {
        ensureSpeciesVisuals(frame);
        glv_surface->clear();

        const double sigma = 18.0;
        const float half = (static_cast<float>(kGridSize) - 1.0f) * kCellSpacing * 0.5f;

        for (int gx = 0; gx < kGridSize; ++gx) {
            for (int gz = 0; gz < kGridSize; ++gz) {
                const float center_x = static_cast<float>(gx) * kCellSpacing - half;
                const float center_z = static_cast<float>(gz) * kCellSpacing - half;
                const auto cell_pos = Ogre::Vector3(center_x, 0.0f, center_z);

                double total_density = 0.0;
                double best_contribution = -std::numeric_limits<double>::infinity();
                std::size_t dominant = frame.species_names.size();

                for (std::size_t i = 0; i < frame.species_names.size() && i < frame.state_values.size(); ++i) {
                    auto anchor = anchors.find(frame.species_names[i]);
                    if (anchor == anchors.end()) {
                        continue;
                    }

                    const auto delta = cell_pos - anchor->second;
                    const double distance_sq = delta.x * delta.x + delta.z * delta.z;
                    const double contribution = std::max(0.0, frame.state_values[i]) * std::exp(-distance_sq / sigma);
                    total_density += contribution;
                    if (contribution > best_contribution) {
                        best_contribution = contribution;
                        dominant = i;
                    }
                }

                const float height = static_cast<float>(std::min(5.0, 0.06 + total_density * glv_height_scale));
                const auto material = dominant < species_materials.size() ? species_materials[dominant]
                                                                          : std::string("ecosim.viewer.neutral");
                const float pad = kCellSpacing * 0.47f;
                glv_surface->begin(material, Ogre::RenderOperation::OT_TRIANGLE_LIST);
                addBox(*glv_surface, center_x - pad, center_x + pad, center_z - pad, center_z + pad, 0.0f, height);
                glv_surface->end();
            }
        }
    }

    void updateRosenzweigMacArthurView(const SimulationFrame &frame) {
        const auto values = preyPredatorValues(frame);
        const double time_value = frame.time > 0.0 ? frame.time : static_cast<double>(frame.tick);
        current_rm_point = Ogre::Vector3(static_cast<float>(values.first * rm_scale_x),
                                         static_cast<float>(time_value * rm_scale_y),
                                         static_cast<float>(values.second * rm_scale_z));

        rm_trajectory->clear();
        rm_trajectory->begin("ecosim.viewer.trajectory", Ogre::RenderOperation::OT_LINE_STRIP);
        for (std::size_t i = 0; frames && i <= current_frame_index && i < frames->size(); ++i) {
            const auto &past = (*frames)[i];
            if (!isRmModel(past)) {
                continue;
            }
            const auto past_values = preyPredatorValues(past);
            const double past_time = past.time > 0.0 ? past.time : static_cast<double>(past.tick);
            rm_trajectory->position(static_cast<float>(past_values.first * rm_scale_x),
                                    static_cast<float>(past_time * rm_scale_y),
                                    static_cast<float>(past_values.second * rm_scale_z));
        }
        rm_trajectory->end();

        rm_current->clear();
        rm_current->begin("ecosim.viewer.current", Ogre::RenderOperation::OT_TRIANGLE_LIST);
        addBox(*rm_current,
               current_rm_point.x - 0.18f,
               current_rm_point.x + 0.18f,
               current_rm_point.z - 0.18f,
               current_rm_point.z + 0.18f,
               current_rm_point.y - 0.18f,
               current_rm_point.y + 0.18f);
        rm_current->end();

        rm_bars->clear();
        rm_bars->begin("ecosim.viewer.prey", Ogre::RenderOperation::OT_TRIANGLE_LIST);
        addBox(*rm_bars, -2.5f, -1.7f, -1.0f, -0.2f, 0.0f, static_cast<float>(values.first * rm_scale_x));
        rm_bars->end();
        rm_bars->begin("ecosim.viewer.predator", Ogre::RenderOperation::OT_TRIANGLE_LIST);
        addBox(*rm_bars, -1.3f, -0.5f, -1.0f, -0.2f, 0.0f, static_cast<float>(values.second * rm_scale_z));
        rm_bars->end();
    }

    void updateShockEffects(const SimulationFrame &frame) {
        for (const auto &flag : frame.flags) {
            if (!utils::startsWith(flag, "shock.")) {
                continue;
            }

            const auto target = flag.substr(std::string("shock.").size());
            Ogre::Vector3 position = isRmModel(frame) ? current_rm_point : Ogre::Vector3::ZERO;
            auto anchor = anchors.find(target);
            if (anchor != anchors.end()) {
                position = anchor->second;
                position.y = 4.8f;
            } else if (!isRmModel(frame)) {
                position = Ogre::Vector3(0.0f, 4.8f, 0.0f);
            }

            shocks.push_back({position, 0.45, 0.45});
        }
    }

    void renderShockEffects(double elapsed_seconds) {
        for (auto &shock : shocks) {
            shock.remaining -= elapsed_seconds;
        }
        shocks.erase(std::remove_if(shocks.begin(),
                                    shocks.end(),
                                    [](const ShockEffect &shock) { return shock.remaining <= 0.0; }),
                     shocks.end());

        shock_object->clear();
        if (shocks.empty()) {
            return;
        }

        shock_object->begin("ecosim.viewer.shock", Ogre::RenderOperation::OT_LINE_LIST);
        for (const auto &shock : shocks) {
            const double progress = 1.0 - shock.remaining / shock.duration;
            const float radius = static_cast<float>(0.35 + progress * 1.8);
            const float y = shock.position.y + static_cast<float>(progress * 0.9);
            const int segments = 32;

            for (int i = 0; i < segments; ++i) {
                const double a0 = 2.0 * kPi * static_cast<double>(i) / segments;
                const double a1 = 2.0 * kPi * static_cast<double>(i + 1) / segments;
                const Ogre::Vector3 p0(shock.position.x + radius * static_cast<float>(std::cos(a0)),
                                       y,
                                       shock.position.z + radius * static_cast<float>(std::sin(a0)));
                const Ogre::Vector3 p1(shock.position.x + radius * static_cast<float>(std::cos(a1)),
                                       y,
                                       shock.position.z + radius * static_cast<float>(std::sin(a1)));
                addLine(*shock_object, p0, p1);
            }

            addLine(*shock_object,
                    Ogre::Vector3(shock.position.x, y - 0.8f, shock.position.z),
                    Ogre::Vector3(shock.position.x, y + 0.8f, shock.position.z));
        }
        shock_object->end();
    }
};

OgreEcosystemViewer::OgreEcosystemViewer() : impl_(std::make_unique<Impl>()) {}

OgreEcosystemViewer::~OgreEcosystemViewer() = default;

bool OgreEcosystemViewer::initialize() {
    return impl_->initialize();
}

void OgreEcosystemViewer::run(const std::vector<SimulationFrame> &frames) {
    impl_->run(frames);
}

void OgreEcosystemViewer::shutdown() {
    impl_->shutdown();
}

} // namespace ecosim::viewer
