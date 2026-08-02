#ifndef __RECRCENMSG_H__INCLUDED
#define __RECRCENMSG_H__INCLUDED

#ifndef __A_MSG_H__
#include "message/a_msg.h"
#endif

enum
 {
    rc_CREATE = RECRUITCENTER_MESSAGE,
    rc_SET_EJECT,
    rc_CHECK_MISSION,
    rc_NEW_MISSION,
    rc_RESERVED_39004,
    rc_SET_VIDEO,
    rc_SET_DEFTAXI,
    rc_SET_DICTIONARY,
    rc_RESERVED_39008,
 };

#endif

/* End of file bimsg.h */
