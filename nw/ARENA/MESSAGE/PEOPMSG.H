#ifndef __PEOPMSG_H__INCLUDED
#define __PEOPMSG_H__INCLUDED

#include "a_msg.h"

enum
 {
    p_EV_PEOPLE_FIRST_MESSAGE = PEOPLE_MESSAGE,
    //{{EVENT
    pe_EVCMD_START,
    pe_EVCMD_STOP,
    pe_EVC_MOVE,
    //}}END_OF_EVENT
    pe_EVC_NEXTNODE,
    pe_EV_SETANIM,
    pe_EVC_FIND_ENEMY,
    pe_EVC_NEXTRNDNODE,
    pe_EV_STARTSHOW,
    pe_EV_SETAUTOANIM,
    // May retail SYSF.SCI extended start payload.  Appended to preserve every
    // January event value used by the recovered source tree.
    pe_EVCMD_START_EX,
    // May retail's private transition from a visible, stationary subject to
    // active route movement.  CreateManEx schedules it after startMoveTime.
    pe_EV_STARTMOVE,
    // May retail's grounded route-progress event. Land, water and
    // object-following People use this instead of the January NEXTNODE path.
    pe_EVC_GROUNDED_NEXTNODE,
 };

#endif
