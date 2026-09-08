#include "doctest.h"
#include "eRectangle.h"

// Tests for eRectangle class
// Purpose: Verify 2D rectangle operations

TEST_CASE("eRectangle default construction") {
    eRectangle rect;
    
    // Default rectangle should be empty or have zero size
    eCoord low = rect.GetLow();
    eCoord high = rect.GetHigh();
    
    // Check that coordinates are accessible
    CHECK(low.x == doctest::Approx(0.0f));
    CHECK(low.y == doctest::Approx(0.0f));
    CHECK(high.x == doctest::Approx(0.0f));
    CHECK(high.y == doctest::Approx(0.0f));
}

TEST_CASE("eRectangle construction with corners") {
    eCoord low(0.0f, 0.0f);
    eCoord high(10.0f, 20.0f);
    eRectangle rect(low, high);
    
    eCoord rectLow = rect.GetLow();
    eCoord rectHigh = rect.GetHigh();
    
    CHECK(rectLow.x == doctest::Approx(0.0f));
    CHECK(rectLow.y == doctest::Approx(0.0f));
    CHECK(rectHigh.x == doctest::Approx(10.0f));
    CHECK(rectHigh.y == doctest::Approx(20.0f));
}

TEST_CASE("eRectangle Clear method") {
    eCoord low(5.0f, 5.0f);
    eCoord high(15.0f, 15.0f);
    eRectangle rect(low, high);
    
    rect.Clear();
    
    eCoord clearedLow = rect.GetLow();
    eCoord clearedHigh = rect.GetHigh();
    
    // After clear, should be empty
    CHECK(clearedLow.x == doctest::Approx(0.0f));
    CHECK(clearedLow.y == doctest::Approx(0.0f));
    CHECK(clearedHigh.x == doctest::Approx(0.0f));
    CHECK(clearedHigh.y == doctest::Approx(0.0f));
}

TEST_CASE("eRectangle Include method") {
    eCoord low(0.0f, 0.0f);
    eCoord high(10.0f, 10.0f);
    eRectangle rect(low, high);
    
    // Include a point outside the current rectangle
    eCoord newPoint(15.0f, 15.0f);
    rect.Include(newPoint);
    
    eCoord rectLow = rect.GetLow();
    eCoord rectHigh = rect.GetHigh();
    
    // Rectangle should now include the new point
    CHECK(rectLow.x <= 0.0f);
    CHECK(rectLow.y <= 0.0f);
    CHECK(rectHigh.x >= 15.0f);
    CHECK(rectHigh.y >= 15.0f);
}

TEST_CASE("eRectangle Contains method") {
    eCoord low(0.0f, 0.0f);
    eCoord high(10.0f, 10.0f);
    eRectangle rect(low, high);
    
    // Point inside
    eCoord insidePoint(5.0f, 5.0f);
    CHECK(rect.Contains(insidePoint) == true);
    
    // Point on edge
    eCoord edgePoint(10.0f, 5.0f);
    CHECK(rect.Contains(edgePoint) == true);
    
    // Point outside
    eCoord outsidePoint(15.0f, 15.0f);
    CHECK(rect.Contains(outsidePoint) == false);
}

TEST_CASE("eRectangle Clamp method") {
    eCoord low(0.0f, 0.0f);
    eCoord high(10.0f, 10.0f);
    eRectangle rect(low, high);
    
    // Clamp a point inside - should remain unchanged
    eCoord insidePoint(5.0f, 5.0f);
    REAL dist1 = rect.Clamp(insidePoint);
    CHECK(insidePoint.x == doctest::Approx(5.0f));
    CHECK(insidePoint.y == doctest::Approx(5.0f));
    CHECK(dist1 == doctest::Approx(0.0f));
    
    // Clamp a point outside - should be moved to edge
    eCoord outsidePoint(15.0f, 15.0f);
    REAL dist2 = rect.Clamp(outsidePoint);
    CHECK(outsidePoint.x <= 10.0f);
    CHECK(outsidePoint.y <= 10.0f);
    CHECK(dist2 > 0.0f);
}

TEST_CASE("eRectangle GetPoint method") {
    eCoord low(0.0f, 0.0f);
    eCoord high(10.0f, 10.0f);
    eRectangle rect(low, high);
    
    // Get a point in the interior
    eCoord interiorPoint = rect.GetPoint(eCoord(5.0f, 5.0f));
    
    // Should be inside the rectangle
    CHECK(rect.Contains(interiorPoint) == true);
}

TEST_CASE("eRectangle Clip method") {
    eCoord low(0.0f, 0.0f);
    eCoord high(10.0f, 10.0f);
    eRectangle rect(low, high);
    
    eCoord start(0.0f, 0.0f);
    eCoord stop(20.0f, 20.0f);
    
    // Clip stop to lie inside the rectangle
    REAL dist = rect.Clip(start, stop);
    
    // Stop should be clamped to the rectangle boundary
    CHECK(stop.x <= 10.0f);
    CHECK(stop.y <= 10.0f);
}

TEST_CASE("eRectangle with negative coordinates") {
    eCoord low(-10.0f, -10.0f);
    eCoord high(10.0f, 10.0f);
    eRectangle rect(low, high);
    
    eCoord origin(0.0f, 0.0f);
    CHECK(rect.Contains(origin) == true);
}
