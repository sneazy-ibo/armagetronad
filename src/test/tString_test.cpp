#include "doctest.h"
#include "tString.h"
#include <cstring>

// Tests for tString class
// Purpose: Document the status quo behavior and detect regressions

DOCTEST_TEST_SUITE("tString")
{
    TEST_CASE("tString default construction")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s;
            // tString includes null terminator, so Len() is 1 for empty string
            CHECK(s.Len() >= 1);
            CHECK(strcmp(s, "") == 0);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString construction from C string")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            CHECK(strcmp(s, "Hello") == 0);
            // Len includes null terminator
            CHECK(s.Len() == 6); // "Hello" + '\0'
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString copy construction")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString original("Hello");
            tString copy = original;
            CHECK(strcmp(copy, "Hello") == 0);
            CHECK(copy.Len() == original.Len());
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString assignment from C string")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s;
            s = "World";
            CHECK(strcmp(s, "World") == 0);
            CHECK(s.Len() == 6); // "World" + '\0'
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString assignment operator")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString original("Hello");
            tString assigned;
            assigned = original;
            CHECK(strcmp(assigned, "Hello") == 0);
            CHECK(assigned.Len() == original.Len());
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString operator+= with C string")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            s += " World";
            CHECK(strcmp(s, "Hello World") == 0);
            CHECK(s.Len() == 12); // "Hello World" + '\0'
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString operator+ with C string")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            tString result = s + " World";
            CHECK(strcmp(result, "Hello World") == 0);
            CHECK(result.Len() == 12); // "Hello World" + '\0'
            // Original should be unchanged
            CHECK(strcmp(s, "Hello") == 0);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString operator<< with C string")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s;
            s << "Hello";
            CHECK(strcmp(s, "Hello") == 0);
            CHECK(s.Len() == 6);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString operator<< with char")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s;
            s << 'H' << 'e' << 'l' << 'l' << 'o';
            CHECK(strcmp(s, "Hello") == 0);
            CHECK(s.Len() == 6);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString operator+ with char")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hell");
            tString result = s + 'o';
            CHECK(strcmp(result, "Hello") == 0);
            CHECK(result.Len() == 6);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString comparison operators - equal")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            CHECK(s == "Hello");
            CHECK(s != "World");
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString comparison operators - less than")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            CHECK(s < "World");
            CHECK(!(s < "Hello"));
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString comparison operators - greater than")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("World");
            CHECK(s > "Hello");
            CHECK(!(s > "World"));
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString Compare method")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            CHECK(s.Compare("Hello") == 0);
            CHECK(s.Compare("World") < 0);
            CHECK(s.Compare("Aardvark") > 0);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString SetPos")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            s.SetPos(10, false);
            CHECK(s.Len() == 10);

            s.SetPos(5, true);
            CHECK(s.Len() == 5);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString StartsWith")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello World");
            CHECK(s.StartsWith("Hello"));
            CHECK(s.StartsWith("Hello W"));
            CHECK(!s.StartsWith("World"));
            CHECK(!s.StartsWith("hello")); // case sensitive
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString StrPos")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello World");
            CHECK(s.StrPos("World") == 6);
            CHECK(s.StrPos("Hello") == 0);
            CHECK(s.StrPos("lo Wo") == 3);
            CHECK(s.StrPos("NotFound") == -1);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString SubStr with start and length")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello World");
            tString sub = s.SubStr(0, 5);
            CHECK(strcmp(sub, "Hello") == 0);

            sub = s.SubStr(6, 5);
            CHECK(strcmp(sub, "World") == 0);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString SubStr with start only")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello World");
            tString sub = s.SubStr(6);
            CHECK(strcmp(sub, "World") == 0);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString toInt")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("42");
            CHECK(s.toInt() == 42);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString conversion to const char*")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            const char* cstr = s;
            CHECK(strcmp(cstr, "Hello") == 0);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString boundary conditions - empty string")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s;
            CHECK(strcmp(s, "") == 0);
            // Len includes null terminator
            CHECK(s.Len() >= 1);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString boundary conditions - single character")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("X");
            CHECK(strcmp(s, "X") == 0);
            // Len includes null terminator
            CHECK(s.Len() == 2);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString concatenation of multiple strings")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s("Hello");
            s += " ";
            s += "World";
            CHECK(strcmp(s, "Hello World") == 0);
            CHECK(s.Len() == 12); // "Hello World" + '\0'
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString operator+ with tString")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s1("Hello");
            tString s2(" World");
            tString result = s1 + s2;
            CHECK(strcmp(result, "Hello World") == 0);
            CHECK(result.Len() == 12);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }

    TEST_CASE("tString operator+= with tString")
    {
        DOCTEST_GIVEN("Setup")
        {
            tString s1("Hello");
            tString s2(" World");
            s1 += s2;
            CHECK(strcmp(s1, "Hello World") == 0);
            CHECK(s1.Len() == 12);
            DOCTEST_THEN("Verification")
            {
                // All assertions verified in setup
            }
        }
    }
}
