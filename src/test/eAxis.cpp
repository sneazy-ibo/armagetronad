#include "doctest.h"
#include "eAxis.h"

// Tests for eAxis class
// Purpose: Verify axis and winding operations

TEST_CASE("eAxis default construction") {
    eAxis axis;
    
    // Default axis should have 4 windings
    CHECK(axis.WindingNumber() == 4);
}

TEST_CASE("eAxis construction with winding number") {
    eAxis axis8(8);
    CHECK(axis8.WindingNumber() == 8);
    
    eAxis axis16(16);
    CHECK(axis16.WindingNumber() == 16);
}

TEST_CASE("eAxis GetDirection method") {
    eAxis axis(4);
    
    // Get direction for winding 0
    eCoord dir0 = axis.GetDirection(0);
    CHECK(dir0.x == doctest::Approx(1.0f));
    CHECK(dir0.y == doctest::Approx(0.0f));
    
    // Get direction for winding 1
    eCoord dir1 = axis.GetDirection(1);
    CHECK(dir1.x == doctest::Approx(0.0f));
    CHECK(dir1.y == doctest::Approx(1.0f));
    
    // Get direction for winding 2
    eCoord dir2 = axis.GetDirection(2);
    CHECK(dir2.x == doctest::Approx(-1.0f));
    CHECK(dir2.y == doctest::Approx(0.0f));
    
    // Get direction for winding 3
    eCoord dir3 = axis.GetDirection(3);
    CHECK(dir3.x == doctest::Approx(0.0f));
    CHECK(dir3.y == doctest::Approx(-1.0f));
}

TEST_CASE("eAxis NearestWinding method") {
    eAxis axis(4);
    
    // Test point along positive x axis
    eCoord posX(1.0f, 0.0f);
    int nearest0 = axis.NearestWinding(posX);
    CHECK(nearest0 == 0);
    
    // Test point along positive y axis
    eCoord posY(0.0f, 1.0f);
    int nearest1 = axis.NearestWinding(posY);
    CHECK(nearest1 == 1);
    
    // Test point along negative x axis
    eCoord negX(-1.0f, 0.0f);
    int nearest2 = axis.NearestWinding(negX);
    CHECK(nearest2 == 2);
    
    // Test point along negative y axis
    eCoord negY(0.0f, -1.0f);
    int nearest3 = axis.NearestWinding(negY);
    CHECK(nearest3 == 3);
}

TEST_CASE("eAxis Turn method") {
    eAxis axis(4);
    
    // Test turning from winding 0
    int current = 0;
    axis.Turn(current, 1); // Turn right
    CHECK(current == 1);
    
    axis.Turn(current, 1); // Turn right again
    CHECK(current == 2);
    
    axis.Turn(current, -1); // Turn left
    CHECK(current == 1);
}

TEST_CASE("eAxis TurnRight method") {
    eAxis axis(4);
    
    int direction = 0;
    axis.TurnRight(direction);
    CHECK(direction == 1);
    
    axis.TurnRight(direction);
    CHECK(direction == 2);
}

TEST_CASE("eAxis TurnLeft method") {
    eAxis axis(4);
    
    int direction = 2;
    axis.TurnLeft(direction);
    CHECK(direction == 1);
    
    axis.TurnLeft(direction);
    CHECK(direction == 0);
}

TEST_CASE("eAxis with different winding numbers") {
    eAxis axis3(3);
    CHECK(axis3.WindingNumber() == 3);
    
    eAxis axis6(6);
    CHECK(axis6.WindingNumber() == 6);
    
    // Test GetDirection with different winding numbers
    eCoord dir = axis6.GetDirection(0);
    CHECK(dir.x == doctest::Approx(1.0f));
    CHECK(dir.y == doctest::Approx(0.0f));
}
