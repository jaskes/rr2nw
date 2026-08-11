#ifndef RR2NW_FOUNTAIN_SUBJECT_STATE_H
#define RR2NW_FOUNTAIN_SUBJECT_STATE_H

#include <cstring>

#include "kernel/h/context.h"
#include "storage/h/subject.h"

// The archived Fountain::addNotify calls ct_Object::addNotify directly, so
// the presentation-owned visibility fields that ct_Subject normally clears
// retain allocator/reuse bytes. Normalize only those two fields before the
// authored START event; broader Fountain cache/save ownership remains separate.
inline void FountainSubjectState_ResetPendingPresentation(SubjectData* state) {
  if (state == nullptr) return;
  state->m_audibleThisFrame = 0;
  state->m_isVisible = 0;
}

inline bool FountainSubjectState_PrepareNewObject(
    SimulationContext* context, KR_ObjectID object) {
  if (context == nullptr || object.isNUL()) return false;
  KR_Object* base = static_cast<KR_Object*>(
      context->queryInterface(object, IUnknownIID));
  if (base == nullptr || base->m_tableName == nullptr ||
      std::strcmp(base->m_tableName, "Fountain") != 0) {
    return false;
  }
  ct_Object* stored = static_cast<ct_Object*>(base);
  ct_Subject* subject = static_cast<ct_Subject*>(stored);
  FountainSubjectState_ResetPendingPresentation(subject);
  return true;
}

#endif
