#include "doctest.h"
#include "tMemStack.h"

// Tests for tMemStack class
// Purpose: Document the status quo behavior and detect regressions

TEST_CASE("tMemStack construction and destruction") {
    tMemStack stack;
    CHECK(stack.GetMem() != nullptr);
    CHECK(stack.GetSize() >= 10);
}

TEST_CASE("tMemStack IncreaseMem") {
    tMemStack stack;
    int originalSize = stack.GetSize();
    
    stack.IncreaseMem();
    CHECK(stack.GetSize() > originalSize);
    CHECK(stack.GetMem() != nullptr);
}
