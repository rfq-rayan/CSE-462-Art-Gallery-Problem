/**
 * @file test_arrangement.cpp
 * @brief Unit tests for Arrangement and face operations
 */

#include <gtest/gtest.h>
#include "core/geometry/arrangement.hpp"
#include "core/geometry/polygon.hpp"
#include "core/geometry/point.hpp"

using namespace agp::geometry;
using KernelE = CGAL::Exact_predicates_exact_constructions_kernel;

// ============================================================
// Fixtures
// ============================================================

class SquareArrangementTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::vector<PointE> vs = {
            PointE(0, 0), PointE(10, 0), PointE(10, 10), PointE(0, 10)
        };
        sq_  = PolygonE(vs);
        arr_ = ArrangementE(sq_);
    }
    PolygonE      sq_;
    ArrangementE  arr_;
};

class LShapeArrangementTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::vector<PointE> vs = {
            PointE(0, 0), PointE(10, 0), PointE(10, 5),
            PointE(5,  5), PointE(5, 10), PointE(0, 10)
        };
        ls_  = PolygonE(vs);
        arr_ = ArrangementE(ls_);
    }
    PolygonE     ls_;
    ArrangementE arr_;
};

// ============================================================
// Initial construction
// ============================================================

TEST_F(SquareArrangementTest, HasAtLeastOneFace) {
    auto faces = arr_.all_faces();
    // At minimum the bounded face created by the 4 polygon edges
    EXPECT_GE(faces.size(), 1u);
}

TEST_F(LShapeArrangementTest, HasAtLeastOneFace) {
    auto faces = arr_.all_faces();
    EXPECT_GE(faces.size(), 1u);
}

// ============================================================
// insert_segment increases face count
// ============================================================

TEST_F(SquareArrangementTest, InsertSegmentIncreasesFaces) {
    size_t before = arr_.all_faces().size();
    // Horizontal diagonal
    arr_.insert_segment(SegmentE(PointE(0, 5), PointE(10, 5)));
    size_t after = arr_.all_faces().size();
    EXPECT_GT(after, before);
}

// ============================================================
// Centroid is inside its face
// ============================================================

TEST_F(SquareArrangementTest, CentroidInsideFace) {
    auto faces = arr_.all_faces();
    ASSERT_FALSE(faces.empty());
    for (auto f : faces) {
        PointE c = arr_.compute_face_centroid(f);
        // Centroid must lie inside the polygon
        EXPECT_TRUE(sq_.contains(c) || sq_.is_on_boundary(c));
    }
}

TEST_F(LShapeArrangementTest, CentroidInsideFace) {
    auto faces = arr_.all_faces();
    ASSERT_FALSE(faces.empty());
    for (auto f : faces) {
        PointE c = arr_.compute_face_centroid(f);
        EXPECT_TRUE(ls_.contains(c) || ls_.is_on_boundary(c));
    }
}

// ============================================================
// square_split
// ============================================================

TEST_F(SquareArrangementTest, SquareSplitProducesFaces) {
    size_t before = arr_.all_faces().size();
    auto faces = arr_.all_faces();
    if (!faces.empty()) {
        arr_.square_split(faces[0]);
    }
    size_t after = arr_.all_faces().size();
    EXPECT_GE(after, before);
}

