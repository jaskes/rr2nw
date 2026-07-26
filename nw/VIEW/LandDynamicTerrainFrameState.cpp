#define LAST_H__VIEW
#include "game.h"

void CLandDynamicMap::SetupLandSticks(CViewDynamicList& list,
                                      _CViewTerrain& terrain) {
  if (m_pOrigin == 0) return;
  for (CViewDynamic* dynamic = list.First(), *next;
       dynamic != 0; dynamic = next) {
    next = dynamic->Next();
    if (dynamic->IsBumpDynamic()) continue;
    CVector2 cell = dynamic->m_cell;
    if (!terrain.FitInTrapezioid(cell) || !m_rect.Contains(cell)) {
      list.Remove(dynamic);
    } else {
      m_pOrigin[cell.x + cell.y * m_size.x].Move(dynamic, list);
      dynamic->m_cell = cell;
    }
  }
}
