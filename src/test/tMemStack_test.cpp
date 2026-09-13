#include "doctest.h"
#include "tMemStack.h"

// Tests for tMemStack class
// Purpose: Document the status quo behavior and detect regressions

TEST_SUITE("tMemStack")
{
    TEST_CASE("tMemStack construction and destruction")
    {
        GIVEN("a tMemStack for testing")
        {
            tMemStack stack;
            THEN("GetMem returns non-null pointer")
            {
                CHECK(stack.GetMem() != nullptr);
            }
            THEN("GetSize returns a reasonable size")
            {
                CHECK(stack.GetSize() >= 10);
            }
        }
    }

    TEST_CASE("tMemStack IncreaseMem")
    {
        GIVEN("a tMemStack for memory increase testing")
        {
            tMemStack stack;
            int originalSize = stack.GetSize();

            stack.IncreaseMem();
            THEN("GetSize returns a larger size after IncreaseMem")
            {
                CHECK(stack.GetSize() > originalSize);
            }
            THEN("GetMem still returns non-null pointer")
            {
                CHECK(stack.GetMem() != nullptr);
            }
        }
    }
}
