#ifndef ArmageTron_STATICS_H
#define ArmageTron_STATICS_H

#include "defs.h"
#include "ePlayer.h"
#include "tLocale.h"

#include <tuple>

inline bool InitStaticsCore()
{
    ePlayer::Init();
    tLocale::Load("languages.txt");

    return true;
}

// call to make sure all static data is initialized
inline void InitStatics()
{
    static bool done = InitStaticsCore();
    std::ignore = done;
}

#endif