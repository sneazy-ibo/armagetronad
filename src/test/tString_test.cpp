#include "doctest.h"
#include "tString.h"
#include <cstring>

// Tests for tString class
// Purpose: Document the status quo behavior and detect regressions

DOCTEST_TEST_SUITE("tString")
{
    TEST_CASE("tString default construction")
    {
        DOCTEST_GIVEN("a default tString")
        {
            tString s;
            // tString includes null terminator, so Len() is 1 for empty string
            DOCTEST_THEN("length is at least 1 and content is empty")
            {
                CHECK(s.Len() >= 1);
                CHECK(strcmp(s, "") == 0);
            }
        }
    }

    TEST_CASE("tString construction from C string")
    {
        DOCTEST_GIVEN("a tString constructed from C string")
        {
            tString s("Hello");
            DOCTEST_THEN("content matches the C string")
            {
                CHECK(strcmp(s, "Hello") == 0);
                // Len includes null terminator
                CHECK(s.Len() == 6); // "Hello" + '\0'
            }
        }
    }

    TEST_CASE("tString copy construction")
    {
        DOCTEST_GIVEN("a tString copy")
        {
            tString original("Hello");
            tString copy = original;
            DOCTEST_THEN("copy matches the original")
            {
                CHECK(strcmp(copy, "Hello") == 0);
                CHECK(copy.Len() == original.Len());
            }
        }
    }

    TEST_CASE("tString assignment from C string")
    {
        DOCTEST_GIVEN("a tString assigned from C string")
        {
            tString s;
            s = "World";
            DOCTEST_THEN("content matches the C string")
            {
                CHECK(strcmp(s, "World") == 0);
                CHECK(s.Len() == 6); // "World" + '\0'
            }
        }
    }

    TEST_CASE("tString assignment operator")
    {
        DOCTEST_GIVEN("a tString assigned from another tString")
        {
            tString original("Hello");
            tString assigned;
            assigned = original;
            DOCTEST_THEN("assigned string matches the original")
            {
                CHECK(strcmp(assigned, "Hello") == 0);
                CHECK(assigned.Len() == original.Len());
            }
        }
    }

    TEST_CASE("tString operator+= with C string")
    {
        DOCTEST_GIVEN("a tString for operator+=")
        {
            tString s("Hello");
            s += " World";
            DOCTEST_THEN("concatenation produces correct result")
            {
                CHECK(strcmp(s, "Hello World") == 0);
                CHECK(s.Len() == 12); // "Hello World" + '\0'
            }
        }
    }

    TEST_CASE("tString operator+ with C string")
    {
        DOCTEST_GIVEN("a tString for operator+")
        {
            tString s("Hello");
            tString result = s + " World";
            // Original should be unchanged
            DOCTEST_THEN("concatenation produces correct result")
            {
                CHECK(strcmp(result, "Hello World") == 0);
                CHECK(result.Len() == 12); // "Hello World" + '\0'
                CHECK(strcmp(s, "Hello") == 0);
            }
        }
    }

    TEST_CASE("tString operator<< with C string")
    {
        DOCTEST_GIVEN("a tString for operator<<")
        {
            tString s;
            s << "Hello";
            DOCTEST_THEN("appended C string produces correct result")
            {
                CHECK(strcmp(s, "Hello") == 0);
                CHECK(s.Len() == 6);
            }
        }
    }

    TEST_CASE("tString operator<< with char")
    {
        DOCTEST_GIVEN("a tString for operator<<")
        {
            tString s;
            s << 'H' << 'e' << 'l' << 'l' << 'o';
            DOCTEST_THEN("appended char produces correct result")
            {
                CHECK(strcmp(s, "Hello") == 0);
                CHECK(s.Len() == 6);
            }
        }
    }

    TEST_CASE("tString operator+ with char")
    {
        DOCTEST_GIVEN("a tString for operator+")
        {
            tString s("Hell");
            tString result = s + 'o';
            DOCTEST_THEN("concatenation with char produces correct result")
            {
                CHECK(strcmp(result, "Hello") == 0);
                CHECK(result.Len() == 6);
            }
        }
    }

    TEST_CASE("tString comparison operators - equal")
    {
        DOCTEST_GIVEN("two tStrings for equality comparison")
        {
            tString s("Hello");
            DOCTEST_THEN("operator== returns correct result")
            {
                CHECK(s == "Hello");
                CHECK(s != "World");
            }
        }
    }

    TEST_CASE("tString comparison operators - less than")
    {
        DOCTEST_GIVEN("two tStrings for less-than comparison")
        {
            tString s("Hello");
            DOCTEST_THEN("operator< returns correct result")
            {
                CHECK(s < "World");
                CHECK(!(s < "Hello"));
            }
        }
    }

    TEST_CASE("tString comparison operators - greater than")
    {
        DOCTEST_GIVEN("two tStrings for greater-than comparison")
        {
            tString s("World");
            DOCTEST_THEN("operator> returns correct result")
            {
                CHECK(s > "Hello");
                CHECK(!(s > "World"));
            }
        }
    }

    TEST_CASE("tString Compare method")
    {
        DOCTEST_GIVEN("two tStrings for Compare method")
        {
            tString s("Hello");
            DOCTEST_THEN("Compare returns correct result")
            {
                CHECK(s.Compare("Hello") == 0);
                CHECK(s.Compare("World") < 0);
                CHECK(s.Compare("Aardvark") > 0);
            }
        }
    }

    TEST_CASE("tString SetPos")
    {
        tString s("Hello");
        s.SetPos(10, false);
        CHECK(s.Len() == 10);

        s.SetPos(5, true);
        CHECK(s.Len() == 5);
    }

    TEST_CASE("tString StartsWith")
    {
        DOCTEST_GIVEN("a tString for StartsWith")
        {
            tString s("Hello World");
            DOCTEST_THEN("StartsWith returns correct result")
            {
                CHECK(s.StartsWith("Hello"));
                CHECK(s.StartsWith("Hello W"));
                CHECK(!s.StartsWith("World"));
                CHECK(!s.StartsWith("hello")); // case sensitive
            }
        }
    }

    TEST_CASE("tString StrPos")
    {
        DOCTEST_GIVEN("a tString for StrPos")
        {
            tString s("Hello World");
            DOCTEST_THEN("StrPos returns correct position")
            {
                CHECK(s.StrPos("World") == 6);
                CHECK(s.StrPos("Hello") == 0);
                CHECK(s.StrPos("lo Wo") == 3);
                CHECK(s.StrPos("NotFound") == -1);
            }
        }
    }

    TEST_CASE("tString SubStr with start and length")
    {
        tString s("Hello World");
        tString sub = s.SubStr(0, 5);
        CHECK(strcmp(sub, "Hello") == 0);

        sub = s.SubStr(6, 5);
        CHECK(strcmp(sub, "World") == 0);
    }

    TEST_CASE("tString SubStr with start only")
    {
        DOCTEST_GIVEN("a tString for SubStr with start only")
        {
            tString s("Hello World");
            tString sub = s.SubStr(6);
            DOCTEST_THEN("SubStr returns correct substring")
            {
                CHECK(strcmp(sub, "World") == 0);
            }
        }
    }

    TEST_CASE("tString toInt")
    {
        DOCTEST_GIVEN("a tString for toInt")
        {
            tString s("42");
            DOCTEST_THEN("toInt converts string to integer correctly")
            {
                CHECK(s.toInt() == 42);
            }
        }
    }

    TEST_CASE("tString conversion to const char*")
    {
        DOCTEST_GIVEN("a tString for conversion to const char*")
        {
            tString s("Hello");
            const char* cstr = s;
            DOCTEST_THEN("conversion to const char* works correctly")
            {
                CHECK(strcmp(cstr, "Hello") == 0);
            }
        }
    }

    TEST_CASE("tString boundary conditions - empty string")
    {
        DOCTEST_GIVEN("an empty tString")
        {
            tString s;
            DOCTEST_THEN("empty string behaves correctly")
            {
                CHECK(strcmp(s, "") == 0);
                // Len includes null terminator
                CHECK(s.Len() >= 1);
            }
        }
    }

    TEST_CASE("tString boundary conditions - single character")
    {
        DOCTEST_GIVEN("a single-character tString")
        {
            tString s("X");
            DOCTEST_THEN("single character string behaves correctly")
            {
                CHECK(strcmp(s, "X") == 0);
                // Len includes null terminator
                CHECK(s.Len() == 2);
            }
        }
    }

    TEST_CASE("tString concatenation of multiple strings")
    {
        DOCTEST_GIVEN("multiple tStrings for concatenation")
        {
            tString s("Hello");
            s += " ";
            s += "World";
            DOCTEST_THEN("concatenation produces correct result")
            {
                CHECK(strcmp(s, "Hello World") == 0);
                CHECK(s.Len() == 12); // "Hello World" + '\0'
            }
        }
    }

    TEST_CASE("tString operator+ with tString")
    {
        DOCTEST_GIVEN("two tStrings for operator+")
        {
            tString s1("Hello");
            tString s2(" World");
            tString result = s1 + s2;
            DOCTEST_THEN("concatenation produces correct result")
            {
                CHECK(strcmp(result, "Hello World") == 0);
                CHECK(result.Len() == 12);
            }
        }
    }

    TEST_CASE("tString operator+= with tString")
    {
        DOCTEST_GIVEN("a tString for operator+=")
        {
            tString s1("Hello");
            tString s2(" World");
            s1 += s2;
            DOCTEST_THEN("concatenation produces correct result")
            {
                CHECK(strcmp(s1, "Hello World") == 0);
                CHECK(s1.Len() == 12);
            }
        }
    }
}
