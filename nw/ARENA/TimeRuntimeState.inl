#ifndef RR2NW_TIME_RUNTIME_STATE_INL
#define RR2NW_TIME_RUNTIME_STATE_INL

namespace {
Session *rr2nw_active_session = NULL;

// Rendering and presentation are allowed to stall without advancing the
// legacy event graph by the complete wall-clock gap.  Vehicle physics already
// owns the same 50 ms ceiling; applying it at the timer sample prevents the
// event queue and physics clocks from diverging after an expensive frame.
const double rr2nw_max_timer_sample_ms = 50.0;
unsigned int rr2nw_clamped_timer_samples = 0;
double rr2nw_clamped_timer_seconds = 0.0;
}

bool a_TTimer::dump(PIN_SaveFile &sf)
{
    m_deltaTime = m_prevTime - m_startTick;

    if (!sf.WriteData((char *)&m_aspect, sizeof(TimerData)))
        return false;

    return true;
}

bool a_TTimer::load(PIN_SaveFile &sf)
{
    if (!sf.GetData((char *)&m_aspect, sizeof(TimerData)))
        return false;

    m_prevTime = ::GetTickCount();
    m_startTick = m_prevTime - m_deltaTime;

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
}

void a_TTimer::Wait(double)
{
}

void a_TTimer::SetCurTime(double)
{
}

double a_TTimer::ConvertSysTime(dword tick)
{
    double t = (double)(((long)tick) - m_startTick) / 1000.0 - m_pauseTime;
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
    const DWORD tick = ::GetTickCount();
    // DWORD subtraction has the modulo-2^32 behavior required by
    // GetTickCount.  Subtracting its signed long aliases would overflow at the
    // 0x7fffffff boundary, weeks before the documented 49-day wrap.
    double addTime = static_cast<double>(
        tick - static_cast<DWORD>(m_prevTime));

    if (addTime > 2000.0)
    {
        m_pauseTime += addTime / 1000.0;
        rr2nw_clamped_timer_seconds += addTime / 1000.0;
        ++rr2nw_clamped_timer_samples;
        addTime = 0;
    }
    else if (addTime > rr2nw_max_timer_sample_ms)
    {
        const double dropped = addTime - rr2nw_max_timer_sample_ms;
        m_pauseTime += dropped / 1000.0;
        rr2nw_clamped_timer_seconds += dropped / 1000.0;
        ++rr2nw_clamped_timer_samples;
        addTime = rr2nw_max_timer_sample_ms;
    }
    m_curTime += addTime;
    m_prevTime = static_cast<long>(tick);
    return (m_curTime / 1000.0 + 0.1) * m_aspect;
}

unsigned int SUA_ClampedTimerSampleCount()
{
    return rr2nw_clamped_timer_samples;
}

double SUA_ClampedTimerSeconds()
{
    return rr2nw_clamped_timer_seconds;
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

void SUA_SkipTime(double t)
{
    g_timer.addTime(t);
    SUA_ProcessEvents();
}

#endif
