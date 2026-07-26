// Renderer-independent DebugMap lifetime and bounded mission pool.
#include <cstring>

#include "graph.h"
#include "dmap.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/hardmsg.h"

#include "DebugMapMissionState.inl"
#include "DebugMapMissionEvents.inl"
#include "DebugMapGlobal.inl"
