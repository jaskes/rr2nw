#ifndef RR2NW_TIME_RUNTIME_STATE_INL
#define RR2NW_TIME_RUNTIME_STATE_INL

#include <cmath>

#include "TimeRuntimeState.h"

namespace {
Session *rr2nw_active_session = NULL;

// Rendering and presentation are allowed to stall without advancing the
// legacy event graph by the complete wall-clock gap.  Vehicle physics already
// owns the same 50 ms ceiling; applying it at the timer sample prevents the
// event queue and physics clocks from diverging after an expensive frame.
const double rr2nw_max_timer_sample_ms = 50.0;
unsigned int rr2nw_clamped_timer_samples = 0;
double rr2nw_clamped_timer_seconds = 0.0;

double rr2nw_sample_timer_at_tick(a_TTimer *timer, DWORD tick)
{
    if (timer == NULL)
        return 0.0;
    double addTime = static_cast<double>(SUA_HostTickDelta(
        static_cast<std::uint32_t>(tick),
        static_cast<std::uint32_t>(timer->m_prevTime)));

    if (addTime > 2000.0)
    {
        timer->m_pauseTime += addTime / 1000.0;
        rr2nw_clamped_timer_seconds += addTime / 1000.0;
        ++rr2nw_clamped_timer_samples;
        addTime = 0;
    }
    else if (addTime > rr2nw_max_timer_sample_ms)
    {
        const double dropped = addTime - rr2nw_max_timer_sample_ms;
        timer->m_pauseTime += dropped / 1000.0;
        rr2nw_clamped_timer_seconds += dropped / 1000.0;
        ++rr2nw_clamped_timer_samples;
        addTime = rr2nw_max_timer_sample_ms;
    }
    timer->m_curTime += addTime;
    timer->m_prevTime = static_cast<long>(tick);
    return (timer->m_curTime / 1000.0 + 0.1) * timer->m_aspect;
}

void rr2nw_rebase_timer(a_TTimer *timer, double interval)
{
    const double unscaled = interval / timer->m_aspect;
    timer->m_curTime = (unscaled > 0.1 ? unscaled - 0.1 : 0.0) * 1000.0;
    const DWORD tick = ::GetTickCount();
    timer->m_prevTime = static_cast<long>(tick);
    // Rebase message timestamps without depending on how long the process was
    // paused between capture and restore.
    timer->m_startTick = static_cast<long>(tick);
    timer->m_pauseTime = -unscaled;
    timer->m_deltaTime = 0;
}
}

bool a_TTimer::dump(PIN_SaveFile &sf)
{
    m_deltaTime = static_cast<long>(
        static_cast<DWORD>(m_prevTime) - static_cast<DWORD>(m_startTick));

    if (!sf.WriteData((char *)&m_aspect, sizeof(TimerData)))
        return false;

    return true;
}

bool a_TTimer::load(PIN_SaveFile &sf)
{
    if (!sf.GetData((char *)&m_aspect, sizeof(TimerData)))
        return false;

    m_prevTime = static_cast<long>(::GetTickCount());
    m_startTick = static_cast<long>(
        static_cast<DWORD>(m_prevTime) - static_cast<DWORD>(m_deltaTime));

    return true;
}

void a_TTimer::Start()
{
    m_startTick = ::GetTickCount();
    m_prevTime = m_startTick;
    m_curTime = 0;
    m_pauseTime = 0;
    rr2nw_clamped_timer_samples = 0;
    rr2nw_clamped_timer_seconds = 0.0;
    Session::m_moment = GetTime();
    Session::m_viewTime = Session::m_moment;
    Session::m_frameSec = 0.0;
    Session::m_simulationTick = 0;
}

void a_TTimer::Wait(double)
{
}

void a_TTimer::SetCurTime(double)
{
}

double a_TTimer::ConvertSysTime(dword tick)
{
    // The recovered Windows adapter and the retained Hardware mouse path must
    // share one event clock. A bound Session is authoritative and cannot be
    // displaced by the signed 24.8-day boundary or 49.7-day host wrap.
    if (rr2nw_active_session != NULL)
        return SUA_AuthoritativeInputTime();

    // The unbound compatibility path is still used by isolated legacy tests.
    // Its start-relative interval is valid across one DWORD wrap as long as
    // callers observe the original less-than-one-wrap contract.
    double t = static_cast<double>(SUA_HostTickDelta(
        static_cast<std::uint32_t>(tick),
        static_cast<std::uint32_t>(m_startTick))) / 1000.0 - m_pauseTime;
    if (t < 0.1)
        t = 0.1;
    return t * m_aspect;
}

void a_TTimer::addTime(double t)
{
    m_curTime += t;
    m_startTick += (int)(t * 1000);
}

double a_TTimer::GetTime()
{
    return rr2nw_sample_timer_at_tick(this, ::GetTickCount());
}

std::uint32_t SUA_HostTickDelta(std::uint32_t currentTick,
                                std::uint32_t previousTick)
{
    return currentTick - previousTick;
}

bool SUA_SampleLegacyTimerAtHostTick(std::uint32_t hostTick,
                                     double *timeSeconds)
{
    if (timeSeconds == NULL)
        return false;
    *timeSeconds = rr2nw_sample_timer_at_tick(
        &g_timer, static_cast<DWORD>(hostTick));
    return std::isfinite(*timeSeconds) && *timeSeconds >= 0.1;
}

double SUA_AuthoritativeInputTime()
{
    const double eventMoment = Session::m_moment;
    return !std::isfinite(eventMoment) || eventMoment < 0.1
        ? 0.1 : eventMoment;
}

unsigned int SUA_ClampedTimerSampleCount()
{
    return rr2nw_clamped_timer_samples;
}

double SUA_ClampedTimerSeconds()
{
    return rr2nw_clamped_timer_seconds;
}

SSimulationClockState::SSimulationClockState()
    : tick(0), eventMoment(0.0), viewTime(0.0), frameSeconds(0.0),
      timerAspect(1.0), clampedSamples(0), clampedSeconds(0.0)
{
}

bool SUA_ValidateSimulationClock(const SSimulationClockState &state)
{
    return std::isfinite(state.eventMoment) && state.eventMoment >= 0.0 &&
           std::isfinite(state.viewTime) && state.viewTime >= 0.0 &&
           std::isfinite(state.frameSeconds) && state.frameSeconds >= 0.0 &&
           state.frameSeconds <= 0.1 &&
           std::isfinite(state.timerAspect) && state.timerAspect > 0.0 &&
           std::isfinite(state.viewTime / state.timerAspect) &&
           std::isfinite(state.clampedSeconds) &&
           state.clampedSeconds >= 0.0;
}

bool SUA_CaptureSimulationClock(SSimulationClockState *state)
{
    if (state == NULL)
        return false;
    state->tick = Session::m_simulationTick;
    state->eventMoment = Session::m_moment;
    state->viewTime = Session::m_viewTime;
    state->frameSeconds = Session::m_frameSec;
    state->timerAspect = g_timer.m_aspect;
    state->clampedSamples = rr2nw_clamped_timer_samples;
    state->clampedSeconds = rr2nw_clamped_timer_seconds;
    return SUA_ValidateSimulationClock(*state);
}

bool SUA_ApplySimulationClock(const SSimulationClockState &state)
{
    if (!SUA_ValidateSimulationClock(state))
        return false;
    g_timer.m_aspect = state.timerAspect;
    rr2nw_rebase_timer(&g_timer, state.viewTime);
    Session::m_simulationTick = state.tick;
    Session::m_moment = state.eventMoment;
    Session::m_viewTime = state.viewTime;
    Session::m_frameSec = state.frameSeconds;
    rr2nw_clamped_timer_samples = state.clampedSamples;
    rr2nw_clamped_timer_seconds = state.clampedSeconds;
    return true;
}

bool SUA_SimulationClockMatches(const SSimulationClockState &state)
{
    SSimulationClockState current;
    return SUA_CaptureSimulationClock(&current) &&
           current.tick == state.tick &&
           current.eventMoment == state.eventMoment &&
           current.viewTime == state.viewTime &&
           current.frameSeconds == state.frameSeconds &&
           current.timerAspect == state.timerAspect &&
           current.clampedSamples == state.clampedSamples &&
           current.clampedSeconds == state.clampedSeconds;
}

double a_TTimer::GetTimeDiff(double timeStamp)
{
    return GetTime() - timeStamp;
}

a_TTimer g_timer;

void SUA_BindSession(Session *session)
{
    rr2nw_active_session = session;
}

void SUA_ProcessEvents()
{
    if (rr2nw_active_session != NULL)
        rr2nw_active_session->poll();
}

bool SUA_ProcessEventsAt(double viewTime)
{
    return rr2nw_active_session != NULL &&
           rr2nw_active_session->pollAt(viewTime) != 0;
}

void SUA_SkipTime(double t)
{
    g_timer.addTime(t);
    SUA_ProcessEvents();
}

#endif
