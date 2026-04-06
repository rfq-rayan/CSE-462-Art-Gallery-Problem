/**
 * @file test_visibility.cpp
 * @brief Unit tests for visibility computations
 */

#include <gtest/gtest.h>
#include "core/geometry/visibility.hpp"
#include "core/geometry/polygon.hpp"
#include "core/geometry/point.hpp"

using namespace agp::geometry;
using KernelE = CGAL::Exact_predicates_exact_constructions_kernel;

// ============================================================
// Fixtures
// ============================================================

class ConvexVisibilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::vector<PointE> vs = {
            PointE(0, 0), PointE(10, 0), PointE(10, 10), PointE(0, 10)
        };
        square_ = PolygonE(vs);
    }
    PolygonE square_;
};

class LShapeVisibilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // L-shape: lower-left 10x5 + upper-left 5x5
        // Reflex vertex at (5,5)
        std::vector<PointE> vs = {
            PointE(0, 0), PointE(10, 0), PointE(10, 5),
            PointE(5,  5), PointE(5, 10), PointE(0, 10)
        };
        l_shape_ = PolygonE(vs);
    }
    PolygonE l_shape_;
};

// ============================================================
// Point visibility in a convex polygon
// ============================================================

TEST_F(ConvexVisibilityTest, VisibleAcrossDiagonal) {
    PointE a(1, 1);
    PointE b(9, 9);
    EXPECT_TRUE(VisibilityE::is_visible(square_, a, b));
}

TEST_F(ConvexVisibilityTest, VisibleCornersConvex) {
    // In a convex polygon every pair of interior points is visible
    PointE a(1, 1);
    PointE b(9, 1);
    EXPECT_TRUE(VisibilityE::is_visible(square_, a, b));
}

// ============================================================
// Point visibility in an L-shaped polygon
// ============================================================

TEST_F(LShapeVisibilityTest, VisibleWithinSameLeg) {
    // Both points in lower horizontal leg
    PointE a(2, 2);
    PointE b(8, 2);
    EXPECT_TRUE(VisibilityE::is_visible(l_shape_, a, b));
}

TEST_F(LShapeVisibilityTest, BlockedByReflexVertex) {
    // Upper-right area is blocked from lower-right area
    PointE upper_left(2, 8);  // upper-left leg
    PointE lower_right(9, 2); // lower-right leg
    EXPECT_FALSE(VisibilityE::is_visible(l_shape_, upper_left, lower_right));
}

// ============================================================
// Visibility polygon
// ============================================================

TEST_F(ConvexVisibilityTest, VisibilityPolygonCentre) {
    PointE centre(5, 5);
    PolygonE vis = VisibilityE::compute_visibility_polygon(square_, centre);
    EXPECT_GE(vis.num_vertices(), 3u);
    // From centre, every corner should be visible -> vis == square
    EXPECT_DOUBLE_EQ(CGAL::to_double(vis.area()),
                     CGAL::to_double(square_.area()));
}

TEST_F(LShapeVisibilityTest, VisibilityPolygonReducedFromLowerRight) {
    // From lower-right area, the upper-left leg is not fully visible
    PointE observer(9, 2);
    PolygonE vis = VisibilityE::compute_visibility_polygon(l_shape_, observer);
    EXPECT_GE(vis.num_vertices(), 3u);
    // Visibility area < full polygon area
    EXPECT_LT(CGAL::to_double(vis.area()),
              CGAL::to_double(l_shape_.area()));
}

// ============================================================
// sees_completely
// ============================================================

TEST_F(ConvexVisibilityTest, SeesCompletelyFromCentre) {
    // Centre point sees the whole polygon, so it sees any face
    PointE centre(5, 5);
    // Face = entire square
    PolygonE face = square_;
    EXPECT_TRUE(VisibilityE::sees_completely(square_, centre, face));
}

// ============================================================
// Enhanced visibility is a superset of regular visibility
// ============================================================

TEST_F(LShapeVisibilityTest, EnhancedVisibilityIsSuperset) {
    PointE guard(9, 2);
    double delta = 0.5;
    PolygonE vis     = VisibilityE::compute_visibility_polygon(l_shape_, guard);
    PolygonE vis_enh = VisibilityE::compute_enhanced_visibility(l_shape_, guard, KernelE::FT(delta));

    // Enhanced area >= plain visibility area
    EXPECT_GE(CGAL::to_double(vis_enh.area()),
              CGAL::to_double(vis.area()) - 1e-6); // small tolerance for numerical noise
}

// ============================================================
// Diminished visibility is a subset of regular visibility
// ============================================================

TEST_F(LShapeVisibilityTest, DiminishedVisibilityIsSubset) {
    PointE guard(9, 2);
    double delta = 0.5;
    PolygonE vis     = VisibilityE::compute_visibility_polygon(l_shape_, guard);
    PolygonE vis_dim = VisibilityE::compute_diminished_visibility(l_shape_, guard, KernelE::FT(delta));

    // Diminished area <= plain visibility area
    EXPECT_LE(CGAL::to_double(vis_dim.area()),
              CGAL::to_double(vis.area()) + 1e-6);
}

