#include <cstdio>

#include "h/super.h"

KR_ActiveObject::StateTransitionTableElem a_TObserver::m_table[1] = {
    {KR_WAKE_UP, EVENT_IS_IGNORED}};

KR_ActiveObject::StateElem a_TObserver::m_state[1] = {
    KR_ActiveObject::StateElem(1, &(a_TObserver::m_table[0]))};

void a_TObserver::draw(CDC& gc) {
  for (ct_SubjectTable* table = m_arena->findFirstSubjectTable();
       table != nullptr; table = m_arena->findNextSubjectTable(table)) {
    for (ct_Subject* subject = table->findFirstSubject(); subject != nullptr;
         subject = table->findNextSubject(subject)) {
      subject->draw(gc);
    }
  }
}

void a_TObserver::loadStateTransitionTable() {
  std::printf("Observer::loadStateTransitionTable");
  m_stateQnty = 1;
  m_stateTable = m_state;
  m_currentState = 0;
}

Supervisor g_super;
