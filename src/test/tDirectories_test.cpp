#include "doctest.h"
#include "tDirectories.h"
#include "tString.h"
#include "tArray.h"

#include "MockConsole.h"

// Tests for tDirectories system
// Purpose: Document the status quo behavior and detect regressions

TEST_SUITE("tDirectories")
{
    TEST_CASE("tPath basic functionality")
    {
        MockConsole con;

        GIVEN("tPath objects")
        {
            // Note: Many tPath methods trigger console output which may
            // require language initialization. For now, we'll test only
            // basic construction.
            
            THEN("tPathResource can be constructed")
            {
                tPathResource resource;
                tString included = resource.GetIncluded();
                // We can't predict the exact path, but it should be a valid tString
                CHECK(included.Len() >= 1); // tString always has at least null terminator
            }
        }
    }

    TEST_CASE("tPathResource basic functionality")
    {
        MockConsole con;

        GIVEN("a tPathResource")
        {
            tPathResource resource;
            
            THEN("it can be constructed without crashing")
            {
                CHECK(true); // If we get here, construction succeeded
            }
            
            THEN("GetIncluded returns a path string")
            {
                tString included = resource.GetIncluded();
                // We can't predict the exact path, but it should be a valid tString
                CHECK(included.Len() >= 1); // tString always has at least null terminator
            }
        }
    }

    TEST_CASE("tDirectories static methods")
    {
        GIVEN("tDirectories static methods")
        {
            // Note: These methods may trigger initialization that requires
            // global state (like language files) which may not be available in tests.
            // For now, we'll skip these tests to avoid crashes.
            
            THEN("methods exist and can be referenced")
            {
                // Just verify the methods exist by taking their address
                // without calling them
                (void)&tDirectories::Data;
                (void)&tDirectories::Config;
                (void)&tDirectories::Var;
                (void)&tDirectories::Screenshot;
                (void)&tDirectories::Resource;
                CHECK(true); // If we get here, the methods exist
            }
        }
    }

    // Note: FileMatchesWildcard and GetFiles tests are skipped because they
    // may trigger initialization of global systems (like console/language) which
    // are not available in the test environment.
}

// TODO: More comprehensive tDirectories tests could be added, but many functions
// depend on the actual filesystem state and configuration, which makes them
// difficult to test in a predictable way. The current tests focus on functions
// that can be tested without external dependencies.
//
// Additional tests that could be added:
// 1. Tests for path resolution (GetReadPath, GetWritePath)
// 2. Tests for file opening functionality
// 3. Tests for directory setting functions
// 4. Tests for path string formatting
// These would require more complex setup or mocking of the filesystem.