#ifndef RR2NW_ACTOR_CADENCE_H
#define RR2NW_ACTOR_CADENCE_H

#include <cmath>
#include <limits>

namespace rr2nw {

// Preserve the retail 0.2 near bias and 2.0 far cadence without coupling
// simulation frequency to whether the actor happened to render last frame.
inline double ActorCadenceScale(double distance, double hazeDistance)
{
    if (!std::isfinite(distance) || !std::isfinite(hazeDistance) ||
        hazeDistance <= 1.0e-6)
        return 1.0;

    double scale = std::fabs(distance) / hazeDistance + 0.2;
    if (scale < 0.2)
        return 0.2;
    if (scale > 2.0)
        return 2.0;
    return scale;
}

// The normalized age remains useful to callers that need a bounded sample
// phase. It never authorizes movement beyond the latest simulation sample.
inline double ActorPresentationRatio(double renderTime, double sampleTime,
                                     double sampleInterval,
                                     double maxSampleInterval)
{
    if (!std::isfinite(renderTime) || !std::isfinite(sampleTime) ||
        !std::isfinite(sampleInterval) ||
        !std::isfinite(maxSampleInterval) || sampleInterval <= 1.0e-3 ||
        sampleInterval >= maxSampleInterval)
        return 0.0;

    const double age = renderTime - sampleTime;
    if (!std::isfinite(age) || age <= 0.0)
        return 0.0;

    const double ratio = age / sampleInterval;
    return ratio < 1.0 ? ratio : 1.0;
}

inline double ActorPresentationRatio(double renderTime, double sampleTime,
                                     double sampleInterval)
{
    return ActorPresentationRatio(renderTime, sampleTime, sampleInterval,
                                  (std::numeric_limits<double>::max)());
}

// Render one confirmed simulation step behind the authoritative pose and
// blend from the previous sample to the current one. This is continuous at
// event boundaries: the end of sample N and the beginning of sample N+1 both
// present sample N. Invalid, zero-length or deliberately unsupported samples
// fall back to the authoritative current pose (zero previous-pose weight).
inline double ActorPresentationPreviousWeight(
    double renderTime, double sampleTime, double sampleInterval,
    double maxSampleInterval)
{
    if (!std::isfinite(renderTime) || !std::isfinite(sampleTime) ||
        !std::isfinite(sampleInterval) ||
        !std::isfinite(maxSampleInterval) || sampleInterval <= 1.0e-3 ||
        sampleInterval >= maxSampleInterval)
        return 0.0;

    const double age = renderTime - sampleTime;
    if (!std::isfinite(age))
        return 0.0;
    return 1.0 - ActorPresentationRatio(
        renderTime, sampleTime, sampleInterval, maxSampleInterval);
}

inline double ActorPresentationPreviousWeight(
    double renderTime, double sampleTime, double sampleInterval)
{
    return ActorPresentationPreviousWeight(
        renderTime, sampleTime, sampleInterval,
        (std::numeric_limits<double>::max)());
}

} // namespace rr2nw

#endif
