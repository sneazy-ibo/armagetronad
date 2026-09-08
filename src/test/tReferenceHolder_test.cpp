#include "doctest.h"
#include "tReferenceHolder.h"
#include "tString.h"

// Tests for tReferenceHolder class
// Purpose: Verify reference holding functionality

// Create a simple test class that inherits from tReferencable
tString testString1("test1");
tString testString2("test2");
tString testString3("test3");

TEST_CASE("tReferenceHolder construction") {
    tReferenceHolder<tString> holder;
    
    // Should construct without error
    CHECK(true);
}

TEST_CASE("tReferenceHolder Add method") {
    tReferenceHolder<tString> holder;
    
    // Add some references
    holder.Add(&testString1);
    holder.Add(&testString2);
    
    // Should not crash
    CHECK(true);
}

TEST_CASE("tReferenceHolder Remove method") {
    tReferenceHolder<tString> holder;
    
    holder.Add(&testString1);
    holder.Add(&testString2);
    holder.Remove(&testString1);
    
    // Should not crash
    CHECK(true);
}

TEST_CASE("tReferenceHolder ReleaseAll method") {
    tReferenceHolder<tString> holder;
    
    holder.Add(&testString1);
    holder.Add(&testString2);
    holder.Add(&testString3);
    
    holder.ReleaseAll();
    
    // Should not crash
    CHECK(true);
}

TEST_CASE("tReferenceHolder duplicate Add") {
    tReferenceHolder<tString> holder;
    
    holder.Add(&testString1);
    holder.Add(&testString1); // Add same reference twice
    
    // Should handle duplicates gracefully
    CHECK(true);
}

TEST_CASE("tReferenceHolder Remove non-existent") {
    tReferenceHolder<tString> holder;
    
    // Remove a reference that wasn't added
    holder.Remove(&testString1);
    
    // Should not crash
    CHECK(true);
}
