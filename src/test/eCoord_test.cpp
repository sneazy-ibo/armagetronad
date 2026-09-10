#include "doctest.h"
#include "eCoord.h"

// Tests for eCoord class
// Purpose: Document the status quo behavior and detect regressions

DOCTEST_TEST_SUITE("eCoord")
{
    TEST_CASE("eCoord construction and default values")
    {
        DOCTEST_GIVEN("a default eCoord")
        {
            eCoord defaultCoord;
            DOCTEST_THEN("its coordinates are zero")
            {
                CHECK(defaultCoord.x == 0);
                CHECK(defaultCoord.y == 0);
            }
        }

        DOCTEST_GIVEN("an eCoord with explicit coordinates")
        {
            eCoord explicitCoord(1.0f, 2.0f);
            DOCTEST_THEN("the coordinates match the constructor arguments")
            {
                CHECK(explicitCoord.x == 1.0f);
                CHECK(explicitCoord.y == 2.0f);
            }
        }

        DOCTEST_GIVEN("an eCoord with a single argument")
        {
            eCoord singleArg(5.0f);
            DOCTEST_THEN("x is set and y defaults to zero")
            {
                CHECK(singleArg.x == 5.0f);
                CHECK(singleArg.y == 0);
            }
        }
    }

    TEST_CASE("eCoord copy for copy construction and assignment")
    {
        DOCTEST_GIVEN("an eCoord for copy construction and assignment")
        {
            eCoord original(3.0f, 4.0f);
            eCoord copy = original;
            DOCTEST_THEN("the copy has the same coordinates as the original")
            {
                CHECK(copy.x == original.x);
                CHECK(copy.y == original.y);
            }

            eCoord assigned(0, 0);
            assigned = original;
            DOCTEST_THEN("the assigned eCoord has the same coordinates as the original")
            {
                CHECK(assigned.x == original.x);
                CHECK(assigned.y == original.y);
            }
        }
    }

    TEST_CASE("eCoord arithmetic operators")
    {
        DOCTEST_GIVEN("two eCoords for arithmetic operations")
        {
            eCoord a(1.0f, 2.0f);
            eCoord b(3.0f, 4.0f);

            // Addition
            eCoord sum = a + b;
            DOCTEST_THEN("addition produces correct results")
            {
                CHECK(sum.x == 4.0f);
                CHECK(sum.y == 6.0f);
            }

            // Subtraction
            eCoord diff = a - b;
            DOCTEST_THEN("subtraction produces correct results")
            {
                CHECK(diff.x == -2.0f);
                CHECK(diff.y == -2.0f);
            }

            // Negation
            eCoord neg = -a;
            DOCTEST_THEN("negation produces correct results")
            {
                CHECK(neg.x == -1.0f);
                CHECK(neg.y == -2.0f);
            }

            // Scalar multiplication
            eCoord scaled = a * 2.0f;
            DOCTEST_THEN("scalar multiplication produces correct results")
            {
                CHECK(scaled.x == 2.0f);
                CHECK(scaled.y == 4.0f);
            }

            // Scalar multiplication assignment
            eCoord a2(1.0f, 2.0f);
            a2 *= 3.0f;
            DOCTEST_THEN("scalar multiplication assignment produces correct results")
            {
                CHECK(a2.x == 3.0f);
                CHECK(a2.y == 6.0f);
            }
        }
    }

    TEST_CASE("eCoord norm calculations")
    {
        DOCTEST_GIVEN("an eCoord for norm calculations")
        {
            eCoord v(3.0f, 4.0f);

            // NormSquared
            REAL normSq = v.NormSquared();
            DOCTEST_THEN("NormSquared returns the squared magnitude")
            {
                CHECK(normSq == 25.0f);
            }

            // Norm
            REAL norm = v.Norm();
            DOCTEST_THEN("Norm returns the magnitude")
            {
                CHECK(norm == doctest::Approx(5.0f));
            }
        }
    }

    TEST_CASE("eCoord scalar product (F)")
    {
        DOCTEST_GIVEN("two eCoords for scalar product")
        {
            eCoord a(1.0f, 2.0f);
            eCoord b(3.0f, 4.0f);

            REAL dot = eCoord::F(a, b);
            DOCTEST_THEN("F returns the dot product")
            {
                CHECK(dot == 11.0f); // 1*3 + 2*4 = 11
            }
        }
    }

    TEST_CASE("eCoord cross product operator*")
    {
        DOCTEST_GIVEN("two eCoords for cross product")
        {
            eCoord a(1.0f, 0.0f);
            eCoord b(0.0f, 1.0f);

            // Cross product in 2D: -x*a.y + y*a.x
            REAL cross = a * b;
            DOCTEST_THEN("operator* returns the 2D cross product")
            {
                CHECK(cross == -1.0f); // 1*1 + 0*0 with the negative sign = -1
            }

            eCoord c(1.0f, 1.0f);
            eCoord d(1.0f, 1.0f);
            DOCTEST_THEN("operator* returns zero for parallel vectors")
            {
                CHECK((c * d) == 0.0f); // parallel vectors
            }
        }
    }

    TEST_CASE("eCoord Turn (complex multiplication)")
    {
        DOCTEST_GIVEN("an eCoord for complex multiplication rotation")
        {
            eCoord v(1.0f, 0.0f);
            // Turn by 90 degrees (0,1 is unit vector at 90 degrees)
            eCoord turned = v.Turn(eCoord(0.0f, 1.0f));
            DOCTEST_THEN("Turn by 90 degrees rotates correctly")
            {
                CHECK(turned.x == doctest::Approx(0.0f));
                CHECK(turned.y == doctest::Approx(1.0f));
            }

            // Turn by -90 degrees (0,-1)
            eCoord turnedNeg = v.Turn(eCoord(0.0f, -1.0f));
            DOCTEST_THEN("Turn by -90 degrees rotates correctly")
            {
                CHECK(turnedNeg.x == doctest::Approx(0.0f));
                CHECK(turnedNeg.y == doctest::Approx(-1.0f));
            }
        }
    }

    TEST_CASE("eCoord Conj (complex conjugation)")
    {
        DOCTEST_GIVEN("an eCoord for complex conjugation")
        {
            eCoord v(1.0f, 2.0f);
            eCoord conj = v.Conj();
            DOCTEST_THEN("Conj negates the imaginary component")
            {
                CHECK(conj.x == 1.0f);
                CHECK(conj.y == -2.0f);
            }
        }
    }

    TEST_CASE("eCoord V function")
    {
        DOCTEST_GIVEN("three eCoords for V function")
        {
            // V(a, b, c) gives coordinate of b when a is at 0 and c is at 1
            eCoord a(0.0f, 0.0f);
            eCoord b(1.0f, 0.0f);
            eCoord c(2.0f, 0.0f);

            REAL result = eCoord::V(a, b, c);
            DOCTEST_THEN("V returns the normalized coordinate")
            {
                CHECK(result == doctest::Approx(0.5f));
            }
        }
    }

    TEST_CASE("eCoord equality operators")
    {
        DOCTEST_GIVEN("eCoords for equality comparison")
        {
            eCoord a(1.0f, 2.0f);
            eCoord b(1.0f, 2.0f);

            // Equality with EPS tolerance
            DOCTEST_THEN("operator== returns true for equal coordinates")
            {
                CHECK(a == b);
            }

            // Test inequality
            eCoord c(3.0f, 4.0f);
            DOCTEST_THEN("operator!= returns true for different coordinates")
            {
                CHECK(a != c);
            }
        }
    }

    TEST_CASE("eCoord se_EstimatedRangeOfMult")
    {
        DOCTEST_GIVEN("two eCoords for se_EstimatedRangeOfMult")
        {
            eCoord a(1.0f, 2.0f);
            eCoord b(3.0f, 4.0f);

            REAL range = se_EstimatedRangeOfMult(a, b);
            DOCTEST_THEN("se_EstimatedRangeOfMult returns a positive value")
            {
                // Just document that it returns a positive value
                CHECK(range > 0);
            }
        }
    }

    TEST_CASE("eCoord st_GetDifference")
    {
        DOCTEST_GIVEN("two eCoords for st_GetDifference")
        {
            eCoord a(1.0f, 2.0f);
            eCoord b(1.0f, 2.0f);

            REAL diff = st_GetDifference(a, b);
            DOCTEST_THEN("st_GetDifference returns a non-negative value")
            {
                CHECK(diff >= 0);
            }
        }
    }
}
