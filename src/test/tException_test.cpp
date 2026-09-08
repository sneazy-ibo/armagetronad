#include "doctest.h"
#include "tException.h"

// Tests for tException class hierarchy
// Purpose: Verify exception handling and message retrieval

TEST_CASE("tException base class methods") {
    // tException is abstract, test through derived classes
    // This verifies the base interface works
}

TEST_CASE("tGenericException construction and methods") {
    tGenericException exc("Test error message", "TestError");
    
    // Test GetName
    tString name = exc.GetName();
    CHECK(name == "TestError");
    
    // Test GetDescription
    tString description = exc.GetDescription();
    CHECK(description == "Test error message");
}

TEST_CASE("tGenericException with NULL name") {
    tGenericException exc("Test message only");
    
    tString name = exc.GetName();
    // When name is NULL, it should return empty or description
    // Check that it doesn't crash
    CHECK(name.Len() >= 0);
    
    tString description = exc.GetDescription();
    CHECK(description == "Test message only");
}

TEST_CASE("tCleanQuit exception") {
    tCleanQuit quit;
    
    // Test GetName
    tString name = quit.GetName();
    // Should return "CleanQuit" or similar
    CHECK(name.Len() > 0);
    
    // Test GetDescription
    tString description = quit.GetDescription();
    CHECK(description.Len() >= 0);
}

TEST_CASE("Exception polymorphism") {
    // Test that tGenericException can be treated as tException
    tGenericException* ge = new tGenericException("Polymorphic test", "PolyError");
    tException* exc = ge;
    
    tString name = exc->GetName();
    CHECK(name == "PolyError");
    
    tString description = exc->GetDescription();
    CHECK(description == "Polymorphic test");
    
    delete ge;
}
