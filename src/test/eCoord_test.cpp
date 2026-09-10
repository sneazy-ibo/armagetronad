#include "doctest.h"
#include "eCoord.h"

// Tests for eCoord class
// Purpose: Document the status quo behavior and detect regressions

TEST_CASE("eCoord construction and default values") {
    eCoord defaultCoord;
    CHECK(defaultCoord.x == 0);
    CHECK(defaultCoord.y == 0);

    eCoord explicitCoord(1.0f, 2.0f);
    CHECK(explicitCoord.x == 1.0f);
    CHECK(explicitCoord.y == 2.0f);

    eCoord singleArg(5.0f);
    CHECK(singleArg.x == 5.0f);
    CHECK(singleArg.y == 0);
}

TEST_CASE("eCoord copy construction and assignment") {
    eCoord original(3.0f, 4.0f);
    eCoord copy = original;
    CHECK(copy.x == original.x);
    CHECK(copy.y == original.y);

    eCoord assigned(0, 0);
    assigned = original;
    CHECK(assigned.x == original.x);
    CHECK(assigned.y == original.y);
}

TEST_CASE("eCoord arithmetic operators") {
    eCoord a(1.0f, 2.0f);
    eCoord b(3.0f, 4.0f);

    // Addition
    eCoord sum = a + b;
    CHECK(sum.x == 4.0f);
    CHECK(sum.y == 6.0f);

    // Subtraction
    eCoord diff = a - b;
    CHECK(diff.x == -2.0f);
    CHECK(diff.y == -2.0f);

    // Negation
    eCoord neg = -a;
    CHECK(neg.x == -1.0f);
    CHECK(neg.y == -2.0f);

    // Scalar multiplication
    eCoord scaled = a * 2.0f;
    CHECK(scaled.x == 2.0f);
    CHECK(scaled.y == 4.0f);

    // Scalar multiplication assignment
    eCoord a2(1.0f, 2.0f);
    a2 *= 3.0f;
    CHECK(a2.x == 3.0f);
    CHECK(a2.y == 6.0f);
}

TEST_CASE("eCoord norm calculations") {
    eCoord v(3.0f, 4.0f);

    // NormSquared
    REAL normSq = v.NormSquared();
    CHECK(normSq == 25.0f);

    // Norm
    REAL norm = v.Norm();
    CHECK(norm == doctest::Approx(5.0f));
}

TEST_CASE("eCoord scalar product (F)") {
    eCoord a(1.0f, 2.0f);
    eCoord b(3.0f, 4.0f);

    REAL dot = eCoord::F(a, b);
    CHECK(dot == 11.0f); // 1*3 + 2*4 = 11
}

TEST_CASE("eCoord cross product operator*") {
    eCoord a(1.0f, 0.0f);
    eCoord b(0.0f, 1.0f);

    // Cross product in 2D: -x*a.y + y*a.x
    REAL cross = a * b;
    CHECK(cross == -1.0f); // 1*1 + 0*0 with the negative sign = -1

    eCoord c(1.0f, 1.0f);
    eCoord d(1.0f, 1.0f);
    CHECK((c * d) == 0.0f); // parallel vectors
}

TEST_CASE("eCoord Turn (complex multiplication)") {
    eCoord v(1.0f, 0.0f);
    // Turn by 90 degrees (0,1 is unit vector at 90 degrees)
    eCoord turned = v.Turn(eCoord(0.0f, 1.0f));
    CHECK(turned.x == doctest::Approx(0.0f));
    CHECK(turned.y == doctest::Approx(1.0f));

    // Turn by -90 degrees (0,-1)
    eCoord turnedNeg = v.Turn(eCoord(0.0f, -1.0f));
    CHECK(turnedNeg.x == doctest::Approx(0.0f));
    CHECK(turnedNeg.y == doctest::Approx(-1.0f));
}

TEST_CASE("eCoord Conj (complex conjugation)") {
    eCoord v(1.0f, 2.0f);
    eCoord conj = v.Conj();
    CHECK(conj.x == 1.0f);
    CHECK(conj.y == -2.0f);
}

TEST_CASE("eCoord V function") {
    // V(a, b, c) gives coordinate of b when a is at 0 and c is at 1
    eCoord a(0.0f, 0.0f);
    eCoord b(1.0f, 0.0f);
    eCoord c(2.0f, 0.0f);
    
    REAL result = eCoord::V(a, b, c);
    CHECK(result == doctest::Approx(0.5f));
}

TEST_CASE("eCoord equality operators") {
    eCoord a(1.0f, 2.0f);
    eCoord b(1.0f, 2.0f);

    // Equality with EPS tolerance
    CHECK(a == b);
    
    // Test inequality
    eCoord c(3.0f, 4.0f);
    CHECK(a != c);
}

TEST_CASE("eCoord se_EstimatedRangeOfMult") {
    eCoord a(1.0f, 2.0f);
    eCoord b(3.0f, 4.0f);
    
    REAL range = se_EstimatedRangeOfMult(a, b);
    // Just document that it returns a positive value
    CHECK(range > 0);
}

TEST_CASE("eCoord st_GetDifference") {
    eCoord a(1.0f, 2.0f);
    eCoord b(1.0f, 2.0f);
    
    REAL diff = st_GetDifference(a, b);
    CHECK(diff >= 0);
}
