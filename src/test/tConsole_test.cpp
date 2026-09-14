#include "doctest.h"
#include "tConsole.h"

// Tests for tConsole system
// Purpose: Document the status quo behavior and detect regressions

namespace
{
// registers itself, swallows log
class MockConsole final : public tConsole
{
public:
    MockConsole() noexcept : tConsole{}
    {
        RegisterBetterConsole(this);
    }

    constexpr tString const& GetLastPrinted() const noexcept
    {
        return lastPrinted_;
    }

private:
    tConsole& DoPrint(const tString& s) noexcept override
    {
        lastPrinted_ = s;
        return *this;
    }

    tString lastPrinted_;
};

// mock console filter
class MockFilter final : public tConsoleFilter
{
    virtual void DoFilterLine(tString& line) noexcept override
    {
        line += "BAR";
    }
    virtual int DoGetPriority() const noexcept override
    {
        return 5;
    }
};
} // namespace

TEST_SUITE("tConsole")
{
    TEST_CASE("tConsoleFilter basic functionality")
    {
        MockConsole mockConsole;

        GIVEN("a tConsoleFilter")
        {
            tConsoleFilter filter;
            THEN("filter has a priority")
            {
                int priority = filter.GetPriority();
                CHECK(priority == 0); // default is 0
            }
        }

        GIVEN("a MockFilter")
        {
            MockFilter filter;
            THEN("filter has a priority")
            {
                int priority = filter.GetPriority();
                CHECK(priority == 5); // overridden
            }

            THEN("filter is applied")
            {
                con << "FOO\n";
#ifdef DEDICATED
                CHECK(mockConsole.GetLastPrinted() == "[0] FOOBAR\n");
#else
                CHECK(mockConsole.GetLastPrinted() == "FOOBAR\n");
#endif
            }
        }
    }

    TEST_CASE("tConsole basic functionality")
    {
        GIVEN("a replacement console")
        {
            MockConsole mockConsole;

            THEN("console can be used for output")
            {
                // Test that we can use the console for output
                con << "Test message\n";
#ifdef DEDICATED
                CHECK(mockConsole.GetLastPrinted() == "[0] Test message\n");
#else
                CHECK(mockConsole.GetLastPrinted() == "Test message\n");
#endif
            }

            THEN("Print method works")
            {
                tString testString("Test string\n");
                con.Print(testString);
#ifdef DEDICATED
                CHECK(mockConsole.GetLastPrinted() == "[0] Test string\n");
#else
                CHECK(mockConsole.GetLastPrinted() == "Test string\n");
#endif
            }
        }
    }
}

// Additional tests that could be added:
// 1. Output formatting with colors, prefixes, timestamps (requires setup)
// 2. Filter chain composition and ordering (requires multiple filters)
// 3. Thread safety testing (requires multi-threaded environment)
