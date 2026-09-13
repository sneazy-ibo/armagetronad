#include "doctest.h"
#include "tHeap.h"

// Tests for tHeap class
// Purpose: Document the status quo behavior and detect regressions

// For now, just test that we can include the header and basic types exist
TEST_SUITE("tHeap")
{
    TEST_CASE("tHeap header inclusion")
    {
        GIVEN("the tHeap header is included")
        {
            // Just verify the header can be included
            THEN("the test passes")
            {
                CHECK(true);
            }
        }
    }
}
