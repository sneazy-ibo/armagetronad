#include "doctest.h"
#include "eTeam.h"

#include "MockConsole.h"

// Tests for eTeam system
// Purpose: Document the status quo behavior and detect regressions

TEST_SUITE("eTeam")
{
    TEST_CASE("eTeam base tests")
    {
        MockConsole mockConsole; // swallow log output

        GIVEN("eTeam")
        {
            auto team = tRefPtr<eTeam>::Make();

            THEN("it can take up one ePlayerNetID")
            {
                auto player = tRefPtr<ePlayerNetID>::Make();
                player->SetName("Z");
                team->AddPlayer(player);

                AND_THEN("it takes the player's name")
                {
                    CHECK(team->Name().StartsWith("Z"));
                }
            }
        }
    }
}
