#include "doctest.h"
#include "tMemStack.h"

// Tests for tMemStack class
// Purpose: Document the status quo behavior and detect regressions

DOCTEST_TEST_SUITE("tMemStack")
{
    TEST_CASE("tMemStack construction and destruction")
    {
        DOCTEST_GIVEN("a tMemStack for testing")
        {
            tMemStack stack;
            DOCTEST_THEN("GetMem returns non-null pointer")
            {
                CHECK(stack.GetMem() != nullptr);
            }
            DOCTEST_THEN("GetSize returns a reasonable size")
            {
                CHECK(stack.GetSize() >= 10);
            }
        }
    }

    TEST_CASE("tMemStack IncreaseMem")
    {
        DOCTEST_GIVEN("a tMemStack for memory increase testing")
        {
            tMemStack stack;
            int originalSize = stack.GetSize();

            stack.IncreaseMem();
            DOCTEST_THEN("GetSize returns a larger size after IncreaseMem")
            {
                CHECK(stack.GetSize() > originalSize);
            }
            DOCTEST_THEN("GetMem still returns non-null pointer")
            {
                CHECK(stack.GetMem() != nullptr);
            }
        }
    }
}
