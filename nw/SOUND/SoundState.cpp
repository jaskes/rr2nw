// RSX-independent sound configuration state used by modern core libraries.
#include <cmath>

#include "sound.h"

#include "SoundStateData.inl"

bool SoundState_SetMaximumDistance(double maximumDistance)
{
    if (!std::isfinite(maximumDistance) || maximumDistance <= 0.0)
        return false;
    const double squared = maximumDistance * maximumDistance;
    if (!std::isfinite(squared) || squared <= 0.0)
        return false;
    snd_distMax = maximumDistance;
    snd_distMax2 = squared;
    return true;
}
