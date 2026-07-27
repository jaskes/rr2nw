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
 }

 //============================================================
Smoker::~Smoker()
 {
 }

 //============================================================
void Smoker::onView(double ts)
{
    if(  !m_sendedMove  ) 
    {
          KR_Event event;
          event.label       = sm_EV_MOVE;
          event.source      = getObjectID();
          event.destination = getObjectID();
          event.timeStamp   = ts;
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
                 echo( "Fountain::receiveEvent: Unknown attribute %s",
                       context->searchObject(oID));
            else m_attr = (AttributeSmoker*)attr;

            if(  m_attr->m_onLand  )
            {
                 double height;
                 CFVector3 normal;
                 CViewScene::Current()->GetTerrain()->GetPlane(m_position,normal,height);
                 m_position.y = height;
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
    // insert your code this
    m_attr = 0;
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
    s_ASSERT( index >= 0 && index <= m_maxObjectQnty ,"SmokerTable::getObjectPTR");
    return &(m_table[ index ]);
 }

void Smoker::render( CViewDynamicList &list, double )
{
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
}

void Smoker::endRender( CViewScene *scene )
{
     if(  m_attr->m_useCorona  )
          scene->RemoveLandDynamic( &m_corona );
}


/* End of file C:\NW\ARENA\OBASE\Smoke\smoker.cpp */
