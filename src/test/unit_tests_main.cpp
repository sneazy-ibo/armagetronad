#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// hack to get sg_MoviePack linked
#include "gStuff.h"
void force_link()
{
    sg_MoviePack();
}
