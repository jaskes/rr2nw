#define LAST_H__VIEW
#include "game.h"

void CViewFigure::SetHaze(SHazeDef& haze) {
  if (haze.nHazeDist <= 0) return;
  m_fHazeMin = haze.nHazeMin;
  m_fHazeMax = haze.nHazeMin + haze.nHazeDist;
  m_fHazeMult = 128.0 / (m_fHazeMax - m_fHazeMin);
  m_fHazeAdd = 200.0 - m_fHazeMin * m_fHazeMult;
  (void)GRSetHaze(haze.nHazeMin, haze.nHazeDist, &haze);
}
