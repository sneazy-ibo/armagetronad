#include "doctest.h"
#include "eRectangle.h"

// Tests for eRectangle class
// Purpose: Verify 2D rectangle operations

DOCTEST_TEST_SUITE("eRectangle")
{
    TEST_CASE("eRectangle default construction")
    {
        DOCTEST_GIVEN("Setup")
        {
            eRectangle rect;

            // Default rectangle is invalid (low > high)
            eCoord low = rect.GetLow();
            eCoord high = rect.GetHigh();

            // Check that coordinates are accessible and represent an empty/invalid rectangle
            CHECK(low.x == doctest::Approx(1E+30f));
            CHECK(low.y == doctest::Approx(1E+30f));
            CHECK(high.x == doctest::Approx(-1E+30f));
            CHECK(high.y == doctest::Approx(-1E+30f));
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle construction with corners")
    {
        DOCTEST_GIVEN("Setup")
        {
            eCoord low(0.0f, 0.0f);
            eCoord high(10.0f, 20.0f);
            eRectangle rect(low, high);

            eCoord rectLow = rect.GetLow();
            eCoord rectHigh = rect.GetHigh();

            CHECK(rectLow.x == doctest::Approx(0.0f));
            CHECK(rectLow.y == doctest::Approx(0.0f));
            CHECK(rectHigh.x == doctest::Approx(10.0f));
            CHECK(rectHigh.y == doctest::Approx(20.0f));
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle Clear method")
    {
        DOCTEST_GIVEN("Setup")
        {
            eCoord low(5.0f, 5.0f);
            eCoord high(15.0f, 15.0f);
            eRectangle rect(low, high);

            rect.Clear();

            eCoord clearedLow = rect.GetLow();
            eCoord clearedHigh = rect.GetHigh();

            // After clear, rectangle is invalid (low > high)
            // Clear sets to: low_(1E+30, 1E+30), high_(-1E+30, -1E+30)
            CHECK(clearedLow.x == doctest::Approx(1E+30f));
            CHECK(clearedLow.y == doctest::Approx(1E+30f));
            CHECK(clearedHigh.x == doctest::Approx(-1E+30f));
            CHECK(clearedHigh.y == doctest::Approx(-1E+30f));
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle Include method")
    {
        DOCTEST_GIVEN("Setup")
        {
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
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle Contains method")
    {
        DOCTEST_GIVEN("Setup")
        {
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
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle Clamp method")
    {
        DOCTEST_GIVEN("Setup")
        {
            eCoord low(0.0f, 0.0f);
            eCoord high(10.0f, 10.0f);
            eRectangle rect(low, high);

            // Clamp a point inside - should remain unchanged
            eCoord insidePoint(5.0f, 5.0f);
            rect.Clamp(insidePoint);
            CHECK(insidePoint.x == doctest::Approx(5.0f));
            CHECK(insidePoint.y == doctest::Approx(5.0f));
            // Clamp returns the maximum movement; for inside points this can be negative
            // Just verify the point wasn't moved

            // Clamp a point outside - should be moved to edge
            eCoord outsidePoint(15.0f, 15.0f);
            REAL dist2 = rect.Clamp(outsidePoint);
            CHECK(outsidePoint.x <= 10.0f);
            CHECK(outsidePoint.y <= 10.0f);
            CHECK(dist2 > 0.0f);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle GetPoint method")
    {
        DOCTEST_GIVEN("Setup")
        {
            eCoord low(0.0f, 0.0f);
            eCoord high(10.0f, 10.0f);
            eRectangle rect(low, high);

            // Get a point in the interior using normalized coordinates [0,1]
            eCoord interiorPoint = rect.GetPoint(eCoord(0.5f, 0.5f));

            // Should be inside the rectangle (at center)
            CHECK(rect.Contains(interiorPoint) == true);
            CHECK(interiorPoint.x == doctest::Approx(5.0f));
            CHECK(interiorPoint.y == doctest::Approx(5.0f));
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle Clip method")
    {
        DOCTEST_GIVEN("Setup")
        {
            eCoord low(0.0f, 0.0f);
            eCoord high(10.0f, 10.0f);
            eRectangle rect(low, high);

            eCoord start(0.0f, 0.0f);
            eCoord stop(20.0f, 20.0f);

            // Clip stop to lie inside the rectangle
            rect.Clip(start, stop);

            // Stop should be clamped to the rectangle boundary
            CHECK(stop.x <= 10.0f);
            CHECK(stop.y <= 10.0f);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("eRectangle with negative coordinates")
    {
        DOCTEST_GIVEN("Setup")
        {
            eCoord low(-10.0f, -10.0f);
            eCoord high(10.0f, 10.0f);
            eRectangle rect(low, high);

            eCoord origin(0.0f, 0.0f);
            CHECK(rect.Contains(origin) == true);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }
}
