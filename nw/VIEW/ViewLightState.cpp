#define LAST_H__VIEW
#include "game.h"

void CViewObject::SetLight(int index, int color, TCSFVector3& position,
                           double radius, int brightness) {
  if (index < 0 || index >= LIGHT_SOURCE_COUNT || radius < 0.0) return;

  m_aGlobalLights[index] = position;
  m_aGlobalLightsRect[index].left =
      static_cast<int>((position.x - radius) * m_terrainScale_1 - 0.5);
  m_aGlobalLightsRect[index].top =
      static_cast<int>((-position.z - radius) * m_terrainScale_1 - 0.5);
  m_aGlobalLightsRect[index].right =
      static_cast<int>((position.x + radius) * m_terrainScale_1 + 0.5);
  m_aGlobalLightsRect[index].bottom =
      static_cast<int>((-position.z + radius) * m_terrainScale_1 + 0.5);
  _gr_pLights[index].r = static_cast<float>(radius);
  _gr_pLights[index].power0 = brightness;
  _gr_pLights[index].color = color;
}
