/**
 * @file test_geometry.cpp
 * @brief Unit tests for core geometry primitives
 */

#include <gtest/gtest.h>
#include "core/geometry/point.hpp"
#include "core/geometry/segment.hpp"
#include "core/geometry/polygon.hpp"

using namespace agp::geometry;
using KernelE = CGAL::Exact_predicates_exact_constructions_kernel;

// ============================================================
// Point tests
// ============================================================

TEST(PointTest, Construction) {
    PointE p(1.0, 2.0);
    EXPECT_DOUBLE_EQ(p.x_double(), 1.0);
    EXPECT_DOUBLE_EQ(p.y_double(), 2.0);
}

TEST(PointTest, Distance) {
    PointE p1(0, 0);
    PointE p2(3, 4);
    EXPECT_DOUBLE_EQ(p1.distance(p2), 5.0);
}

TEST(PointTest, Equality) {
    PointE p1(1, 2);
    PointE p2(1, 2);
    PointE p3(3, 4);
    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);
}

// ============================================================
// Segment tests
// ============================================================

TEST(SegmentTest, Length) {
    PointE p1(0, 0);
    PointE p2(10, 0);
    SegmentE seg(p1, p2);
    EXPECT_DOUBLE_EQ(seg.length(), 10.0);
}

TEST(SegmentTest, Midpoint) {
    PointE p1(0, 0);
    PointE p2(4, 4);
    SegmentE seg(p1, p2);
    PointE mid = seg.midpoint();
    EXPECT_DOUBLE_EQ(mid.x_double(), 2.0);
    EXPECT_DOUBLE_EQ(mid.y_double(), 2.0);
}

// ============================================================
// Square polygon tests
// ============================================================

class SquarePolygonTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::vector<PointE> vertices = {
            PointE(0, 0), PointE(10, 0), PointE(10, 10), PointE(0, 10)
        };
        square_ = PolygonE(vertices);
    }
    PolygonE square_;
};

TEST_F(SquarePolygonTest, NumVertices) {
    EXPECT_EQ(square_.num_vertices(), 4u);
}

TEST_F(SquarePolygonTest, NoReflexVertices) {
    EXPECT_EQ(square_.num_reflex_vertices(), 0u);
}

TEST_F(SquarePolygonTest, ContainsInside) {
    EXPECT_TRUE(square_.contains(PointE(5, 5)));
}

TEST_F(SquarePolygonTest, DoesNotContainOutside) {
    EXPECT_FALSE(square_.contains(PointE(15, 5)));
}

TEST_F(SquarePolygonTest, ContainsBoundary) {
    EXPECT_TRUE(square_.is_on_boundary(PointE(0, 5)));
}

TEST_F(SquarePolygonTest, Area) {
    auto area = CGAL::to_double(square_.area());
    EXPECT_DOUBLE_EQ(area, 100.0);
}

// ============================================================
// L-shaped polygon tests
// ============================================================

class LShapePolygonTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::vector<PointE> vertices = {
            PointE(0, 0), PointE(10, 0), PointE(10, 5),
            PointE(5,  5), PointE(5, 10), PointE(0, 10)
        };
        l_shape_ = PolygonE(vertices);
    }
    PolygonE l_shape_;
};

TEST_F(LShapePolygonTest, NumVertices) {
    EXPECT_EQ(l_shape_.num_vertices(), 6u);
}

TEST_F(LShapePolygonTest, OneReflexVertex) {
    EXPECT_EQ(l_shape_.num_reflex_vertices(), 1u);
}

TEST_F(LShapePolygonTest, ContainsInside) {
    EXPECT_TRUE(l_shape_.contains(PointE(2, 2)));
    EXPECT_TRUE(l_shape_.contains(PointE(2, 8)));
}

TEST_F(LShapePolygonTest, DoesNotContainMissingCorner) {
    // Point in the missing upper-right rectangle that completes the square
    EXPECT_FALSE(l_shape_.contains(PointE(8, 8)));
}

