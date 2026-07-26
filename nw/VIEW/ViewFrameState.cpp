#define LAST_H__VIEW
#include "game.h"

CFVector3 CViewOrder::m_currOrdViewPoint;
bool CViewOrder::m_bIsGlobalOrder = TRUE;
TCSFVector3* CViewOrder::m_pLights = 0;

void CViewObject::SetOrderViewPointToGlobal() {
#if _VIEW_ORTHO
  if (!m_bViewOrtho) {
#endif
    CViewOrder::SetOrderViewPoint(
        CFVector3(m_viewPointInvMx.m[0][3], m_viewPointInvMx.m[1][3],
                  m_viewPointInvMx.m[2][3]));
#if _VIEW_ORTHO
  } else {
    CViewOrder::SetOrderViewPoint(
        CFVector3(m_viewPointInvMx.m[0][2], m_viewPointInvMx.m[1][2],
                  m_viewPointInvMx.m[2][2]) * m_viewPointScale.z);
  }
#endif
}

void CViewObject::BeginDraw() {
  SetOrderViewPointToGlobal();
  SetLightsToGlobal();
  CViewOrder::SetGlobalOrder(TRUE);
  for (dword index = 0, lights = m_dwLights;
       index < LIGHT_SOURCE_COUNT && lights != 0;
       ++index, lights >>= 1) {
    if ((lights & 1) != 0) {
      _gr_pLights[index].Set(m_viewPointDirMx * m_aGlobalLights[index]);
    }
  }
}

void CViewObject::SetClipRect(TCCRect2& rect) {
  m_clipRect2.left = (m_clipRect.left = rect.left) * rect.left;
  m_clipRect2.right = (m_clipRect.right = rect.right) * rect.right;
  m_clipRect2.top = (m_clipRect.top = rect.top) * rect.top;
  m_clipRect2.bottom = (m_clipRect.bottom = rect.bottom) * rect.bottom;
}

void CViewObject::SetClipPlanes(double front, double back) {
  if (front <= 0.0 || back <= front) return;
  const int zShift = Round(floor(log(front) / log(2.0)));
  if (zShift < -16 || zShift > 14) return;
  m_fFrontClip = front;
  m_f1FrontClip = 1.0 / front;
  m_fBackClip = back;
  m_f1BackClip = 1.0 / back;
  m_z1MinShift = zShift;
  m_fz1MinScale = static_cast<double>(1u << (m_z1MinShift + 16));
  _gr_fFrontClip = static_cast<float>(front);
}

void CViewDynamicList::SortZOrder() {
  if (m_pFirst == m_pLast) return;
  CViewDynamic* dynamic = 0;
  CViewDynamicList linears;
  for (CViewDynamic* current = m_pFirst, *next; current != 0;
       current = next) {
    next = current->Next();
    switch (current->m_eDType) {
      case CViewDynamic::DT_LINEAR:
        linears.Move(current, *this);
        break;
      case CViewDynamic::DT_SPHERIC:
        static_cast<CViewSphericDynamic*>(current)->m_fSortValue =
            Abs2(static_cast<CViewSphericDynamic*>(current)->m_dynBase -
                 CViewOrder::m_currOrdViewPoint);
        break;
      default:
        ASSERT(0);
    }
  }

  CViewDynamicList pending;
  pending.Move(*this);
  for (;;) {
    CViewDynamic* best = pending.m_pFirst;
    if (best == 0) break;
    if (best->IsBumpDynamic()) {
      for (dynamic = best->m_pNext; dynamic != 0;
           dynamic = dynamic->m_pNext) {
        if (static_cast<CViewBumpDynamic*>(dynamic)->m_fSortValue >
            static_cast<CViewBumpDynamic*>(best)->m_fSortValue) {
          best = dynamic;
        }
      }
    }
    Move(best, pending);
  }
  Append(linears);
}
