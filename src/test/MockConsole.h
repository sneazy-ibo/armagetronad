#ifndef ArmageTron_MOCK_CONSOLE_H
#define ArmageTron_MOCK_CONSOLE_H

#include "defs.h"
#include "tConsole.h"

// registers itself, swallows log
// use when you want to capture console log
// or just suppress messages
class MockConsole final : public tConsole
{
public:
    MockConsole() noexcept : tConsole{}
    {
        RegisterBetterConsole(this);
    }

    tString const& GetLastPrinted() const noexcept
    {
        return lastPrinted_;
    }

private:
    tConsole& DoPrint(const tString& s) noexcept override
    {
        lastPrinted_ = s;
#ifdef DEDICATED
        // remove origin decorator
        if (lastPrinted_.StartsWith("[0] "))
            lastPrinted_ = lastPrinted_.SubStr(4);
#endif
        return *this;
    }

    tString lastPrinted_;
};

#endif