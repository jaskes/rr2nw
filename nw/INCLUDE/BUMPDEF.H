#ifndef __BUMPDEF_H__
#define __BUMPDEF_H__

#include "debugext.h"
#include "mathlib.h"

enum {
  BF_NONE = 0,
  BF_BUMPSTATIC,
  BF_BUMPDYNAMIC,
  BF_BUMPSHOOT,
  BF_BUMPLAND,
  BF_BUMPSHELTER,

  BF1_ADVANCED = 1
};

struct SBonusDef;

struct SBumpDef0 {
#ifdef _DEBUG
  enum TYPE { T_BUMPDEF0, T_BUMPDEF, T_USER } eType;
#endif
  CFVector3 start, vel, vel1;
  double fTime;
  double fRadius;
  dword dwFlags1;
  SBumpDef0() { IF_DEBUG(eType = T_BUMPDEF0); }
};

struct SBumpDef : public SBumpDef0 {
 public:
  double fMass;
  int nBumpFlags;
  SBonusDef* pBonus;
  void* pBumpRef;
  SBumpDef() {
    IF_DEBUG(eType = T_BUMPDEF);
    dwFlags1 = 0;
  }
};

class CBumpFigures;
bool Bump(SBumpDef& def, SBumpDef& def1, int nFlag = BF_BUMPDYNAMIC);
bool Bump(SBumpDef& def, CBumpFigures& figs);

#endif  // __BUMPDEF_H__
