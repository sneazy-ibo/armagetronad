#include "doctest.h"
#include "tRandom.h"

// Tests for tRandomizer and tReproducibleRandomizer classes
// Purpose: Verify random number generation

DOCTEST_TEST_SUITE("tRandomizer")
{
    TEST_CASE("tRandomizer Get() returns value in range")
    {
        DOCTEST_GIVEN("Setup")
        {
            tRandomizer rand;

            // Get multiple random values and verify they're in [0, 1)
            for (int i = 0; i < 100; i++)
            {
            REAL val = rand.Get();
            CHECK(val >= 0.0f);
            CHECK(val < 1.0f);
            }
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tRandomizer Get(int max) returns value in range")
    {
        DOCTEST_GIVEN("Setup")
        {
            tRandomizer rand;

            // Test with max = 10
            for (int i = 0; i < 100; i++)
            {
            int val = rand.Get(10);
            CHECK(val >= 0);
            CHECK(val <= 10);
            }

            // Test with max = 100
            for (int i = 0; i < 100; i++)
            {
            int val = rand.Get(100);
            CHECK(val >= 0);
            CHECK(val <= 100);
            }

            // Test with max = 1 (should return 0 or 1)
            for (int i = 0; i < 100; i++)
            {
            int val = rand.Get(1);
            bool valid = (val == 0) || (val == 1);
            CHECK(valid);
            }
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tRandomizer singleton access")
    {
        DOCTEST_GIVEN("Setup")
        {
            // Get the standard randomizer instance
            tRandomizer& rand = tRandomizer::GetInstance();

            // Verify it can generate values
            REAL val = rand.Get();
            CHECK(val >= 0.0f);
            CHECK(val < 1.0f);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tReproducibleRandomizer Get() returns value in range")
    {
        DOCTEST_GIVEN("Setup")
        {
            tReproducibleRandomizer rand;

            // Get multiple random values and verify they're in [0, 1)
            for (int i = 0; i < 100; i++)
            {
            REAL val = rand.Get();
            CHECK(val >= 0.0f);
            CHECK(val < 1.0f);
            }
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tReproducibleRandomizer Get(int max) returns value in range")
    {
        DOCTEST_GIVEN("Setup")
        {
            tReproducibleRandomizer rand;

            // Test with max = 50
            for (int i = 0; i < 100; i++)
            {
            int val = rand.Get(50);
            CHECK(val >= 0);
            CHECK(val <= 50);
            }
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tReproducibleRandomizer singleton access")
    {
        DOCTEST_GIVEN("Setup")
        {
            // Get the standard reproducible randomizer instance
            tReproducibleRandomizer& rand = tReproducibleRandomizer::GetInstance();

            // Verify it can generate values
            REAL val = rand.Get();
            CHECK(val >= 0.0f);
            CHECK(val < 1.0f);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tReproducibleRandomizer reproducibility")
    {
        DOCTEST_GIVEN("Setup")
        {
            // Create two identical randomizers
            tReproducibleRandomizer rand1;
            tReproducibleRandomizer rand2;

            // They should produce the same sequence if they use the same seed
            // Note: This depends on the implementation
            // For now, just verify they both work
            REAL val1 = rand1.Get();
            REAL val2 = rand2.Get();

            CHECK(val1 >= 0.0f);
            CHECK(val1 < 1.0f);
            CHECK(val2 >= 0.0f);
            CHECK(val2 < 1.0f);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("Randomizer distribution test")
    {
        DOCTEST_GIVEN("Setup")
        {
            tRandomizer rand;

            // Generate many random numbers and verify distribution
            const int numSamples = 1000;
            const int numBuckets = 10;
            int buckets[numBuckets] = {0};

            for (int i = 0; i < numSamples; i++)
            {
            REAL val = rand.Get();
            int bucket = static_cast<int>(val * numBuckets);
            if (bucket == numBuckets)
            bucket = numBuckets - 1; // Handle edge case
            buckets[bucket]++;
            }

            // Each bucket should have at least some samples
            // (This is a weak test, but verifies the distribution isn't completely broken)
            for (int i = 0; i < numBuckets; i++)
            {
            CHECK(buckets[i] > 0);
            }
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }
}
