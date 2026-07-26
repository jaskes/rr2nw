#define LAST_H__VIEW
#include "game.h"

bool CLandDynamicMap::IsEmpty() {
  if (m_pOriginLight == 0) return true;

  for (int y = m_rect.top; y < m_rect.bottom; ++y) {
    for (int x = m_rect.left; x < m_rect.right; ++x) {
      if (m_pOriginLight[x + y * m_size.x] != 0) return false;
    }
  }
  return true;
}
