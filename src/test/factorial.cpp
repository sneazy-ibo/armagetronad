#include "doctest.h"

// from the doctest samples. This is here to test whether the test framework integration works.

int factorial(int number) {
    return number <= 1 ? 1 : factorial(number - 1) * number;
}

TEST_CASE("testing the factorial function") {
    CHECK(factorial(0) == 1);
    CHECK(factorial(1) == 1);
    CHECK(factorial(2) == 2);
    CHECK(factorial(3) == 6);
    CHECK(factorial(10) == 3628800);
}
