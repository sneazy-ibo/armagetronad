#include "doctest.h"
#include "ePlayer.h"

#include "tDefer.h"

// Tests for ePlayerNetID system
// Purpose: Document the status quo behavior and detect regressions

TEST_SUITE("ePlayerNetID")
{
    TEST_CASE("ePlayerNetID basics")
    {
        // check that player gets destroyed at the end
        tRefPtr<nObserver> observer;
        // INVARIANT_CHECK(!observer.get()); // does not work yet, objects (temporarily) leak because the network system knows about them.

        GIVEN("an ePlayerNetID for an existing player")
        {
            // you can spawn players just by creating them,
            // store the result in a smart pointer for later cleanup.
            // This constructor creates a player bound to local player 0.
            auto player = tRefPtr<ePlayerNetID>::Make(0);

            // observe player
            observer = &player->GetObserver();
            player->ReleaseOwnership();

            THEN("player has a name")
            {
                tString name;
                player->GetName(name);
                CHECK(name.Len() > 2);
            }

            THEN("player has a network ID")
            {
                CHECK(player->ID() > 0);
            }
        }

        GIVEN("an ePlayerNetID for a remote player")
        {
            // This constructor creates a player not bound to a local player.
            auto player = tRefPtr<ePlayerNetID>::Make();

            // observe player
            observer = &player->GetObserver();

            THEN("player has a network ID")
            {
                CHECK(player->ID() > 0);
            }
        }
    }
}
