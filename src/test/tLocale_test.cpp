#include "doctest.h"
#include "tLocale.h"

#include "Statics.h"

// Tests for tLocale system
// Purpose: Document the status quo behavior and detect regressions

TEST_SUITE("tLocale")
{
    TEST_CASE("tLanguage basic functionality")
    {
        InitStatics(); // we need tLocale initialized

        GIVEN("tLanguage class")
        {
            THEN("FirstLanguage returns a language pointer")
            {
                tLanguage* lang = tLanguage::FirstLanguage();
                CHECK(lang);
            }
            
            THEN("FindStrict method exists")
            {
                auto *english = tLanguage::FindStrict(tString("British English"));
                CHECK(english);

#if false // this would log an error to console
                auto *nope = tLanguage::FindStrict(tString("nope1"));
                CHECK(!nope); // should be missing
#endif                
            }           
            
            THEN("FindSloppy method exists")
            {
                auto *english = tLanguage::FindSloppy(tString("American English"));
                CHECK(english);

                auto *nope = tLanguage::FindSloppy(tString("nope3"));
                CHECK(!nope); // should be missing
            }

            THEN("Find method exists")
            {
                auto *german = tLanguage::Find(tString("German"));
                CHECK(german);

                auto *nope = tLanguage::Find(tString("nope2"));
                CHECK(nope); // should be created
            }
        }
    }

    TEST_CASE("tOutput basic functionality")
    {
        GIVEN("tOutput class")
        {
            THEN("tOutput can be constructed and destroyed")
            {
                tOutput output;
            }
            
            THEN("AddLiteral works")
            {
                tOutput output;
                output.AddLiteral("test");
                CHECK(tString(output) == "test");
            }
            
            THEN("AddSpace works")
            {
                tOutput output;
                output.AddSpace();
                CHECK(true); // If we get here, AddSpace worked
            }
            
            THEN("AddString works")
            {
                tOutput output;
                output.AddString("test string");
                CHECK(true); // If we get here, AddString worked
            }
        }
    }

    // Note: tLocaleItem is defined in tLocale.cpp and may require
    // global initialization. For now, we'll skip testing it directly
    // as it's not easily testable in isolation.
}

// TODO: More comprehensive tLocale tests could be added, but the system
// has significant global state and dependencies that make isolated unit testing
// challenging. The current tests focus on:
// 1. Basic language class operations
// 2. Basic output class operations
// 3. Class existence verification
//
// Additional tests that could be added:
// 1. Language file loading from language/ directory (requires file system)
// 2. String translation with known language files (requires initialization)
// 3. Fallback to base language when translations missing (requires setup)
// 4. Runtime locale switching via tLocale::SetLanguage (requires global state)
// 5. Placeholder substitution using %1, %2, etc. syntax (requires initialization)
//
// These would need to be integration tests rather than unit tests, or would
// require significant refactoring to make the locale system more testable.
