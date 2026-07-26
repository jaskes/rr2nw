#ifndef RR2NW_TERRAIN_VIEW_STATE_H
#define RR2NW_TERRAIN_VIEW_STATE_H

struct SVector2;

bool TerrainView_FitCell(bool terrainInView,
                         const long polygonRect[4],
                         const int (*edges)[2],
                         int primaryAxis,
                         bool primaryReverse,
                         bool secondaryReverse,
                         SVector2& cell);

#endif  // RR2NW_TERRAIN_VIEW_STATE_H
