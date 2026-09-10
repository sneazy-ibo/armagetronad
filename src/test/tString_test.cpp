#include "doctest.h"
#include "tString.h"
#include <cstring>

// Tests for tString class
// Purpose: Document the status quo behavior and detect regressions

DOCTEST_TEST_SUITE("tString")
{
    TEST_CASE("tString default construction")
    {
        tString s;
        // tString includes null terminator, so Len() is 1 for empty string
        CHECK(s.Len() >= 1);
        CHECK(strcmp(s, "") == 0);
    }

    TEST_CASE("tString construction from C string")
    {
        tString s("Hello");
        CHECK(strcmp(s, "Hello") == 0);
        // Len includes null terminator
        CHECK(s.Len() == 6); // "Hello" + '\0'
    }

    TEST_CASE("tString copy construction")
    {
        tString original("Hello");
        tString copy = original;
        CHECK(strcmp(copy, "Hello") == 0);
        CHECK(copy.Len() == original.Len());
    }

    TEST_CASE("tString assignment from C string")
    {
        tString s;
        s = "World";
        CHECK(strcmp(s, "World") == 0);
        CHECK(s.Len() == 6); // "World" + '\0'
    }

    TEST_CASE("tString assignment operator")
    {
        tString original("Hello");
        tString assigned;
        assigned = original;
        CHECK(strcmp(assigned, "Hello") == 0);
        CHECK(assigned.Len() == original.Len());
    }

    TEST_CASE("tString operator+= with C string")
    {
        tString s("Hello");
        s += " World";
        CHECK(strcmp(s, "Hello World") == 0);
        CHECK(s.Len() == 12); // "Hello World" + '\0'
    }

    TEST_CASE("tString operator+ with C string")
    {
        tString s("Hello");
        tString result = s + " World";
        CHECK(strcmp(result, "Hello World") == 0);
        CHECK(result.Len() == 12); // "Hello World" + '\0'
        // Original should be unchanged
        CHECK(strcmp(s, "Hello") == 0);
    }

    TEST_CASE("tString operator<< with C string")
    {
        tString s;
        s << "Hello";
        CHECK(strcmp(s, "Hello") == 0);
        CHECK(s.Len() == 6);
    }

    TEST_CASE("tString operator<< with char")
    {
        tString s;
        s << 'H' << 'e' << 'l' << 'l' << 'o';
        CHECK(strcmp(s, "Hello") == 0);
        CHECK(s.Len() == 6);
    }

    TEST_CASE("tString operator+ with char")
    {
        tString s("Hell");
        tString result = s + 'o';
        CHECK(strcmp(result, "Hello") == 0);
        CHECK(result.Len() == 6);
    }

    TEST_CASE("tString comparison operators - equal")
    {
        tString s("Hello");
        CHECK(s == "Hello");
        CHECK(s != "World");
    }

    TEST_CASE("tString comparison operators - less than")
    {
        tString s("Hello");
        CHECK(s < "World");
        CHECK(!(s < "Hello"));
    }

    TEST_CASE("tString comparison operators - greater than")
    {
        tString s("World");
        CHECK(s > "Hello");
        CHECK(!(s > "World"));
    }

    TEST_CASE("tString Compare method")
    {
        tString s("Hello");
        CHECK(s.Compare("Hello") == 0);
        CHECK(s.Compare("World") < 0);
        CHECK(s.Compare("Aardvark") > 0);
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
        tString s("Hello World");
        CHECK(s.StartsWith("Hello"));
        CHECK(s.StartsWith("Hello W"));
        CHECK(!s.StartsWith("World"));
        CHECK(!s.StartsWith("hello")); // case sensitive
    }

    TEST_CASE("tString StrPos")
    {
        tString s("Hello World");
        CHECK(s.StrPos("World") == 6);
        CHECK(s.StrPos("Hello") == 0);
        CHECK(s.StrPos("lo Wo") == 3);
        CHECK(s.StrPos("NotFound") == -1);
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
        tString s("Hello World");
        tString sub = s.SubStr(6);
        CHECK(strcmp(sub, "World") == 0);
    }

    TEST_CASE("tString toInt")
    {
        tString s("42");
        CHECK(s.toInt() == 42);
    }

    TEST_CASE("tString conversion to const char*")
    {
        tString s("Hello");
        const char* cstr = s;
        CHECK(strcmp(cstr, "Hello") == 0);
    }

    TEST_CASE("tString boundary conditions - empty string")
    {
        tString s;
        CHECK(strcmp(s, "") == 0);
        // Len includes null terminator
        CHECK(s.Len() >= 1);
    }

    TEST_CASE("tString boundary conditions - single character")
    {
        tString s("X");
        CHECK(strcmp(s, "X") == 0);
        // Len includes null terminator
        CHECK(s.Len() == 2);
    }

    TEST_CASE("tString concatenation of multiple strings")
    {
        tString s("Hello");
        s += " ";
        s += "World";
        CHECK(strcmp(s, "Hello World") == 0);
        CHECK(s.Len() == 12); // "Hello World" + '\0'
    }

    TEST_CASE("tString operator+ with tString")
    {
        tString s1("Hello");
        tString s2(" World");
        tString result = s1 + s2;
        CHECK(strcmp(result, "Hello World") == 0);
        CHECK(result.Len() == 12);
    }

    TEST_CASE("tString operator+= with tString")
    {
        tString s1("Hello");
        tString s2(" World");
        s1 += s2;
        CHECK(strcmp(s1, "Hello World") == 0);
        CHECK(s1.Len() == 12);
    }
}
