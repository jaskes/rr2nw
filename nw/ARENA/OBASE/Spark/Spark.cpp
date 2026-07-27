/*
 * File  : D:\GAME\OBASE\Spark\Spark.cpp
 * Autor :
 * Ver   1.0 
 */
#include "Spark.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/sparkmsg.h"
#include "message/skinmsg.h"
#include "enum/spaceenum.h"
#include "scene.h"
#include "h/light.h"

#ifdef  __TRACE_NW__
#include "afxwin.h"
#else
class CDC{};
#endif

#ifndef RR2NW_SPARK_ATTRIBUTE_STATE_EXTERNAL
#include "SparkAttributeState.inl"
#endif


//{{EVENT_LABEL_NAMES
static s_ELN elnTable[]=
{
   s_ELN(sp_EVC_LIFE,"sp_EVC_LIFE"),
   s_ELN(sp_EV_CREATE,"sp_EV_CREATE"),
   s_ELN(sp_EV_SET_PHASE_COUNT,"sp_EV_SET_PHASE_COUNT"),
   s_ELN(sp_EV_SET_PHASE,"sp_EV_SET_PHASE"),
   s_ELN(),
};

static s_ELNTable selnTable("Default",elnTable);

//}}END_OF_EVENT_LABEL_NAMES

 //===========================================================================
class SparkTable : public ct_SubjectTable
{
 private:
    Spark *m_table;
 public:
    SparkTable()
    {
        m_table = NULL;
        registerClass( "Spark" );
    }
    ~SparkTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    virtual  bool      isRendering();
};

static SparkTable          __classTable;

 /*********************************
  *
  *   Spark implementation
  *
  *********************************/

 //============================================================
Spark::Spark()
 {
    m_attr = &__defaultSparkAttr;
    m_curPhase = 0;
 }

 //============================================================
Spark::~Spark()
 {
 }

 //============================================================
int Spark::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case KR_WAKE_UP:
            break;

    case sp_EV_CREATE:
            {
            int attrIndex;

            //{{GET_EVENT(sp_EV_CREATE)
            event.data.open(EDO_READ)
                           .descend(VECTOR3D_F , 0 )
                             .getDouble(m_position.x)
                             .getDouble(m_position.y)
                             .getDouble(m_position.z)
                           .ascend()
                           .getInt(attrIndex)
                      .close();
            //}}END_OF_GET_EVENT(sp_EV_CREATE)
            __attrSparkTable.setAttribute(attrIndex,(ct_Attribute*&)m_attr);

            event.label      = sp_EVC_LIFE;
            event.source     = getObjectID();
            event.timeStamp += m_attr->m_phase[0].time;
            //{{PUT_EVENT(sp_EVC_LIFE)
            //}}END_OF_PUT_EVENT(sp_EVC_LIFE)
            issueEvent( event );
            m_curPhase = 0;
            }
            break;

    case sp_EVC_LIFE:
            {
            //{{GET_EVENT(sp_EVC_LIFE)
            //}}END_OF_GET_EVENT(sp_EVC_LIFE)
            if(  m_curPhase >= m_attr->m_phaseCnt-1  )
                 getContext()->removeObject(getObjectID());
            else
            {
                 event.source     = getObjectID();
                 event.timeStamp += m_attr->m_phase[m_curPhase].time;
                 //{{PUT_EVENT(sp_EVC_LIFE)
                 //}}END_OF_PUT_EVENT(sp_EVC_LIFE)
                 issueEvent( event );

                 m_curPhase++;
            }

            }
            break;

    case KR_SET_ATTR:
            {
            KR_ObjectID oID;
            event.data.open(EDO_READ)
                        .getObjectID(oID)
                      .close();
            ct_Attribute *attr = __attrSparkTable.searchAttribute(oID);
            if( attr==NULL )
                 echo( "Spark::receiveEvent: Unknown attribute %s",
                       context->searchObject(oID));
            else m_attr = (AttributeSpark*)attr;
            m_viewDynSpr.initRef( ((AttributeSpark*)attr)->m_cacheSkin );
            }
            break;


    default: return 0;
    }
    return 1;
 }

 //============================================================
void Spark::addNotify()
 {
    ct_Object::addNotify();
    // insert your code this
 }

 //============================================================
void Spark::removeNotify()
 {
    ct_Object::removeNotify();
    // insert your code this
 }

 //============================================================
#ifdef  __TRACE_NW__
void Spark::draw( CDC &gc )
 {
    switch( m_attr->m_skin )
    {
    case 0:
            {
            CBrush brush(RGB(255,255,0));
            CBrush *ob = gc.SelectObject(&brush);
            double x =  m_position.x,
                   y = -m_position.z;
            double r = (m_curTime-m_startTime)*10 / (m_attr->m_timeOfLife*2);
            gc.Ellipse( (int)(x-r), (int)(y-r), (int)(x+r), (int)(y+r) );
            gc.SelectObject(ob);
            }
            break;
    default: s_ASSERTNQ("Spark::draw()");
    }
 }
#else
void Spark::draw( CDC & ) {}
#endif

CFVector3 Spark::realPosition()
 {
    return m_position;
 }

 /*************************************
  *
  *   SparkTable implementation
  *
  *************************************/

 //============================================================
void SparkTable::allocObjects( int objectQnty )
 {
    m_table = new Spark[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void SparkTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *SparkTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index <= m_maxObjectQnty ,"SparkTable::getObjectPTR");
    return &(m_table[ index ]);
 }

 //============================================================
bool SparkTable::isRendering()
 {
    return true;
 }

 //============================================================
void Spark::render(  CViewDynamicList &list, double  )
 {
    const SparkPhase &cur = m_attr->m_phase[m_curPhase];
    m_viewDynSpr.prepareToRender(m_position,m_attr->m_maxRadius, 
              cur.u0,cur.v0, cur.u1, cur.v1, 
              m_attr->m_cacheSkin);
    list.Load( &m_viewDynSpr );
    g_lightChain.add( m_position, 
                      cur.color, 
                      cur.brightness, 
                      cur.radius );
 }

 //============================================================
void Spark::endRender( CViewScene *scene )
 {
    scene->RemoveLandDynamic( &m_viewDynSpr );
 }


/* End of file D:\GAME\OBASE\Spark\Spark.cpp */
