// Fountain.cpp is a CP866 archival source file. Keep it byte-identical while
// making its class-table registrars and explicit recovered linker anchor one
// indivisible object file.
#include "Fountain.cpp"

#include "FountainClassTableState.h"

void FountainClassTable_Link() {}

bool FountainClassTable_EnsurePortalArabesk(SimulationContext *context,
                                            double timeStamp) {
  if (context == 0 || g_arena.getContext() != context)
    return false;
  if (context->isExist("Portal.Arabesk"))
    return true;
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("Fountain");
  if (table == ct_NULLID || !context->isExist("Fount.Attr.Arab"))
    return false;

  const KR_ObjectID attribute = context->searchObject("Fount.Attr.Arab");
  KR_ObjectID object =
      g_arena.newObject(table, "Portal.Arabesk");
  if (object.isNUL())
    return false;

  KR_Event event;
  event.label = fou_EVCMD_START;
  event.source = g_arena.getObjectID();
  event.destination = object;
  event.timeStamp = timeStamp < 0.1 ? 0.1 : timeStamp;
  event.data.open(EDO_WRITE)
      .putObjectID(attribute)
      .putDouble(2514.84)
      .putDouble(63.1582)
      .putDouble(-2306.53)
      .close();
  context->sendEventNow(event);
  return context->isExist("Portal.Arabesk");
}
