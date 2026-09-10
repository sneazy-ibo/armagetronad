#include "doctest.h"
#include "tMemStack.h"

// Tests for tMemStack class
// Purpose: Document the status quo behavior and detect regressions

DOCTEST_TEST_SUITE("tMemStack")
{
    TEST_CASE("tMemStack construction and destruction")
    {
        DOCTEST_GIVEN("Setup")
        {
            tMemStack stack;
            CHECK(stack.GetMem() != nullptr);
            CHECK(stack.GetSize() >= 10);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tMemStack IncreaseMem")
    {
        DOCTEST_GIVEN("Setup")
        {
            tMemStack stack;
            int originalSize = stack.GetSize();

            stack.IncreaseMem();
            CHECK(stack.GetSize() > originalSize);
            CHECK(stack.GetMem() != nullptr);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }
}
