/**
 * @file step_recorder.hpp
 * @brief Step recording utility for step-by-step visualization
 *
 * Records algorithm steps for visualization and debugging.
 */

#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <chrono>

namespace agp {
namespace utils {

/**
 * @brief Type of algorithm step being recorded
 */
enum class StepType {
    INIT,                   ///< Initial state before algorithm starts
    IP_SOLVE_STAGE_1,       ///< After first IP solve (vertex guards only)
    IP_SOLVE_STAGE_2,       ///< After second IP solve (with face guards)
    CRITICAL_WITNESS_UPDATE,///< Critical witness added/updated
    SPLIT,                  ///< Face split operation
    GRANULARITY_UPDATE,     ///< Granularity parameter changed
    TERMINATE               ///< Algorithm termination
};

/**
 * @brief Type of guard (vertex or face-based)
 */
enum class GuardType {
    VERTEX_GUARD,  ///< Guard placed at a vertex
    FACE_GUARD     ///< Guard placed within a face
};

/**
 * @brief Status of a witness point
 */
enum class WitnessStatus {
    SEEN,     ///< Witness is covered by a guard
    UNSEEN,   ///< Witness is not yet covered
    CRITICAL  ///< Witness is critical (uncovered and needs attention)
};

/**
 * @brief Data for a single guard
 */
struct GuardData {
    double x, y;              ///< Position
    GuardType type;           ///< Guard type
    int original_id;          ///< Index in candidates vector
};

/**
 * @brief Data for a single witness point
 */
struct WitnessData {
    double x, y;              ///< Position
    WitnessStatus status;     ///< Coverage status
    int original_id;          ///< Index in witnesses vector
};

/**
 * @brief Data for a face in the arrangement
 */
struct FaceData {
    std::vector<std::pair<double, double>> vertices;  ///< Face boundary vertices
    int id;                    ///< Face identifier
    bool is_splittable;        ///< Whether face can be split
};

/**
 * @brief Data for a chord (line segment)
 */
struct ChordData {
    double x1, y1;  ///< Start point
    double x2, y2;  ///< End point
};

/**
 * @brief Single visualization step capturing algorithm state
 */
struct VisualizationStep {
    int step_id = 0;                  ///< Unique step identifier
    StepType type = StepType::INIT;   ///< Step type

    // Geometry state
    std::vector<std::pair<double, double>> polygon_vertices;  ///< Original polygon
    std::vector<FaceData> faces;           ///< Arrangement faces
    std::vector<ChordData> chords;         ///< Chords in arrangement

    // Solution state
    std::vector<GuardData> guards;        ///< Selected guards
    std::vector<WitnessData> witnesses;   ///< Witness points
    std::vector<std::pair<double, double>> candidates;  ///< All candidate positions

    // Algorithm state
    int iteration = 0;                ///< Current iteration number
    int granularity_k = 0;            ///< Granularity parameter (2^-k)
    double objective_value = 0.0;     ///< Current IP objective value
    std::string status_message;       ///< Human-readable status

    // For split operations
    std::string split_type;           ///< Type of split performed
    int split_face_id = -1;           ///< Face that was split

    // Timing
    double timestamp_ms = 0.0;        ///< Timestamp in milliseconds

    VisualizationStep() = default;
    explicit VisualizationStep(StepType t) : type(t) {}
};

/**
 * @brief Records algorithm steps for visualization
 *
 * This class captures the state of the iterative algorithm at each
 * significant step, allowing for step-by-step visualization and
 * debugging of the solution process.
 */
class StepRecorder {
public:
    /**
     * @brief Construct a step recorder
     * @param output_file Optional file path for auto-saving
     */
    explicit StepRecorder(const std::string& output_file = "");

    /**
     * @brief Start recording a new step
     * @param type The type of step being recorded
     */
    void startStep(StepType type);

    /**
     * @brief Complete the current step and add to recording
     */
    void endStep();

    /**
     * @brief Add a guard to the current step
     */
    void addGuard(double x, double y, GuardType type, int id);

    /**
     * @brief Add a witness to the current step
     */
    void addWitness(double x, double y, WitnessStatus status, int id);

    /**
     * @brief Add a face to the current step
     */
    void addFace(const std::vector<std::pair<double, double>>& vertices,
                 int id, bool splittable);

    /**
     * @brief Add a chord to the current step
     */
    void addChord(double x1, double y1, double x2, double y2);

    /**
     * @brief Set iteration number for current step
     */
    void setIteration(int iter);

    /**
     * @brief Set granularity for current step
     */
    void setGranularity(int k);

    /**
     * @brief Set objective value for current step
     */
    void setObjectiveValue(double value);

    /**
     * @brief Set status message for current step
     */
    void setStatusMessage(const std::string& message);

    /**
     * @brief Set polygon vertices for current step
     */
    void setPolygonVertices(const std::vector<std::pair<double, double>>& vertices);

    /**
     * @brief Set split information for current step
     */
    void setSplitInfo(const std::string& split_type, int face_id);

    /**
     * @brief Add a candidate position
     */
    void addCandidate(double x, double y);

    /**
     * @brief Convert all recorded steps to JSON string
     */
    std::string toJson() const;

    /**
     * @brief Save recorded steps to a file
     * @param filename Output file path (uses output_file_ if empty)
     */
    void saveToFile(const std::string& filename = "") const;

    /**
     * @brief Get number of recorded steps
     */
    size_t size() const { return steps_.size(); }

    /**
     * @brief Check if recording is enabled
     */
    bool isEnabled() const { return enabled_; }

    /**
     * @brief Enable or disable recording
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

private:
    std::vector<VisualizationStep> steps_;  ///< All recorded steps
    VisualizationStep current_step_;        ///< Step being built
    bool has_current_step_ = false;         ///< Whether current_step_ is active
    std::string output_file_;               ///< Default output file
    bool enabled_ = true;                   ///< Recording enabled flag
};

} // namespace utils
} // namespace agp
