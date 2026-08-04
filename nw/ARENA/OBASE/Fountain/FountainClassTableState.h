#ifndef RR2NW_FOUNTAIN_CLASS_TABLE_STATE_H
#define RR2NW_FOUNTAIN_CLASS_TABLE_STATE_H

// Pulls the archived Fountain/FountainAttr table registrars into the recovered
// game-services executable. Static-library registration alone is not enough:
// without an explicit reference the linker may discard Fountain.cpp.
void FountainClassTable_Link();

class SimulationContext;

// Recreates the one May-authored Portal barrier from its already published
// Fountain/FountainAttr tables. This is used only to reconcile derived Portal
// presentation after an older partial-occupancy continuation is applied.
bool FountainClassTable_EnsurePortalArabesk(SimulationContext *context,
                                            double timeStamp);

#endif
