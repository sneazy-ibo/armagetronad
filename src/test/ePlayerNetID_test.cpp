#include "doctest.h"
#include "ePlayer.h"

#include "tDefer.h"

// not needed here, maybe for some other place
/*
#include "gAIBase.h"
#include "tToDo.h"
namespace
{
void Cleanup()
{
    sn_SetNetState(nSTANDALONE);

    st_DoToDo();

    // delete all the things
    nNetObject::SyncAll();
    gAIPlayer::ClearAll();
    nNetObject::ClearAll();
    ePlayerNetID::ClearAll();
    nNetObject::ClearAllDeleted();

    st_DoToDo();
}
} // namespace
*/

// Tests for ePlayerNetID system
// Purpose: Document the status quo behavior and detect regressions

TEST_SUITE("ePlayerNetID")
{
    TEST_CASE("ePlayerNetID basics")
    {
        // check that player gets destroyed at the end
        INVARIANT_CHECK(!se_PlayerNetIDs.Len())
        // INVARIANT(Cleanup();)

        GIVEN("an ePlayerNetID for an existing player")
        {
            // you can spawn players just by creating them,
            // store the result in a smart pointer for later cleanup.
            // This constructor creates a player bound to local player 0.
            auto player = tRefPtr<ePlayerNetID>::Make(0);

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

            THEN("player has a network ID")
            {
                CHECK(player->ID() > 0);
            }
        }
    }
}
