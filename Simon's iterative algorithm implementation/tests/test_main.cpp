/**
 * @file test_geometry.cpp
 * @brief Basic geometry tests
 */

#include <gtest/gtest.h>
#include "core/geometry/point.hpp"
#include "core/geometry/segment.hpp"
#include "core/geometry/polygon.hpp"

using namespace agp::geometry;

class GeometryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple square polygon via constructor (Polygon has no add_vertex)
        std::vector<PointE> vs = {
            PointE(0, 0), PointE(10, 0), PointE(10, 10), PointE(0, 10)
        };
        square_ = PolygonE(vs);
    }

    PolygonE square_;
};

TEST_F(GeometryTest, PointConstruction) {
    PointE p1(1.0, 2.0);
    EXPECT_DOUBLE_EQ(p1.x_double(), 1.0);
    EXPECT_DOUBLE_EQ(p1.y_double(), 2.0);

    PointE p2(3.5, 7.5);
    EXPECT_DOUBLE_EQ(p2.x_double(), 3.5);
    EXPECT_DOUBLE_EQ(p2.y_double(), 7.5);
}

TEST_F(GeometryTest, PointDistance) {
    PointE p1(0, 0);
    PointE p2(3, 4);

    EXPECT_DOUBLE_EQ(p1.distance(p2), 5.0);
}

TEST_F(GeometryTest, SegmentConstruction) {
    PointE p1(0, 0);
    PointE p2(10, 0);
    SegmentE seg(p1, p2);

    EXPECT_DOUBLE_EQ(seg.length(), 10.0);
}

TEST_F(GeometryTest, PolygonVertices) {
    EXPECT_EQ(square_.num_vertices(), 4);
    EXPECT_EQ(square_.num_reflex_vertices(), 0); // Square has no reflex vertices
}

TEST_F(GeometryTest, PolygonContains) {
    // Inside point
    PointE inside(5, 5);
    EXPECT_TRUE(square_.contains(inside));

    // Outside point
    PointE outside(15, 5);
    EXPECT_FALSE(square_.contains(outside));

    // Boundary point
    PointE boundary(0, 5);
    EXPECT_TRUE(square_.contains(boundary));
}

TEST_F(GeometryTest, PolygonArea) {
    auto area = CGAL::to_double(square_.area());
    EXPECT_DOUBLE_EQ(area, 100.0);
}

TEST_F(GeometryTest, ReflexVertexDetection) {
    // L-shaped polygon has one reflex vertex
    std::vector<PointE> vs = {
        PointE(0, 0), PointE(10, 0), PointE(10, 5),
        PointE(5, 5), PointE(5, 10), PointE(0, 10)
    };
    PolygonE l_shaped(vs);

    EXPECT_EQ(l_shaped.num_vertices(), 6);
    EXPECT_EQ(l_shaped.num_reflex_vertices(), 1);
}

