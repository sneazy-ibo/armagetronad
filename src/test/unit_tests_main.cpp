#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "tConsole.h"
namespace
{
// registers itself, swallows log
class SilentConsole final : public tConsole
{
public:
    SilentConsole() noexcept : tConsole{}
    {
        RegisterBetterConsole(this);
    }

private:
    tConsole& DoPrint(const tString& s) noexcept override
    {
        return *this;
    }
};

static SilentConsole s_console;

}