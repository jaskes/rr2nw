/*
 * File  : C:\NW\ARENA\OBASE\Smoke\smoker.cpp
 * Autor :
 * Ver   1.0 
 */
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
#ifdef RR2NW_SMOKER_SUBJECT_ONLY
    (void)ts;
#else
    if(  m_attr != 0 && !m_sendedMove  )
    {
          KR_Event event;
          event.label       = sm_EV_MOVE;
          event.source      = getObjectID();
          event.destination = getObjectID();
          event.timeStamp   = ts;
          issueEvent( event );
          m_sendedMove = true;
    }
#endif
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
#ifdef RR2NW_SMOKER_SUBJECT_ONLY
            m_sendedMove = false;
#else
            if(  m_attr == 0  )
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
                 createSmoke( m_position,
                              getObjectID(),
                              ts,
                              m_attr->m_smokeTableID,
                              m_attr->m_smokeAttrID
                             );

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
            }
            else m_sendedMove = false;
#endif

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
#ifdef RR2NW_SMOKER_SUBJECT_ONLY
                 m_attr = 0;
                 return 0;
#else
                 double height;
                 CFVector3 normal;
                 CViewScene::Current()->GetTerrain()->GetPlane(m_position,normal,height);
                 m_position.y = height;
#endif
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
    ct_Subject::removeNotify();
    // insert your code this
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
#ifdef RR2NW_SMOKER_SUBJECT_ONLY
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
#ifdef RR2NW_SMOKER_SUBJECT_ONLY
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


/* End of file C:\NW\ARENA\OBASE\Smoke\smoker.cpp */
