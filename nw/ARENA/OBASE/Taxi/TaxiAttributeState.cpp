#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "Taxi.h"

#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "message/skinmsg.h"
#include "storage/h/subject.h"

// Extracted verbatim in behavior from Taxi.cpp so Vehicle can link the
// attribute registry without constructing the full renderer-backed Taxi class.
AttributeTableTaxi __attrTaxiTable;

void AttributeTableTaxi::allocObjects(int objectQnty)
{
    m_table = new AttributeTaxi[objectQnty];

    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableTaxi::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableTaxi::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTable::getObjectPTR");
    return &(m_table[index]);
}

void AttributeTaxi::update(double ts)
{
    KR_ObjectID skinID = context->searchObject(m_skinName);
    s_ASSERT(!skinID.isNUL(), "AttributeTaxi::update");

    m_skinID = skinID;

    KR_Event event;
    event.timeStamp = ts;
    event.label = sk_EV_QUERY_MODEL_PTR;
    event.destination = skinID;
    context->sendEventNow(event);
    s_ASSERT(event.label == sk_EV_QUERY_MODEL_PTR_OK, "AttributeTaxi::update");
    event.data.open(EDO_READ)
              .get(&m_cacheSkin, sizeof(void *))
              .close();

    m_attrForVehicle = context->searchObject(m_attrForVehicleName);

    m_cacheCorpseTable = g_arena.searchSeanceClassTable("Corpse");
    m_cacheCorpseAttr = g_arena.getAttributeIndex(
        g_arena.searchSeanceClassTable("CorpseAttr"),
        context->searchObject(m_corpseAttrName));
}
