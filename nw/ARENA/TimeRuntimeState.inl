#ifndef RR2NW_TIME_RUNTIME_STATE_INL
#define RR2NW_TIME_RUNTIME_STATE_INL

namespace {
Session *rr2nw_active_session = NULL;
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
    long t = ::GetTickCount();
    double addTime = t - m_prevTime;

    if (addTime > 2000.0)
    {
        m_pauseTime += addTime / 1000.0;
        addTime = 0;
    }

    m_curTime += addTime;
    m_prevTime = t;
    return (m_curTime / 1000.0 + 0.1) * m_aspect;
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
