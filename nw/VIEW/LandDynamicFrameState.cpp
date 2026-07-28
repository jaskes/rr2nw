#define LAST_H__VIEW
#include "game.h"

bool CLandDynamicMap::IsEmpty() {
  if (m_pOrigin == 0 || m_pOriginLight == 0) {
    return m_pOrigin == 0 && m_pOriginLight == 0;
  }

  for (int y = m_rect.top; y < m_rect.bottom; ++y) {
    for (int x = m_rect.left; x < m_rect.right; ++x) {
      const int index = x + y * m_size.x;
      if (m_pOrigin[index].First() != 0 || m_pOriginLight[index] != 0) {
        return false;
      }
    }
  }
  return true;
}

void CLandDynamicMap::SetupLights(dword lights) {
  if (m_pOriginLight == 0) return;
  for (dword index = 0, mask = 1;
       lights != 0 && index < LIGHT_SOURCE_COUNT;
       lights >>= 1, mask <<= 1, ++index) {
    if ((lights & 1) == 0) continue;
    SRect2 cell;
    cell.DoSetIntersect(m_rect, CViewObject::m_aGlobalLightsRect[index]);
    for (int y = cell.top; y < cell.bottom; ++y) {
      for (int x = cell.left; x < cell.right; ++x) {
        m_pOriginLight[x + y * m_size.x] |= mask;
      }
    }
  }
}

void CLandDynamicMap::RemoveLights(dword lights) {
  if (m_pOriginLight == 0) return;
  for (dword index = 0; lights != 0 && index < LIGHT_SOURCE_COUNT;
       lights >>= 1, ++index) {
    if ((lights & 1) == 0) continue;
    SRect2 cell;
    cell.DoSetIntersect(m_rect, CViewObject::m_aGlobalLightsRect[index]);
    for (int y = cell.top; y < cell.bottom; ++y) {
      for (int x = cell.left; x < cell.right; ++x) {
        m_pOriginLight[x + y * m_size.x] = 0;
      }
    }
  }
}

void CLandDynamicMap::ApplyLights(CViewDynamic* dynamic) {
  if (dynamic == 0 || m_pOriginLight == 0) return;
  if (!dynamic->IsBumpDynamic()) {
    if (m_rect.Contains(dynamic->m_cell)) {
      dynamic->m_dwLights =
          m_pOriginLight[dynamic->m_cell.x + dynamic->m_cell.y * m_size.x];
    }
    return;
  }

  SRect2 cell;
  CViewSphericDynamic* spheric =
      static_cast<CViewSphericDynamic*>(dynamic);
  CViewBumpDynamic* bump = static_cast<CViewBumpDynamic*>(dynamic);
  cell.left = static_cast<int>(
      (spheric->m_dynBase.x - bump->m_bump.fRadius) * m_f_1_CellSize - 0.5);
  cell.top = static_cast<int>(
      (-spheric->m_dynBase.z - bump->m_bump.fRadius) * m_f_1_CellSize - 0.5);
  cell.right = static_cast<int>(
      (spheric->m_dynBase.x + bump->m_bump.fRadius) * m_f_1_CellSize + 0.5);
  cell.bottom = static_cast<int>(
      (-spheric->m_dynBase.z + bump->m_bump.fRadius) * m_f_1_CellSize + 0.5);
  cell.DoSetIntersect(cell, m_rect);
  dynamic->m_dwLights = 0;
  // Preserve the recovered cursor mutation: left is not reset for each row.
  for (; cell.top < cell.bottom; ++cell.top) {
    for (; cell.left < cell.right; ++cell.left) {
      dynamic->m_dwLights |=
          m_pOriginLight[cell.left + cell.top * m_size.x];
    }
  }
  dynamic->m_dwLights &= dynamic->m_dwLightMask;
}
