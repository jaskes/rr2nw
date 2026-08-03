#ifndef RR2NW_HOWITZER_BULLET_COMPATIBILITY_SHIM_H
#define RR2NW_HOWITZER_BULLET_COMPATIBILITY_SHIM_H

// Howitzer.cpp includes the January BULLET.H only to inspect a BulletAttr.
// That header defines AttributeBullet's constructor inline, while the modern
// recovered Bullet runtime owns the single out-of-line definition.  Suppress
// the obsolete duplicate and expose the ABI-compatible modern declaration.
#define __BULLET_H__INCLUDED
#include "obase/bullet/BulletAttributeState.h"

#endif
