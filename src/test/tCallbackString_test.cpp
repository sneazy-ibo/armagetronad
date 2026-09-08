#include "doctest.h"
#include "tCallbackString.h"

// Tests for tCallbackString class
// Purpose: Verify string-returning callback registration and execution

// Helper functions for testing
tString stringFunc1() { return tString("test1"); }
tString stringFunc2() { return tString("test2"); }
tString emptyStringFunc() { return tString(""); }

TEST_CASE("tCallbackString construction and execution") {
    tCallbackString* anchor = NULL;
    
    // Create a callback with our test function
    tCallbackString cb(anchor, stringFunc1);
    
    // Execute all callbacks and get the result
    tString result = tCallbackString::Exec(anchor);
    
    // Should return the string from our function
    CHECK(result == "test1");
}

TEST_CASE("tCallbackString multiple callbacks") {
    tCallbackString* anchor = NULL;
    
    // Create multiple callbacks
    tCallbackString cb1(anchor, stringFunc1);
    tCallbackString cb2(anchor, stringFunc2);
    
    // Execute - should return the first one (or concatenate, depending on implementation)
    tString result = tCallbackString::Exec(anchor);
    
    // Check that we got a result
    CHECK(result.Len() > 0);
}

TEST_CASE("tCallbackString empty string") {
    tCallbackString* anchor = NULL;
    
    tCallbackString cb(anchor, emptyStringFunc);
    
    tString result = tCallbackString::Exec(anchor);
    CHECK(result == "");
}
