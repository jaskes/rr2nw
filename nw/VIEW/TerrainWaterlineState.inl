double __terrainWaterline = 20;

double _CViewTerrain::Waterline() {
  return __terrainWaterline * m_fCellSize * m_fHScale;
}
