/*
 * File  : C:\NW\ARENA\OBASE\Smoke\smoker.cpp
 * Autor :
 * Ver   1.0 
 */
#include <cmath>

#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "graph.h"
#include "smoker.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/fountmsg.h"
#include "message/smokermsg.h"
#include "h/light.h"
#include "h/phisics.h"
#include "h/cachesmoke.h"
#include "SmokerAttributeState.h"
#include "SmokeSubjectState.h"
#include "SmokerSubjectState.h"

 //===========================================================================
class SmokerTable : public ct_SubjectTable
{
 private:
    Smoker *m_table;
 public:
    SmokerTable()
    {
        m_table = NULL;
        registerClass( "Smoker" );
    }

    SmokerTable(const char * name)
    {
        m_table = NULL;
        registerClass(name);
    }

    ~SmokerTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    virtual  bool      isRendering();
    int                capacity() const { return m_maxObjectQnty; }
};

 //===========================================================================
bool SmokerTable::isRendering()
{
     return true;
}

static SmokerTable  __classTable;
static SmokerTable  __classTable2("DynSmoker");

#ifndef RR2NW_SMOKER_ATTRIBUTE_STATE_EXTERNAL
#include "SmokerAttributeState.inl"
#endif

static bool SmokerTerrainReady(const AttributeSmoker *attr)
{
    if (attr == NULL || !attr->m_onLand)
        return attr != NULL;
    CViewScene *scene = CViewScene::Current();
    return scene != NULL && scene->GetTerrain() != NULL;
}

static bool SmokerPlaceOnTerrain(const AttributeSmoker *attr,
                                 CFVector3 &position)
{
    if (!SmokerTerrainReady(attr))
        return false;
    if (!attr->m_onLand)
        return true;
    double height;
    CFVector3 normal;
    CViewScene::Current()->GetTerrain()->GetPlane(position, normal, height);
    position.y = height;
    return true;
}

static bool SmokerCanEmit(SimulationContext *context,
                          AttributeSmoker *attr)
{
    if (context == NULL || attr == NULL || attr->m_createIncMin <= 0 ||
        attr->m_createIncMax < attr->m_createIncMin ||
        attr->m_smokeTableID == ct_NULLID ||
        attr->m_smokeAttrID.isNUL() || !SmokerTerrainReady(attr))
        return false;
    const ct_ClassTableID smokeTable =
        g_arena.searchSeanceClassTable("Smoke");
    const char *smokeAttribute = context->searchObject(attr->m_smokeAttrID);
    return smokeTable != ct_NULLID &&
           attr->m_smokeTableID == smokeTable &&
           smokeAttribute != NULL &&
           SmokeSubjectState_SimulationSupported(context, smokeAttribute);
}

static bool SmokerLightReady(const AttributeSmoker *attr)
{
    return attr != NULL && attr->m_useLight &&
           std::isfinite(attr->m_minLightBright) &&
           std::isfinite(attr->m_maxLightBright) &&
           std::isfinite(attr->m_lightRadius) &&
           std::isfinite(attr->m_lightBrightStep) &&
           std::isfinite(attr->m_lightOffset) &&
           attr->m_minLightBright >= 0.0 &&
           attr->m_maxLightBright >= attr->m_minLightBright &&
           attr->m_maxLightBright <= 255.0 &&
           attr->m_lightRadius > 0.0 &&
           attr->m_lightBrightStep >= 0.0;
}

static bool SmokerCoronaReady(const AttributeSmoker *attr)
{
    return attr != NULL && attr->m_useCorona &&
           std::isfinite(attr->m_coronaR) &&
           std::isfinite(attr->m_maxCoronaR) &&
           attr->m_coronaR > 0.0 && attr->m_maxCoronaR > 0.0 &&
           attr->m_coronaAlpha > 0 && attr->m_coronaAlpha <= 255 &&
           attr->m_coronaHText != NULL && attr->m_coronaColor != 0;
}

static KR_ObjectID SmokerEmitSmoke(const CFVector3 &position,
                                   const KR_ObjectID &master,
                                   double timeStamp,
                                   ct_ClassTableID table,
                                   const KR_ObjectID &attribute)
{
    KR_Event event;
    event.label = fou_EVCMD_START;
    event.source = master;
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(attribute)
              .putDouble(position.x)
              .putDouble(position.y)
              .putDouble(position.z)
            .close();
    return g_arena.newObject(table, "Smok.Static", event);
}
 /*********************************
  *
  *   Smoker implementation
  *
  *********************************/

 //============================================================
Smoker::Smoker()
 {
    m_attr = 0;
    m_position = CFVector3(0,0,0);
    m_cnt = 0;
    m_startTime = 0;
    m_sendedMove = false;
    m_brightness = 0;
 }

 //============================================================
Smoker::~Smoker()
 {
 }

 //============================================================
void Smoker::onView(double ts)
{
    if(  m_attr != 0 && SmokerCanEmit(context, m_attr) &&
         !m_sendedMove  )
    {
          KR_Event event;
          event.label       = sm_EV_MOVE;
          event.source      = getObjectID();
          event.destination = getObjectID();
          event.timeStamp   = ts < 0.1 ? 0.1 : ts;
          issueEvent( event );
          m_sendedMove = true;
    }
}

CFVector3 Smoker::realPosition()
{
     return m_position;
}
 //============================================================
int Smoker::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case sm_EV_MOVE:
            if(  m_attr == 0 || !SmokerCanEmit(context, m_attr)  )
            {
                 m_sendedMove = false;
                 break;
            }
            if(  m_isVisible  )
            {
                 double ts = event.timeStamp;

                 event.timeStamp += context->rnd_f(
                                                    m_attr->m_createIncMin,
                                                    m_attr->m_createIncMax
                 );
                 issueEvent( event );
                 m_sendedMove = true;
                 SmokerEmitSmoke( m_position,
                                  getObjectID(),
                                  ts,
                                  m_attr->m_smokeTableID,
                                  m_attr->m_smokeAttrID
                                );

#ifndef RR2NW_SMOKER_LIGHT_CORONA_DISABLED
                 if(  m_attr->m_useLight  )
                 {
                      m_brightness +=  context->rnd_f(m_attr->m_lightBrightStep) 
                                     - m_attr->m_lightBrightStep/2;
                      if(  m_brightness < m_attr->m_minLightBright  )
                           m_brightness = m_attr->m_minLightBright;
                      else
                      if(  m_brightness > m_attr->m_maxLightBright  )
                           m_brightness = m_attr->m_maxLightBright;

                 }
#endif
            }
            else m_sendedMove = false;

            break;

    case KR_WAKE_UP:
            break;

    case KR_SET_ATTR:
            s_ASSERTNQ("Smoker:receiveEvent:case KR_SET_ATTR: do't supported");
            break;

    case sm_EV_REMOVE:
            context->removeObject(getObjectID());
            break;

    case fou_EVCMD_START:
     {
            KR_ObjectID oID;
            event.data.open(EDO_READ)
                        .getObjectID(oID)
                        .getDouble(m_position.x)
                        .getDouble(m_position.y)
                        .getDouble(m_position.z)
                      .close();

            ct_Attribute *attr = __attrSmokerTable.searchAttribute(oID);
            if( attr==NULL )
            {
                 echo( "Fountain::receiveEvent: Unknown attribute %s",
                       context->searchObject(oID));
                 m_attr = 0;
                 return 0;
            }
            else m_attr = (AttributeSmoker*)attr;

            if(  m_attr->m_onLand  )
            {
                 if( !SmokerPlaceOnTerrain(m_attr, m_position) )
                 {
                      m_attr = 0;
                      return 0;
                 }
            }
     }
     m_sendedMove = false;
     m_startTime = event.timeStamp;
     
     if(   m_attr->m_maxTimeLife > 0  )
     {
           event.label       = sm_EV_REMOVE;
           event.source      = getObjectID();
           event.destination = getObjectID();
           event.timeStamp   = m_startTime + m_attr->m_maxTimeLife;
           issueEvent( event );
     }

     break;


    default: return 0;
    }
    return 1;
 }

 //============================================================
void Smoker::addNotify()
 {
    ct_Subject::addNotify();
    m_attr = 0;
    m_position = CFVector3(0,0,0);
    m_cnt = 0;
    m_startTime = 0;
    m_sendedMove = false;
    m_brightness = 0;
 }

 //============================================================
void Smoker::removeNotify()
 {
    if (context != NULL)
    {
         context->removeEvent(sm_EV_MOVE, getObjectID());
         context->removeEvent(sm_EV_REMOVE, getObjectID());
    }
    ct_Subject::removeNotify();
 }


 /*************************************
  *
  *   SmokerTable implementation
  *
  *************************************/

 //============================================================
void SmokerTable::allocObjects( int objectQnty )
 {
    m_table = new Smoker[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void SmokerTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *SmokerTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index < m_maxObjectQnty ,"SmokerTable::getObjectPTR");
    return &(m_table[ index ]);
 }

void Smoker::render( CViewDynamicList &list, double )
{
#ifdef RR2NW_SMOKER_LIGHT_CORONA_DISABLED
    (void)list;
#else
    if(  m_attr == 0  )
         return;
    if(  m_attr->m_useLight  )
         g_lightChain.add( m_position+CFVector3(0,m_attr->m_lightOffset,0), 
                      m_attr->m_lightColor, 
                      (int)(m_brightness), 
                      m_attr->m_lightRadius );

    if(  m_attr->m_useCorona  )
    {             
         double r = Abs2(CViewObject::m_viewPointInvMx.Offset()-realPosition())*m_attr->m_coronaR;
         if(  r > m_attr->m_maxCoronaR  )
              r = m_attr->m_maxCoronaR;
         m_corona.prepareToRender(
                                realPosition(),
                                r,
                                2<<16, 2<<16,
                                126<<16, 126<<16,
                                m_attr->m_coronaAlpha,
                                m_attr->m_coronaColor,
                                m_attr->m_coronaHText
                                 );
         list.Load( &m_corona );
    }
#endif
}

void Smoker::endRender( CViewScene *scene )
{
#ifdef RR2NW_SMOKER_LIGHT_CORONA_DISABLED
     (void)scene;
#else
     if(  m_attr != 0 && m_attr->m_useCorona  )
          scene->RemoveLandDynamic( &m_corona );
#endif
}

namespace {

const unsigned long long kDynSmokerHashOffset = 14695981039346656037ull;
const unsigned long long kDynSmokerHashPrime = 1099511628211ull;

void DynSmokerHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kDynSmokerHashPrime;
    }
}

void DynSmokerHashString(unsigned long long &hash, const char *value)
{
    DynSmokerHashBytes(hash, value,
                       static_cast<int>(strlen(value)) + 1);
}

}  // namespace

void SmokerSubjectState_Link()
{
}

bool SmokerSubjectState_DynTableReady(SimulationContext *context,
                                      int expectedCapacity)
{
    if (context == NULL || expectedCapacity <= 0 ||
        g_arena.getContext() != context)
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("DynSmoker");
    return table != ct_NULLID &&
           table == __classTable2.getClassTableID() &&
           __classTable2.capacity() == expectedCapacity &&
           __classTable2.isRendering();
}

int SmokerSubjectState_DynCapacity()
{
    return __classTable2.capacity();
}

int SmokerSubjectState_DynLiveCount()
{
    int count = 0;
    for (ct_Subject *subject = __classTable2.findFirstSubject();
         subject != NULL;
         subject = __classTable2.findNextSubject(subject))
        ++count;
    return count;
}

unsigned long long SmokerSubjectState_DynFingerprint(
    SimulationContext *context)
{
    const int capacity = SmokerSubjectState_DynCapacity();
    if (!SmokerSubjectState_DynTableReady(context, capacity) ||
        SmokerSubjectState_DynLiveCount() != 0)
        return 0;
    unsigned long long hash = kDynSmokerHashOffset;
    DynSmokerHashString(hash, "DynSmoker");
    DynSmokerHashBytes(hash, &capacity, sizeof(capacity));
    const int rendering = __classTable2.isRendering() ? 1 : 0;
    DynSmokerHashBytes(hash, &rendering, sizeof(rendering));
    const int emission = 1;
    DynSmokerHashBytes(hash, &emission, sizeof(emission));
#ifdef RR2NW_SMOKER_LIGHT_CORONA_DISABLED
    const int lightCorona = 0;
#else
    const int lightCorona = 1;
#endif
    DynSmokerHashBytes(hash, &lightCorona, sizeof(lightCorona));
    return hash;
}

bool SmokerSubjectState_ProbeDynLifecycle(SimulationContext *context,
                                          const char *attributeName,
                                          double timeStamp)
{
    const int capacity = SmokerSubjectState_DynCapacity();
    if (!SmokerSubjectState_DynTableReady(context, capacity) ||
        attributeName == NULL || attributeName[0] == 0 ||
        SmokerSubjectState_DynLiveCount() != 0 ||
        !context->isExist(attributeName))
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoker *attribute = attributeID.isNUL()
        ? NULL
        : static_cast<AttributeSmoker *>(
              __attrSmokerTable.searchAttribute(attributeID));
    if (attribute == NULL || attribute->m_onLand ||
        attribute->m_maxTimeLife > 0)
        return false;

    static const char kProbeName[] = "DynSmoker.Lifecycle.Probe";
    if (context->isExist(kProbeName))
        return false;
    KR_ObjectID object =
        g_arena.newObject(__classTable2.getClassTableID(), kProbeName);
    Smoker *smoker = static_cast<Smoker *>(__classTable2.findFirstSubject());
    if (object.isNUL() || smoker == NULL ||
        smoker->getObjectID() != object ||
        SmokerSubjectState_DynLiveCount() != 1)
    {
        if (!object.isNUL())
            context->removeObject(object);
        return false;
    }

    const CFVector3 position(17.0, 3.0, -19.0);
    KR_Event event;
    event.label = fou_EVCMD_START;
    event.source = g_arena.getObjectID();
    event.destination = object;
    event.timeStamp = timeStamp < 0.1 ? 0.1 : timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(attributeID)
              .putDouble(position.x)
              .putDouble(position.y)
              .putDouble(position.z)
            .close();
    context->sendEventNow(event);

    const bool started = smoker->m_attr == attribute &&
                         smoker->m_position == position &&
                         smoker->m_startTime == event.timeStamp &&
                         !smoker->m_sendedMove &&
                         smoker->m_brightness == 0;
    context->removeObject(object);
    return started && !context->isExist(kProbeName) &&
           SmokerSubjectState_DynLiveCount() == 0;
}

bool SmokerSubjectState_EmissionSupported(SimulationContext *context,
                                           const char *attributeName)
{
    const int capacity = SmokerSubjectState_DynCapacity();
    if (!SmokerSubjectState_DynTableReady(context, capacity) ||
        attributeName == NULL || attributeName[0] == 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoker *attribute = attributeID.isNUL()
        ? NULL
        : static_cast<AttributeSmoker *>(
              __attrSmokerTable.searchAttribute(attributeID));
    return SmokerCanEmit(context, attribute);
}

bool SmokerSubjectState_LightCoronaSupported(
    SimulationContext *context, const char *attributeName)
{
    if (!SmokerSubjectState_EmissionSupported(context, attributeName))
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoker *attribute = attributeID.isNUL()
        ? NULL
        : static_cast<AttributeSmoker *>(
              __attrSmokerTable.searchAttribute(attributeID));
#ifdef RR2NW_SMOKER_LIGHT_CORONA_DISABLED
    (void)attribute;
    return false;
#else
    return SmokerLightReady(attribute) && SmokerCoronaReady(attribute);
#endif
}

bool SmokerSubjectState_ProbeEmissionLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp)
{
    if (!SmokerSubjectState_EmissionSupported(context, attributeName) ||
        SmokerSubjectState_DynLiveCount() != 0 ||
        SmokeSubjectState_LiveCount() != 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoker *attribute = static_cast<AttributeSmoker *>(
        __attrSmokerTable.searchAttribute(attributeID));

    static const char kProbeName[] = "DynSmoker.Emission.Probe";
    static const char kSmokeName[] = "Smok.Static";
    if (context->isExist(kProbeName) || context->isExist(kSmokeName))
        return false;
    KR_ObjectID object =
        g_arena.newObject(__classTable2.getClassTableID(), kProbeName);
    Smoker *smoker = static_cast<Smoker *>(__classTable2.findFirstSubject());
    if (object.isNUL() || smoker == NULL || smoker->getObjectID() != object)
    {
        if (!object.isNUL())
            context->removeObject(object);
        return false;
    }

    const CFVector3 position(17.0, 3.0, -19.0);
    CFVector3 expectedPosition = position;
    if (!SmokerPlaceOnTerrain(attribute, expectedPosition))
    {
        context->removeObject(object);
        return false;
    }
    KR_Event event;
    event.label = fou_EVCMD_START;
    event.source = g_arena.getObjectID();
    event.destination = object;
    event.timeStamp = timeStamp < 0.1 ? 0.1 : timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(attributeID)
              .putDouble(position.x)
              .putDouble(position.y)
              .putDouble(position.z)
            .close();
    context->sendEventNow(event);

    smoker->m_isVisible = true;
    smoker->onView(event.timeStamp);
    const bool firstMoveScheduled =
        smoker->m_attr == attribute &&
        smoker->m_position == expectedPosition && smoker->m_sendedMove &&
        context->removeEvent(sm_EV_MOVE, object) != 0;
    if (firstMoveScheduled)
    {
        event.label = sm_EV_MOVE;
        event.source = object;
        event.destination = object;
        context->sendEventNow(event);
    }

    KR_ObjectID smoke = context->searchObject(kSmokeName);
    const bool emitted = firstMoveScheduled && !smoke.isNUL() &&
                         context->isExist(object) &&
                         context->isExist(smoke) &&
                         smoker->m_sendedMove &&
                         SmokerSubjectState_DynLiveCount() == 1 &&
                         SmokeSubjectState_LiveCount() == 1;
    const bool queued = emitted &&
                        context->removeEvent(sm_EV_MOVE, object) != 0 &&
                        context->removeEvent(fou_EVC_MOVING, smoke) != 0;
    if (queued)
    {
        KR_Event pending;
        pending.label = sm_EV_MOVE;
        pending.source = object;
        pending.destination = object;
        pending.timeStamp = event.timeStamp + 1000.0;
        context->addEvent(pending);
        pending.label = fou_EVC_MOVING;
        pending.source = smoke;
        pending.destination = smoke;
        context->addEvent(pending);
    }
    if (context->isExist(object))
        context->removeObject(object);
    if (!smoke.isNUL() && context->isExist(smoke))
        context->removeObject(smoke);

    return queued && !context->isExist(kProbeName) &&
           !context->isExist(kSmokeName) &&
           SmokerSubjectState_DynLiveCount() == 0 &&
           SmokeSubjectState_LiveCount() == 0 &&
           context->removeEvent(sm_EV_MOVE, object) == 0 &&
           context->removeEvent(sm_EV_REMOVE, object) == 0 &&
           (smoke.isNUL() ||
            context->removeEvent(fou_EVC_MOVING, smoke) == 0);
}

bool SmokerSubjectState_ProbeLightCoronaLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp)
{
    if (!SmokerSubjectState_LightCoronaSupported(context, attributeName) ||
        SmokerSubjectState_DynLiveCount() != 0 ||
        SmokeSubjectState_LiveCount() != 0 ||
        g_lightChain.m_count != 0 || g_lightChain.m_list != NULL)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoker *attribute = static_cast<AttributeSmoker *>(
        __attrSmokerTable.searchAttribute(attributeID));

    static const char kProbeName[] = "DynSmoker.LightCorona.Probe";
    static const char kSmokeName[] = "Smok.Static";
    if (context->isExist(kProbeName) || context->isExist(kSmokeName))
        return false;
    KR_ObjectID object =
        g_arena.newObject(__classTable2.getClassTableID(), kProbeName);
    Smoker *smoker = static_cast<Smoker *>(__classTable2.findFirstSubject());
    if (object.isNUL() || smoker == NULL || smoker->getObjectID() != object)
    {
        if (!object.isNUL())
            context->removeObject(object);
        return false;
    }

    const CFVector3 position(17.0, 3.0, -19.0);
    KR_Event event;
    event.label = fou_EVCMD_START;
    event.source = g_arena.getObjectID();
    event.destination = object;
    event.timeStamp = timeStamp < 0.1 ? 0.1 : timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(attributeID)
              .putDouble(position.x)
              .putDouble(position.y)
              .putDouble(position.z)
            .close();
    context->sendEventNow(event);
    smoker->m_isVisible = true;
    smoker->onView(event.timeStamp);

    const bool firstMove = smoker->m_attr == attribute &&
                           smoker->m_brightness == 0.0 &&
                           context->removeEvent(sm_EV_MOVE, object) != 0;
    if (firstMove)
    {
        event.label = sm_EV_MOVE;
        event.source = object;
        event.destination = object;
        context->sendEventNow(event);
    }
    KR_ObjectID smoke = context->searchObject(kSmokeName);
    const bool moved = firstMove && !smoke.isNUL() &&
                       smoker->m_brightness >=
                           attribute->m_minLightBright &&
                       smoker->m_brightness <=
                           attribute->m_maxLightBright &&
                       context->removeEvent(sm_EV_MOVE, object) != 0 &&
                       context->removeEvent(fou_EVC_MOVING, smoke) != 0;
    if (!smoke.isNUL() && context->isExist(smoke))
        context->removeObject(smoke);

    CViewDynamicList list;
    if (moved)
        smoker->render(list, event.timeStamp);
    const CFVector3 lightPosition =
        position + CFVector3(0, attribute->m_lightOffset, 0);
    const bool rendered = moved && g_lightChain.m_count == 1 &&
                          g_lightChain.m_list == g_lightChain.m_dim &&
                          g_lightChain.m_list->m_pos == lightPosition &&
                          g_lightChain.m_list->m_color ==
                              attribute->m_lightColor &&
                          g_lightChain.m_list->m_brightness ==
                              static_cast<int>(smoker->m_brightness) &&
                          g_lightChain.m_list->m_radius ==
                              attribute->m_lightRadius &&
                          list.First() == &smoker->m_corona &&
                          smoker->m_corona.m_pos == position &&
                          smoker->m_corona.m_radius > 0.0 &&
                          smoker->m_corona.m_radius <=
                              attribute->m_maxCoronaR &&
                          smoker->m_corona.m_alpha ==
                              attribute->m_coronaAlpha &&
                          smoker->m_corona.m_color ==
                              attribute->m_coronaColor &&
                          smoker->m_corona.m_htext ==
                              attribute->m_coronaHText;
    list.Clear(false);
    if (rendered)
        g_lightChain.render();
    const bool lightPublished = rendered &&
                                CViewObject::EnabledLights() == 1 &&
                                _gr_pLights[0].r ==
                                    attribute->m_lightRadius &&
                                _gr_pLights[0].power0 ==
                                    static_cast<int>(smoker->m_brightness) &&
                                _gr_pLights[0].color ==
                                    attribute->m_lightColor;
    CViewObject::EnableLights(0);
    g_lightChain.m_list = NULL;
    g_lightChain.m_count = 0;

    if (moved)
    {
        KR_Event pending;
        pending.label = sm_EV_MOVE;
        pending.source = object;
        pending.destination = object;
        pending.timeStamp = event.timeStamp + 1000.0;
        context->addEvent(pending);
    }
    if (context->isExist(object))
        context->removeObject(object);

    return lightPublished && !context->isExist(kProbeName) &&
           !context->isExist(kSmokeName) &&
           SmokerSubjectState_DynLiveCount() == 0 &&
           SmokeSubjectState_LiveCount() == 0 &&
           g_lightChain.m_count == 0 && g_lightChain.m_list == NULL &&
           CViewObject::EnabledLights() == 0 &&
           context->removeEvent(sm_EV_MOVE, object) == 0 &&
           context->removeEvent(sm_EV_REMOVE, object) == 0 &&
           (smoke.isNUL() ||
            context->removeEvent(fou_EVC_MOVING, smoke) == 0);
}


/* End of file C:\NW\ARENA\OBASE\Smoke\smoker.cpp */
