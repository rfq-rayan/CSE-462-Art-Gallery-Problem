/**
 * @file step_recorder.cpp
 * @brief Implementation of step recording for visualization
 */

#include "core/utils/step_recorder.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>

namespace agp {
namespace utils {

StepRecorder::StepRecorder(const std::string& output_file)
    : output_file_(output_file)
    , enabled_(true) {
}

void StepRecorder::startStep(StepType type) {
    steps_.push_back(VisualizationStep());
    current_step_ = &steps_.back();
    current_step_->type = type;
    current_step_->step_id = static_cast<int>(steps_.size()) - 1;

    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch() - start_time_);
    current_step_->timestamp_ms = static_cast<double>(duration.count());
}

void StepRecorder::endStep() {
    current_step_ = nullptr;
}

void StepRecorder::addGuard(double x, double y, GuardType type, int id) {
    if (current_step_) {
        current_step_->guards.push_back({x, y, type, id});
    }
}

void StepRecorder::addWitness(double x, double y, WitnessStatus status, int id) {
    if (current_step_) {
        current_step_->witnesses.push_back({x, y, status, id});
    }
}

void StepRecorder::addFace(const std::vector<std::pair<double, double>>& vertices, int id, bool splittable) {
    if (current_step_) {
        current_step_->faces.push_back({vertices, id, splittable});
    }
}

void StepRecorder::addChord(double x1, double y1, double x2, double y2) {
    if (current_step_) {
        current_step_->chords.push_back({x1, y1, x2, y2});
    }
}

void StepRecorder::setIteration(int iteration) {
    if (current_step_) {
        current_step_->iteration = iteration;
    }
}

void StepRecorder::setGranularity(int k) {
    if (current_step_) {
        current_step_->granularity_k = k;
    }
}

void StepRecorder::setObjectiveValue(double value) {
    if (current_step_) {
        current_step_->objective_value = value;
    }
}

void StepRecorder::setStatusMessage(const std::string& message) {
    if (current_step_) {
        current_step_->status_message = message;
    }
}

void StepRecorder::setSplitInfo(const std::string& split_type, int face_id) {
    if (current_step_) {
        current_step_->split_type = split_type;
        current_step_->split_face_id = face_id;
    }
}

void StepRecorder::setPolygonVertices(const std::vector<std::pair<double, double>>& vertices) {
    if (current_step_) {
        current_step_->polygon_vertices = vertices;
    }
}

std::string StepRecorder::toJson() const {
    nlohmann::json j;

    j["steps"] = nlohmann::json::array();
    for (const auto& step : steps_) {
        nlohmann::json step_j;
        step_j["step_id"] = step.step_id;
        step_j["type"] = static_cast<int>(step.type);
        step_j["iteration"] = step.iteration;
        step_j["granularity_k"] = step.granularity_k;
        step_j["objective_value"] = step.objective_value;
        step_j["status_message"] = step.status_message;
        step_j["timestamp_ms"] = step.timestamp_ms;

        // Guards
        step_j["guards"] = nlohmann::json::array();
        for (const auto& g : step.guards) {
            step_j["guards"].push_back({
                {"x", g.x},
                {"y", g.y},
                {"type", static_cast<int>(g.type)},
                {"original_id", g.original_id}
            });
        }

        // Witnesses
        step_j["witnesses"] = nlohmann::json::array();
        for (const auto& w : step.witnesses) {
            step_j["witnesses"].push_back({
                {"x", w.x},
                {"y", w.y},
                {"status", static_cast<int>(w.status)},
                {"original_id", w.original_id}
            });
        }

        // Candidates
        step_j["candidates"] = nlohmann::json::array();
        for (const auto& c : step.candidates) {
            step_j["candidates"].push_back({c.first, c.second});
        }

        // Faces
        step_j["faces"] = nlohmann::json::array();
        for (const auto& f : step.faces) {
            nlohmann::json face_j;
            face_j["id"] = f.id;
            face_j["splittable"] = f.is_splittable;
            face_j["vertices"] = nlohmann::json::array();
            for (const auto& v : f.vertices) {
                face_j["vertices"].push_back({v.first, v.second});
            }
            step_j["faces"].push_back(face_j);
        }

        // Chords
        step_j["chords"] = nlohmann::json::array();
        for (const auto& c : step.chords) {
            step_j["chords"].push_back({
                {"x1", c.x1},
                {"y1", c.y1},
                {"x2", c.x2},
                {"y2", c.y2}
            });
        }

        // Polygon vertices
        step_j["polygon_vertices"] = nlohmann::json::array();
        for (const auto& v : step.polygon_vertices) {
            step_j["polygon_vertices"].push_back({v.first, v.second});
        }

        // Split info
        if (!step.split_type.empty()) {
            step_j["split_type"] = step.split_type;
            step_j["split_face_id"] = step.split_face_id;
        }

        j["steps"].push_back(step_j);
    }

    return j.dump(2);
}

void StepRecorder::saveToFile(const std::string& filename) const {
    std::string file = filename.empty() ? output_file_ : filename;
    std::ofstream out(file);
    if (out.is_open()) {
        out << toJson();
        out.close();
    }
}

size_t StepRecorder::size() const {
    return steps_.size();
}

bool StepRecorder::isEnabled() const {
    return enabled_;
}

} // namespace utils
} // namespace agp
