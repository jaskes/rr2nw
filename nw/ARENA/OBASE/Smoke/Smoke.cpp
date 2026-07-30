/*
 * File  : C:\NW\ARENA\OBASE\Smoke\Smoke.cpp
 * Author :
 * Ver   1.0 
 */

#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "Smoke.h"
#include "SmokeActiveWorldState.h"
#include "SmokeSubjectState.h"
#ifdef RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
#include "SmokeAttributeState.h"
#endif
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/fountmsg.h"
#include "h/cachesmoke.h"
#include "h/phisics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if defined(RR2NW_SMOKE_SUBJECT_ONLY) || \
    defined(RR2NW_SMOKE_SIMULATION_ONLY)
#define RR2NW_SMOKE_RENDER_DISABLED
#endif

#ifndef RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
 //===========================================================================
class AttributeSmoke : public ct_Attribute
{
 public:
    enum
    {
         MAX_COLOR = 32
    };
    GR_HTEXTURE    m_cacheImage;
    unsigned long  m_cacheColor;
    unsigned long  colors[MAX_COLOR];
    
  
    virtual void    update(double);
  //{{ATTRIBUTE
    ct_AttrItem  m_array[39];
    double          m_radius                 ;  // 
    int             m_onLand                 ;  // 
    int             m_maxBlob                ;  // 
    double          m_minDirInc              ;  // 
    double          m_maxDirInc              ;  // 
    double          m_rndOfs                 ;  // RND Radius for start centers
    double          m_ofsHAngle              ;  // 
    double          m_ofsVAngle              ;  // 
    double          m_ofsSpeed               ;  // 
    double          m_dirHAngle              ;  // 
    double          m_dirVAngle              ;  // 
    double          m_dirSpeed               ;  // 
    double          m_maxTimeLife            ;  // 
    double          m_minRA                  ;  // 
    double          m_maxRA                  ;  // 
    double          m_minRB                  ;  // 
    double          m_maxRB                  ;  // 
    double          m_minRC                  ;  // 
    double          m_maxRC                  ;  // 
    double          m_minTA                  ;  // 
    double          m_maxTA                  ;  // 
    double          m_minTB                  ;  // 
    double          m_maxTB                  ;  // 
    double          m_minTC                  ;  // 
    double          m_maxTC                  ;  // 
    ct_AttrStr      m_imageName              ;  // 
    int             m_RGB                    ;  // 
    double          m_timeIncrement          ;  // 
    int             RGB0                     ;  // 
    int             RGB1                     ;  // 
    int             RGB2                     ;  // 
    int             RGB3                     ;  // 
    int             m_isColorGradient        ;  // 
    int             m_useRndDir              ;  // 
    double          m_rndDir                 ;  // 
    double          m_r0                     ;  // 
    double          m_r1                     ;  // 
    double          m_r2                     ;  // 
    double          m_r3                     ;  // 

    AttributeSmoke()
    {
        m_cacheImage          = NULL;
        m_cacheColor          = 0;
        for( int i = 0; i < MAX_COLOR; ++i )
             colors[i] = 0;
        m_radius             = 3;
        m_onLand             = 1;
        m_maxBlob            = 4;
        m_minDirInc          = 0.9;
        m_maxDirInc          = 0.91;
        m_rndOfs             = 1;
        m_ofsHAngle          = 0;
        m_ofsVAngle          = 0;
        m_ofsSpeed           = 0.5;
        m_dirHAngle          = 0.0;
        m_dirVAngle          = 0.0;
        m_dirSpeed           = 1.0;
        m_maxTimeLife        = 20;
        m_minRA              = 0;
        m_maxRA              = 0;
        m_minRB              = 0;
        m_maxRB              = 0;
        m_minRC              = 1;
        m_maxRC              = 2;
        m_minTA              = 0;
        m_maxTA              = 0;
        m_minTB              = 0;
        m_maxTB              = 0;
        m_minTC              = 200;
        m_maxTC              = 255;
        strncpy(m_imageName,"Smoke.spr", sizeof( ct_AttrStr )-1 );
        m_RGB                = 0;
        m_timeIncrement      = 0.02;
        RGB0                 = 0;
        RGB1                 = 0;
        RGB2                 = 0;
        RGB3                 = 0;
        m_isColorGradient    = 0;
        m_useRndDir          = 0;
        m_rndDir             = 0.2;
        m_r0                 = 0.1;
        m_r1                 = 1;
        m_r2                 = 1;
        m_r3                 = 0.5;

        m_array[0].set("m_radius",m_radius);
        m_array[1].set("m_onLand",m_onLand);
        m_array[2].set("m_maxBlob",m_maxBlob);
        m_array[3].set("m_minDirInc",m_minDirInc);
        m_array[4].set("m_maxDirInc",m_maxDirInc);
        m_array[5].set("m_rndOfs",m_rndOfs);
        m_array[6].set("m_ofsHAngle",m_ofsHAngle);
        m_array[7].set("m_ofsVAngle",m_ofsVAngle);
        m_array[8].set("m_ofsSpeed",m_ofsSpeed);
        m_array[9].set("m_dirHAngle",m_dirHAngle);
        m_array[10].set("m_dirVAngle",m_dirVAngle);
        m_array[11].set("m_dirSpeed",m_dirSpeed);
        m_array[12].set("m_maxTimeLife",m_maxTimeLife);
        m_array[13].set("m_minRA",m_minRA);
        m_array[14].set("m_maxRA",m_maxRA);
        m_array[15].set("m_minRB",m_minRB);
        m_array[16].set("m_maxRB",m_maxRB);
        m_array[17].set("m_minRC",m_minRC);
        m_array[18].set("m_maxRC",m_maxRC);
        m_array[19].set("m_minTA",m_minTA);
        m_array[20].set("m_maxTA",m_maxTA);
        m_array[21].set("m_minTB",m_minTB);
        m_array[22].set("m_maxTB",m_maxTB);
        m_array[23].set("m_minTC",m_minTC);
        m_array[24].set("m_maxTC",m_maxTC);
        m_array[25].set("m_imageName",m_imageName);
        m_array[26].set("m_RGB",m_RGB);
        m_array[27].set("m_timeIncrement",m_timeIncrement);
        m_array[28].set("RGB0",RGB0);
        m_array[29].set("RGB1",RGB1);
        m_array[30].set("RGB2",RGB2);
        m_array[31].set("RGB3",RGB3);
        m_array[32].set("m_isColorGradient",m_isColorGradient);
        m_array[33].set("m_useRndDir",m_useRndDir);
        m_array[34].set("m_rndDir",m_rndDir);
        m_array[35].set("m_r0",m_r0);
        m_array[36].set("m_r1",m_r1);
        m_array[37].set("m_r2",m_r2);
        m_array[38].set("m_r3",m_r3);

        linkTable(m_array,39);
    }
//}}END_OF_ATTRIBUTE
};
#endif


 //===========================================================================
class SmokeTable : public ct_SubjectTable
{
 private:
    Smoke *m_table;
 public:
    SmokeTable()
    {
        m_table = NULL;
        registerClass( "Smoke" );
    }
    ~SmokeTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    virtual  bool      isRendering();
    int                capacity() const { return m_maxObjectQnty; }
    Smoke             *find(const KR_ObjectID &object) const;
};

#ifndef RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
 //===========================================================================
class AttributeTableSmoke : public ct_AttributeTable
{
 protected:
    AttributeSmoke *m_table;

 public:
    AttributeTableSmoke()
    {
       m_table = NULL;
       registerClass( "SmokeAttr" );
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
};
#endif

static SmokeTable  __classTable;
#ifndef RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
static AttributeTableSmoke __attrTable;
#else
#define __attrTable __attrSmokeTable
#endif

static bool SmokeAttributeCanSimulate(const AttributeSmoke *attr)
{
    return attr != NULL && attr->m_maxBlob > 0 &&
           attr->m_maxBlob <= Smoke::MAXSMOKEBLOB &&
           attr->m_timeIncrement > 0.002 &&
           attr->m_maxTimeLife > 0 && attr->m_radius > 0 &&
           attr->m_minDirInc <= attr->m_maxDirInc &&
           attr->m_minRA <= attr->m_maxRA &&
           attr->m_minRB <= attr->m_maxRB &&
           attr->m_minRC <= attr->m_maxRC &&
           attr->m_minTA <= attr->m_maxTA &&
           attr->m_minTB <= attr->m_maxTB &&
           attr->m_minTC <= attr->m_maxTC;
}

static bool SmokeTerrainReady(const AttributeSmoke *attr)
{
    if (attr == NULL || !attr->m_onLand)
        return attr != NULL;
    CViewScene *scene = CViewScene::Current();
    return scene != NULL && scene->GetTerrain() != NULL;
}

static bool SmokePlaceOnTerrain(const AttributeSmoke *attr,
                                CFVector3 &position)
{
    if (!SmokeTerrainReady(attr))
        return false;
    if (!attr->m_onLand)
        return true;
    double height;
    CFVector3 normal;
    CViewScene::Current()->GetTerrain()->GetPlane(position, normal, height);
    position.y = height;
    return true;
}
 /*********************************
  *
  *   Smoke implementation
  *
  *********************************/

 //============================================================
Smoke::Smoke()
 {
    resetTransientState();
 }

void Smoke::resetTransientState()
 {
    m_smokeAttrID = KR_ObjectID::NUL();
    m_viewIter = 0;
    m_pos = CFVector3(0,0,0);
    m_prevTimeStamp = 0;
    m_setRemove = 0;
    m_attr = 0;
    m_cnt  = 0;
    m_viewObj.m_master = this;
    m_viewObj.m_visible = false;
    m_viewObj.m_z = 0;
    m_dynamicPublished = false;
    for( int i = 0; i < MAXSMOKEBLOB; ++i )
         m_blob[i] = SmokeBlob();
 }

 //============================================================
Smoke::~Smoke()
 {
 }

void Smoke::onHide(double)
{
    m_setRemove = 1;
//    context->removeObject(getObjectID());
}


void Smoke::setSmokeAttr()
{
	m_attr = NULL;
	ct_Attribute *attr = __attrTable.searchAttribute(m_smokeAttrID);
    if( attr==NULL )
	{
		const char *name = context->searchObject(m_smokeAttrID);
		echo( "Smoke::receiveEvent: Unknown attribute %s",
		      name == NULL ? "<missing>" : name);
	}
	else 
		m_attr = (AttributeSmoke*)attr;
}

 //============================================================
int Smoke::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case fou_EVC_MOVING:
#ifdef RR2NW_SMOKE_SUBJECT_ONLY
            break;
#else
            if(  m_setRemove  )
            {
                 context->removeObject(getObjectID());
                 break;
            }

            if(  onMove(event.timeStamp-m_prevTimeStamp)  )
            {
                 m_prevTimeStamp = event.timeStamp;

                 s_ASSERT(m_attr->m_timeIncrement >0.002,"Smoke bed");
                 event.timeStamp += m_attr->m_timeIncrement;
                 m_viewIter--;
            
                 issueEvent( event );
            }
            break;
#endif

    case KR_WAKE_UP:
            break;

    case KR_SET_ATTR:
            s_ASSERTNQ("Smoke:receiveEvent:case KR_SET_ATTR: do't supported");
            break;

    case fou_EVCMD_START:
            {
            //KR_ObjectID oID;

            resetTransientState();

            event.data.open(EDO_READ)
                        .getObjectID(m_smokeAttrID)
                        .getDouble(m_pos.x)
                        .getDouble(m_pos.y)
                        .getDouble(m_pos.z)
                      .close();

			setSmokeAttr();

             m_viewIter = MAX_ITER_WAIT;
#ifdef RR2NW_SMOKE_SUBJECT_ONLY
            if( m_attr == NULL ) return 0;
            m_prevTimeStamp = event.timeStamp;
#else
            if( !SmokeAttributeCanSimulate(m_attr) )
            {
                 context->removeObject(getObjectID());
                 break;
            }
            if( !SmokePlaceOnTerrain(m_attr, m_pos) )
            {
                 context->removeObject(getObjectID());
                 break;
            }
            m_prevTimeStamp  = event.timeStamp;
            onCreate();

            event.label      = fou_EVC_MOVING;
            event.source     = getObjectID();
            context->sendEventNow( event );
#endif
            }
            break;

    case fou_EVCMD_START_WITHDIR:
     {
            KR_ObjectID oID;
            CFVector3   dir;

            resetTransientState();

            event.data.open(EDO_READ)
                        .getObjectID(oID)
                        .getDouble(m_pos.x)
                        .getDouble(m_pos.y)
                        .getDouble(m_pos.z)
                        .getDouble(dir.x)
                        .getDouble(dir.y)
                        .getDouble(dir.z)
                      .close();

            ct_Attribute *attr = __attrTable.searchAttribute(oID);
            if( attr==NULL )
            {
                 const char *name = context->searchObject(oID);
                 echo( "Smoke::receiveEvent: Unknown attribute %s",
                       name == NULL ? "<missing>" : name);
            }
            else m_attr = (AttributeSmoke*)attr;

            m_viewIter = MAX_ITER_WAIT;
#ifdef RR2NW_SMOKE_SUBJECT_ONLY
            if( m_attr == NULL ) return 0;
            m_smokeAttrID = oID;
            m_prevTimeStamp = event.timeStamp;
#else
            m_smokeAttrID = oID;
            if( !SmokeAttributeCanSimulate(m_attr) )
            {
                 context->removeObject(getObjectID());
                 break;
            }
            if( !SmokePlaceOnTerrain(m_attr, m_pos) )
            {
                 context->removeObject(getObjectID());
                 break;
            }
            m_prevTimeStamp  = event.timeStamp;
            onCreate( dir );

            event.label      = fou_EVC_MOVING;
            event.source     = getObjectID();
            context->sendEventNow( event );
#endif
     }
            break;


    default: return 0;
    }
    return 1;
 }

 //============================================================
void s_SmokeObject::prepareToRender()
{
#ifdef RR2NW_SMOKE_RENDER_DISABLED
    m_visible = false;
#else
    int cnt = m_master->m_cnt;
    m_visible = false;

    if(  cnt > 0  )
    {
         CFVector3 p(0,0,0);

         for( int i = 0; i < cnt; ++i )
              p += m_master->m_blob[i].m_position;
         p *= 1./cnt;
         m_dynBase = m_dynBase1 = m_bump.start = p;
         CFVector3 v = CViewObject::m_viewPointDirSMx*p;
         if(  v.z > 1  )
         {
              m_visible = true;
              m_z       = (int)(65536./v.z);

              m_bump.vel = CFVector3(0,0,0);
              m_bump.fTime = 0;
              m_bump.fRadius = m_master->m_attr->m_radius;
         }
    }
#endif
}

 //============================================================
void Smoke::addNotify()
 {
    ct_Subject::addNotify();
    resetTransientState();
 }

 //============================================================
void Smoke::removeNotify()
 {
    if (context != NULL)
         context->removeEvent(fou_EVC_MOVING, getObjectID());
    if (m_dynamicPublished)
    {
         CViewScene *scene = CViewScene::Current();
         if (scene != NULL)
              scene->RemoveLandDynamic(&m_viewObj);
         m_dynamicPublished = false;
    }
    ct_Subject::removeNotify();
    resetTransientState();
 }

 //============================================================
void Smoke::draw()
 {
#ifdef RR2NW_SMOKE_RENDER_DISABLED
     return;
#else
     if(  m_cnt <= 0  )
          return;
     int i;
    

     for( i = 0; i < m_cnt; ++i )
     {
          SmokeBlob &b = m_blob[i];
          CFVector3	v = CViewObject::m_viewPointDirSMx*b.m_position;
          if( v.z < CViewObject::m_fFrontClip ) continue;
          double d_v = 1./v.z;

          
          int screen_width  = Round(b.m_radius*CViewObject::m_viewPointScale.x*d_v);
          int screen_x = Round(v.x*d_v),
		      screen_y = Round(v.y*d_v);

          SGRAlphaSprite par;

          par.x0 = screen_x-screen_width/2;
          par.y0 = screen_y-screen_width/2;
          par.x1 = par.x0+screen_width;
          par.y1 = par.y0+screen_width;

          par.u0 = b.u0;
          par.v0 = b.v0;
          par.u1 = b.u1;
          par.v1 = b.v1;

          if(  m_attr->m_isColorGradient  )
          {
               int index = (int)(b.m_phase*(AttributeSmoke::MAX_COLOR-1.0)/b.m_maxTimeLife);
               if(  index < 0  ) index = 0;
               else
               if(  index > AttributeSmoke::MAX_COLOR-1  ) 
                    index = AttributeSmoke::MAX_COLOR-1;

               par.color   = m_attr->colors[index];
          }
          else par.color   = m_attr->m_cacheColor;
          par.opacity = b.m_alpha;
          par.iz      = (int)(d_v*65536);

          par.hTexture = m_attr->m_cacheImage;

          GRDrawAlphaSprite(&par);
     }
#endif
 }

 /*************************************
  *
  *   SmokeTable implementation
  *
  *************************************/
 //============================================================
bool SmokeTable::isRendering()
{
    return 1;
}

 //============================================================
void SmokeTable::allocObjects( int objectQnty )
 {
    m_table = new Smoke[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void SmokeTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *SmokeTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index < m_maxObjectQnty ,"SmokeTable::getObjectPTR");
    return &(m_table[ index ]);
 }

Smoke *SmokeTable::find(const KR_ObjectID &object) const
{
    for (ct_Subject *subject = findFirstSubject(); subject != NULL;
         subject = findNextSubject(subject))
        if (subject->getObjectID() == object)
            return static_cast<Smoke *>(subject);
    return NULL;
}

 /*************************************
  *
  *   AttributeTable implementation
  *
  *************************************/

 //============================================================
#ifndef RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
void AttributeTableSmoke::allocObjects( int objectQnty )
 {
    m_table = new AttributeSmoke[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void AttributeTableSmoke::freeObjects()
 {
    delete [] m_table;
    m_table         = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *AttributeTableSmoke::getObjectPTR( int index )
 {
    s_ASSERT(index>=0 && index <m_maxObjectQnty,"AttributeTable::getObjectPTR");
    return &(m_table[ index ]);
 }
#endif

 //============================================================
int  Smoke::addBlob()
{
   if(  m_cnt < MAXSMOKEBLOB  )
   {
        m_cnt++;
        return  m_cnt-1;
   }

   return -1;
}

 //============================================================
void Smoke::delBlob(int index)
{
    if(  index <  m_cnt && index >=0  )
    {
         for( int i = index+1; i < m_cnt; ++i )
              m_blob[i-1] = m_blob[i];
         m_cnt--;
    }
}

 //============================================================
void s_SmokeObject::Draw()
{
       m_master->draw();
}

 //============================================================
void Smoke::render( CViewDynamicList &list, double )
{
#ifdef RR2NW_SMOKE_RENDER_DISABLED
     (void)list;
#else
     m_viewIter = MAX_ITER_WAIT;
     m_viewObj.prepareToRender();

     if(  m_viewObj.m_visible  )
     {
          list.Load( &m_viewObj );
          m_dynamicPublished = true;
     }
#endif
}

 //============================================================
void Smoke::endRender( CViewScene *scene )
{
#ifdef RR2NW_SMOKE_RENDER_DISABLED
     (void)scene;
#else
     if(  m_dynamicPublished && scene != NULL  )
          scene->RemoveLandDynamic( &m_viewObj );
     m_dynamicPublished = false;
#endif
}

 //============================================================
CFVector3 Smoke::realPosition()
{
    return m_pos;
}


 //============================================================
void Smoke::preCreate( SmokeBlob &b )
{
   int u0 = 2,
       v0 = 2,
       u1 = 126,
       v1 = 126,t;

   int morph = context->rnd_i();
   if(  morph&1  )
   {
        t = u0; u0 = u1; u1 = t;
   }
   if(  morph&2  )
   {
        t = v0; v0 = v1; v1 = t;
   }

   //
   // Координаты в текстуре
   //
   int pos = context->rnd_i()&3;
   int x = (pos& 1)*128,
       y = (pos>>1)*128;
   b.u0 = (x+u0)<<16;
   b.v0 = (y+v0)<<16;
   b.u1 = (x+u1)<<16;
   b.v1 = (y+v1)<<16;

   b.m_ref   = m_attr->m_cacheImage;
   b.m_phase = 0;
   b.m_color = m_attr->m_cacheColor;

   //
   // Позиция внутри сферы
   //
   CFVector3 p(m_pos);
   if(  morph&4  )
        p.x += context->rnd_f(m_attr->m_rndOfs);
   else p.x -= context->rnd_f(m_attr->m_rndOfs);
   if(  morph&8  )
        p.y += context->rnd_f(m_attr->m_rndOfs);
   else p.y -= context->rnd_f(m_attr->m_rndOfs);
   if(  morph&16  )
        p.z += context->rnd_f(m_attr->m_rndOfs);
   else p.z -= context->rnd_f(m_attr->m_rndOfs);

   b.m_position = b.m_startPos = p;
   b.m_dirIncrement = context->rnd_f(m_attr->m_minDirInc,m_attr->m_maxDirInc);

   //
   // Задаем вектор сноса
   //
   CFMatrix3x4 m;
   m.LoadIdentity();
   m.RotateOzL(m_attr->m_ofsVAngle);
   m.RotateOyL(m_attr->m_ofsHAngle);

   b.m_ofsDir   = (m*CFVector3(0,1,0))*m_attr->m_ofsSpeed;


   b.m_maxTimeLife = m_attr->m_maxTimeLife;


   b.tA         = context->rnd_f( m_attr->m_minTA, m_attr->m_maxTA );
   b.tB         = context->rnd_f( m_attr->m_minTB, m_attr->m_maxTB );
   b.tC         = context->rnd_f( m_attr->m_minTC, m_attr->m_maxTC );

   if(  m_attr->m_isColorGradient  )
   {
        double d = b.tB*b.tB-4*b.tA*b.tC;
        if(  fabs(b.tA) > 1.0e-12  )
        {
             if(  d >= 0  )
             {
                  double sqd = sqrt(d);
                  double t0 = (-b.tB-sqd)/(2*b.tA);
                  double t1 = (-b.tB+sqd)/(2*b.tA);
                  if(  t0 <= 0 || (t1 > 0 && t1 < t0)  )
                       t0 = t1;
                  if(  t0 > 0  )
                       b.m_maxTimeLife = t0;
             }
        }
        else if(  fabs(b.tB) > 1.0e-12  )
        {
             double t0 = -b.tC/b.tB;
             if(  t0 > 0  )
                  b.m_maxTimeLife = t0;
        }
   }


   if(  m_attr->m_isColorGradient  )
   {
        calcCoef( b.m_maxTimeLife/3, m_attr->m_r0, m_attr->m_r1, m_attr->m_r2, m_attr->m_r3,
                  b.a0, b.a1, b.a2, b.a3 );
   }
   else
   {
        b.rA         = context->rnd_f( m_attr->m_minRA, m_attr->m_maxRA );
        b.rB         = context->rnd_f( m_attr->m_minRB, m_attr->m_maxRB );
        b.rC         = context->rnd_f( m_attr->m_minRC, m_attr->m_maxRC  );
   }

}

 //============================================================
void Smoke::onCreate()
{
    for( int i = 0; i < m_attr->m_maxBlob; ++i )
    {
         int num = addBlob();
         if(  num < 0  )
              break;
         SmokeBlob &b = m_blob[num];


         preCreate(b);

         if(  !m_attr->m_useRndDir  )
         {
              CFMatrix3x4 m;
              m.LoadIdentity();
              m.RotateOzL(m_attr->m_dirVAngle);
              m.RotateOyL(m_attr->m_dirHAngle);

              b.m_dir      = (m*CFVector3(0,1,0))*m_attr->m_dirSpeed;
         }
         else
         {
              CFMatrix3x4 m;
              m.LoadIdentity();
              m.RotateOzL(context->rnd_f(-m_attr->m_rndDir,m_attr->m_rndDir));
              m.RotateOyL(context->rnd_f(-M_PI,M_PI));

              b.m_dir      = (m*CFVector3(0,1,0))*m_attr->m_dirSpeed;
         }
  }
}

 //============================================================
void Smoke::onCreate( const CFVector3 &dir )
{
    for( int i = 0; i < m_attr->m_maxBlob; ++i )
    {
         int num = addBlob();
         if(  num < 0  )
              break;
         SmokeBlob &b = m_blob[num];

         preCreate(b);

         b.m_dir      = Normal(dir)*m_attr->m_dirSpeed;
  }
}

 //============================================================
int Smoke::onMove( double t )
{

    if(  m_cnt <= 0  )
    {
         context->removeObject(getObjectID());
         return 0;
    }

    for( int i = 0; i < m_cnt; ++i )
    {
         SmokeBlob &b = m_blob[i];
         double T = b.m_phase += t;

         if(  m_attr->m_isColorGradient  )
              b.m_radius = b.a0 + b.a1*T + b.a2*T*T + b.a3*T*T*T;    
         else b.m_radius = b.rA*T*T+b.rB*T+b.rC;

         if(  b.m_radius < 0.001 || b.m_phase > m_attr->m_maxTimeLife  )
         {
              delBlob(i);--i;
              continue;
         }
         b.m_alpha = (int)(b.tA*T*T+b.tB*T+b.tC);
         if(  b.m_alpha <= 0  )
         {
              delBlob(i);--i;
              continue;
         }
         if(  b.m_alpha > 255  )
              b.m_alpha = 255;

         b.m_position += (b.m_ofsDir+b.m_dir)*t;
         b.m_dir *= b.m_dirIncrement*(1.0-t);
    }

    if(  m_cnt <= 0  )
    {
         context->removeObject(getObjectID());
         return 0;
    }

    return 1;
}


#ifndef RR2NW_SMOKE_TEXTURE_CACHE_EXTERNAL
#include "SmokeTextureCache.inl"
#endif

#ifndef RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
inline double getR(int x) { return x>>16; }
inline double getG(int x) { return (x>>8)&255; }
inline double getB(int x) { return x&255; }

unsigned long calcRGB( double x, double r[4], double g[4], double b[4] )
{
     int R,G,B;
     double x2 = x*x, x3 = x2*x;

     R = (int)(r[0] + r[1]*x + r[2]*x2 + r[3]*x3);
     G = (int)(g[0] + g[1]*x + g[2]*x2 + g[3]*x3);
     B = (int)(b[0] + b[1]*x + b[2]*x2 + b[3]*x3);

     if(  R < 0  ) R = 0;
     else if(  R > 255  ) R = 255;

     if(  G < 0  ) G = 0;
     else if(  G > 255  ) G = 255;

     if(  B < 0  ) B = 0;
     else if(  B > 255  ) B = 255;

     return GRTransparentColor(R,G,B);
}

void AttributeSmoke::update(double)
{
   m_cacheColor = GRTransparentColor(m_RGB>>16,(m_RGB>>8)&255,m_RGB&255);
   m_cacheImage = g_loadSmoke( m_imageName, NULL );

   if(  m_isColorGradient  )
   {
        double r[4];
        double g[4];
        double b[4];

        calcCoef(MAX_COLOR/3.0, getR(RGB0), getR(RGB1), getR(RGB2), getR(RGB3),
              r[0], r[1], r[2], r[3]);
        calcCoef(MAX_COLOR/3.0, getG(RGB0), getG(RGB1), getG(RGB2), getG(RGB3),
              g[0], g[1], g[2], g[3]);
        calcCoef(MAX_COLOR/3.0, getB(RGB0), getB(RGB1), getB(RGB2), getB(RGB3),
              b[0], b[1], b[2], b[3]);

        for( int i = 0; i < 32; ++i )
            colors[i] = calcRGB(i*32./31., r,g,b);
   }
}
#endif


bool	Smoke::dump(PIN_SaveFile & sf)
{
		if (!ct_Subject::dump(sf))
			return false;

		if (!sf.WriteData( (char *) & m_smokeAttrID, sizeof(SmokeData)  ))
			return false;

		return true;
}

bool	Smoke::load(PIN_SaveFile & sf)
{
		if (!ct_Subject::load(sf))
			return false;

		if (!sf.GetData( (char *) & m_smokeAttrID, sizeof(SmokeData)  ))
			return false;
		
		return true;
}

void	Smoke::loadNotify()
{
	ct_Subject::loadNotify();
	setSmokeAttr();
	m_viewObj.m_master = this;

    for( int i = 0; i < m_cnt; ++i )
    {
         SmokeBlob &b = m_blob[i];
		 b.m_ref = m_attr->m_cacheImage;
	}

}

void Smoke::draw( CDC & ){}

namespace {

const unsigned long long kSmokeSubjectHashOffset = 14695981039346656037ull;
const unsigned long long kSmokeSubjectHashPrime = 1099511628211ull;
const std::uint32_t kSmokeActiveWorldMagic = 0x314b4d53u; // SMK1
const std::uint32_t kSmokeActiveWorldVersion = 1u;
const std::size_t kMaximumActiveWorldSmokes = 4096;
const std::size_t kMaximumActiveWorldString = MAX_SYMBOLIC_LENGHT - 1;

std::string g_smokeActiveWorldFailure;

struct StableSmokeBlob
{
    double phase;
    std::uint32_t color;
    int u0;
    int v0;
    int u1;
    int v1;
    CFVector3 startPosition;
    CFVector3 position;
    CFVector3 direction;
    CFVector3 offsetDirection;
    double directionIncrement;
    double maximumTimeLife;
    double radius;
    int alpha;
    double radiusA;
    double radiusB;
    double radiusC;
    double opacityA;
    double opacityB;
    double opacityC;
    double spline0;
    double spline1;
    double spline2;
    double spline3;

    StableSmokeBlob()
        : phase(0.0), color(0), u0(0), v0(0), u1(0), v1(0),
          startPosition(0.0, 0.0, 0.0), position(0.0, 0.0, 0.0),
          direction(0.0, 0.0, 0.0), offsetDirection(0.0, 0.0, 0.0),
          directionIncrement(0.0), maximumTimeLife(0.0), radius(0.0),
          alpha(0), radiusA(0.0), radiusB(0.0), radiusC(0.0),
          opacityA(0.0), opacityB(0.0), opacityC(0.0), spline0(0.0),
          spline1(0.0), spline2(0.0), spline3(0.0) {}
};

struct StableSmokeRecord
{
    std::string name;
    std::string attribute;
    int viewIterations;
    CFVector3 position;
    double previousTimeStamp;
    int removeOnMove;
    double movingTimeStamp;
    std::vector<StableSmokeBlob> blobs;

    StableSmokeRecord()
        : viewIterations(0), position(0.0, 0.0, 0.0),
          previousTimeStamp(0.0), removeOnMove(0),
          movingTimeStamp(0.0) {}
};

bool FailSmokeActiveWorld(const std::string &message)
{
    g_smokeActiveWorldFailure = message;
    return false;
}

bool SmokeIsNul(const KR_ObjectID &value)
{
    KR_ObjectID copy = value;
    return copy.isNUL() != 0;
}

bool SmokeFiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

std::string SmokeObjectName(SimulationContext *context,
                            const KR_ObjectID &object)
{
    if (context == NULL || SmokeIsNul(object))
        return std::string();
    const char *name = context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

bool CollectStableSmokeRoster(SimulationContext *context,
                              std::vector<Smoke *> *objects)
{
    if (context == NULL || objects == NULL ||
        g_arena.getContext() != context)
        return false;
    objects->clear();
    for (ct_Subject *subject = __classTable.findFirstSubject();
         subject != NULL; subject = __classTable.findNextSubject(subject))
        objects->push_back(static_cast<Smoke *>(subject));
    std::sort(objects->begin(), objects->end(),
              [context](const Smoke *left, const Smoke *right)
              {
                  const std::string leftName = SmokeObjectName(
                      context, left->getObjectID());
                  const std::string rightName = SmokeObjectName(
                      context, right->getObjectID());
                  if (leftName != rightName)
                      return leftName < rightName;
                  return left->getObjectID().id < right->getObjectID().id;
              });
    return true;
}

bool ValidateStableSmokeBlob(const StableSmokeBlob &blob)
{
    if (!std::isfinite(blob.phase) || blob.phase < 0.0 ||
        !SmokeFiniteVector(blob.startPosition) ||
        !SmokeFiniteVector(blob.position) ||
        !SmokeFiniteVector(blob.direction) ||
        !SmokeFiniteVector(blob.offsetDirection) ||
        !std::isfinite(blob.directionIncrement) ||
        blob.directionIncrement < 0.0 ||
        !std::isfinite(blob.maximumTimeLife) ||
        blob.maximumTimeLife <= 0.0 || !std::isfinite(blob.radius) ||
        blob.radius < 0.001 || blob.alpha <= 0 || blob.alpha > 255 ||
        !std::isfinite(blob.radiusA) || !std::isfinite(blob.radiusB) ||
        !std::isfinite(blob.radiusC) || !std::isfinite(blob.opacityA) ||
        !std::isfinite(blob.opacityB) || !std::isfinite(blob.opacityC) ||
        !std::isfinite(blob.spline0) || !std::isfinite(blob.spline1) ||
        !std::isfinite(blob.spline2) || !std::isfinite(blob.spline3))
        return FailSmokeActiveWorld("SMK1 blob state is invalid");
    return true;
}

bool ValidateStableSmokeRecord(const StableSmokeRecord &record)
{
    if (record.name.empty() || record.attribute.empty() ||
        record.name.size() > kMaximumActiveWorldString ||
        record.attribute.size() > kMaximumActiveWorldString)
        return FailSmokeActiveWorld(
            "SMK1 owner/attribute identity is invalid");
    if (!SmokeFiniteVector(record.position) ||
        !std::isfinite(record.previousTimeStamp) ||
        record.previousTimeStamp < 0.1 ||
        !std::isfinite(record.movingTimeStamp) ||
        record.movingTimeStamp <= record.previousTimeStamp ||
        (record.removeOnMove != 0 && record.removeOnMove != 1) ||
        record.blobs.empty() ||
        record.blobs.size() > Smoke::MAXSMOKEBLOB)
        return FailSmokeActiveWorld("SMK1 owner/MOVING state is invalid");
    for (std::size_t index = 0; index < record.blobs.size(); ++index)
        if (!ValidateStableSmokeBlob(record.blobs[index]))
            return false;
    return true;
}

StableSmokeBlob CaptureStableSmokeBlob(const SmokeBlob &source)
{
    StableSmokeBlob blob;
    blob.phase = source.m_phase;
    blob.color = static_cast<std::uint32_t>(source.m_color);
    blob.u0 = source.u0;
    blob.v0 = source.v0;
    blob.u1 = source.u1;
    blob.v1 = source.v1;
    blob.startPosition = source.m_startPos;
    blob.position = source.m_position;
    blob.direction = source.m_dir;
    blob.offsetDirection = source.m_ofsDir;
    blob.directionIncrement = source.m_dirIncrement;
    blob.maximumTimeLife = source.m_maxTimeLife;
    blob.radius = source.m_radius;
    blob.alpha = source.m_alpha;
    blob.radiusA = source.rA;
    blob.radiusB = source.rB;
    blob.radiusC = source.rC;
    blob.opacityA = source.tA;
    blob.opacityB = source.tB;
    blob.opacityC = source.tC;
    blob.spline0 = source.a0;
    blob.spline1 = source.a1;
    blob.spline2 = source.a2;
    blob.spline3 = source.a3;
    return blob;
}

void ApplyStableSmokeBlob(const StableSmokeBlob &source,
                          GR_HTEXTURE texture, SmokeBlob *blob)
{
    *blob = SmokeBlob();
    blob->m_phase = source.phase;
    blob->m_color = source.color;
    blob->u0 = source.u0;
    blob->v0 = source.v0;
    blob->u1 = source.u1;
    blob->v1 = source.v1;
    blob->m_startPos = source.startPosition;
    blob->m_position = source.position;
    blob->m_dir = source.direction;
    blob->m_ofsDir = source.offsetDirection;
    blob->m_dirIncrement = source.directionIncrement;
    blob->m_maxTimeLife = source.maximumTimeLife;
    blob->m_radius = source.radius;
    blob->m_alpha = source.alpha;
    blob->m_ref = texture;
    blob->rA = source.radiusA;
    blob->rB = source.radiusB;
    blob->rC = source.radiusC;
    blob->tA = source.opacityA;
    blob->tB = source.opacityB;
    blob->tC = source.opacityC;
    blob->a0 = source.spline0;
    blob->a1 = source.spline1;
    blob->a2 = source.spline2;
    blob->a3 = source.spline3;
}

bool CaptureStableSmokeRecord(SimulationContext *context, Smoke *object,
                              StableSmokeRecord *record)
{
    if (context == NULL || object == NULL || record == NULL ||
        object->m_dynamicPublished || object->m_attr == NULL ||
        !SmokeAttributeCanSimulate(object->m_attr) ||
        object->m_cnt <= 0 || object->m_cnt > Smoke::MAXSMOKEBLOB ||
        object->m_cnt > object->m_attr->m_maxBlob)
        return FailSmokeActiveWorld(
            "live Smoke is not at a stable simulated frame boundary");
    record->name = SmokeObjectName(context, object->getObjectID());
    record->attribute = SmokeObjectName(context, object->m_smokeAttrID);
    record->viewIterations = object->m_viewIter;
    record->position = object->m_pos;
    record->previousTimeStamp = object->m_prevTimeStamp;
    record->removeOnMove = object->m_setRemove;
    record->blobs.clear();
    for (int index = 0; index < object->m_cnt; ++index)
    {
        if (object->m_blob[index].m_ref != object->m_attr->m_cacheImage)
            return FailSmokeActiveWorld(
                "live Smoke blob texture does not match its attribute");
        record->blobs.push_back(
            CaptureStableSmokeBlob(object->m_blob[index]));
    }
    KR_Event moving[2];
    const int count = context->copyEvents(
        fou_EVC_MOVING, object->getObjectID(), moving, 2);
    if (count != 1 || moving[0].source != object->getObjectID() ||
        moving[0].destination != object->getObjectID())
        return FailSmokeActiveWorld(
            "live Smoke private MOVING boundary is invalid");
    // MOVING never reads its inherited START payload. SMK1 owns the semantic
    // label/time edge and deliberately reconstructs an empty canonical body.
    record->movingTimeStamp = moving[0].timeStamp;
    return ValidateStableSmokeRecord(*record);
}

bool CollectStableSmokeRecords(
    SimulationContext *context, std::vector<StableSmokeRecord> *records)
{
    std::vector<Smoke *> objects;
    if (records == NULL || !CollectStableSmokeRoster(context, &objects))
        return false;
    records->clear();
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        StableSmokeRecord record;
        if (!CaptureStableSmokeRecord(context, objects[index], &record))
            return false;
        records->push_back(record);
    }
    return true;
}

bool StableSmokeRosterMatches(
    const std::vector<Smoke *> &objects, SimulationContext *context,
    const std::vector<StableSmokeRecord> &records)
{
    if (objects.size() != records.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (SmokeObjectName(context, objects[index]->getObjectID()) !=
            records[index].name)
            return false;
    return true;
}

void SmokePutU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void SmokePutDouble(std::vector<unsigned char> *bytes, double value)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 0; shift < 64; shift += 8)
        bytes->push_back(static_cast<unsigned char>(bits >> shift));
}

void SmokePutVector(std::vector<unsigned char> *bytes,
                    const CFVector3 &value)
{
    SmokePutDouble(bytes, value.x);
    SmokePutDouble(bytes, value.y);
    SmokePutDouble(bytes, value.z);
}

bool SmokePutString(std::vector<unsigned char> *bytes,
                    const std::string &value)
{
    if (bytes == NULL || value.empty() ||
        value.size() > kMaximumActiveWorldString ||
        value.find('\0') != std::string::npos)
        return false;
    SmokePutU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
}

bool SmokeGetU32(const std::vector<unsigned char> &bytes,
                 std::size_t *offset, std::uint32_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 4)
        return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
    return true;
}

bool SmokeGetDouble(const std::vector<unsigned char> &bytes,
                    std::size_t *offset, double *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 8)
        return false;
    std::uint64_t bits = 0;
    for (int shift = 0; shift < 64; shift += 8)
        bits |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
}

bool SmokeGetVector(const std::vector<unsigned char> &bytes,
                    std::size_t *offset, CFVector3 *value)
{
    return value != NULL && SmokeGetDouble(bytes, offset, &value->x) &&
           SmokeGetDouble(bytes, offset, &value->y) &&
           SmokeGetDouble(bytes, offset, &value->z);
}

bool SmokeGetString(const std::vector<unsigned char> &bytes,
                    std::size_t *offset, std::string *value)
{
    std::uint32_t size = 0;
    if (offset == NULL || value == NULL ||
        !SmokeGetU32(bytes, offset, &size) || size == 0 ||
        size > kMaximumActiveWorldString || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    value->assign(reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return value->find('\0') == std::string::npos;
}

void SmokePutBlob(std::vector<unsigned char> *bytes,
                  const StableSmokeBlob &blob)
{
    SmokePutDouble(bytes, blob.phase);
    SmokePutU32(bytes, blob.color);
    SmokePutU32(bytes, static_cast<std::uint32_t>(blob.u0));
    SmokePutU32(bytes, static_cast<std::uint32_t>(blob.v0));
    SmokePutU32(bytes, static_cast<std::uint32_t>(blob.u1));
    SmokePutU32(bytes, static_cast<std::uint32_t>(blob.v1));
    SmokePutVector(bytes, blob.startPosition);
    SmokePutVector(bytes, blob.position);
    SmokePutVector(bytes, blob.direction);
    SmokePutVector(bytes, blob.offsetDirection);
    SmokePutDouble(bytes, blob.directionIncrement);
    SmokePutDouble(bytes, blob.maximumTimeLife);
    SmokePutDouble(bytes, blob.radius);
    SmokePutU32(bytes, static_cast<std::uint32_t>(blob.alpha));
    SmokePutDouble(bytes, blob.radiusA);
    SmokePutDouble(bytes, blob.radiusB);
    SmokePutDouble(bytes, blob.radiusC);
    SmokePutDouble(bytes, blob.opacityA);
    SmokePutDouble(bytes, blob.opacityB);
    SmokePutDouble(bytes, blob.opacityC);
    SmokePutDouble(bytes, blob.spline0);
    SmokePutDouble(bytes, blob.spline1);
    SmokePutDouble(bytes, blob.spline2);
    SmokePutDouble(bytes, blob.spline3);
}

bool SmokeGetBlob(const std::vector<unsigned char> &bytes,
                  std::size_t *offset, StableSmokeBlob *blob)
{
    std::uint32_t color = 0, u0 = 0, v0 = 0, u1 = 0, v1 = 0, alpha = 0;
    if (blob == NULL || !SmokeGetDouble(bytes, offset, &blob->phase) ||
        !SmokeGetU32(bytes, offset, &color) ||
        !SmokeGetU32(bytes, offset, &u0) ||
        !SmokeGetU32(bytes, offset, &v0) ||
        !SmokeGetU32(bytes, offset, &u1) ||
        !SmokeGetU32(bytes, offset, &v1) ||
        !SmokeGetVector(bytes, offset, &blob->startPosition) ||
        !SmokeGetVector(bytes, offset, &blob->position) ||
        !SmokeGetVector(bytes, offset, &blob->direction) ||
        !SmokeGetVector(bytes, offset, &blob->offsetDirection) ||
        !SmokeGetDouble(bytes, offset, &blob->directionIncrement) ||
        !SmokeGetDouble(bytes, offset, &blob->maximumTimeLife) ||
        !SmokeGetDouble(bytes, offset, &blob->radius) ||
        !SmokeGetU32(bytes, offset, &alpha) ||
        !SmokeGetDouble(bytes, offset, &blob->radiusA) ||
        !SmokeGetDouble(bytes, offset, &blob->radiusB) ||
        !SmokeGetDouble(bytes, offset, &blob->radiusC) ||
        !SmokeGetDouble(bytes, offset, &blob->opacityA) ||
        !SmokeGetDouble(bytes, offset, &blob->opacityB) ||
        !SmokeGetDouble(bytes, offset, &blob->opacityC) ||
        !SmokeGetDouble(bytes, offset, &blob->spline0) ||
        !SmokeGetDouble(bytes, offset, &blob->spline1) ||
        !SmokeGetDouble(bytes, offset, &blob->spline2) ||
        !SmokeGetDouble(bytes, offset, &blob->spline3))
        return false;
    blob->color = color;
    blob->u0 = static_cast<int>(static_cast<std::int32_t>(u0));
    blob->v0 = static_cast<int>(static_cast<std::int32_t>(v0));
    blob->u1 = static_cast<int>(static_cast<std::int32_t>(u1));
    blob->v1 = static_cast<int>(static_cast<std::int32_t>(v1));
    blob->alpha = static_cast<int>(static_cast<std::int32_t>(alpha));
    return ValidateStableSmokeBlob(*blob);
}

bool SmokePutRecord(std::vector<unsigned char> *bytes,
                    const StableSmokeRecord &record)
{
    if (!SmokePutString(bytes, record.name) ||
        !SmokePutString(bytes, record.attribute))
        return false;
    SmokePutU32(bytes, static_cast<std::uint32_t>(record.viewIterations));
    SmokePutVector(bytes, record.position);
    SmokePutDouble(bytes, record.previousTimeStamp);
    SmokePutU32(bytes, static_cast<std::uint32_t>(record.removeOnMove));
    SmokePutDouble(bytes, record.movingTimeStamp);
    SmokePutU32(bytes, static_cast<std::uint32_t>(record.blobs.size()));
    for (std::size_t index = 0; index < record.blobs.size(); ++index)
        SmokePutBlob(bytes, record.blobs[index]);
    return true;
}

bool SmokeGetRecord(const std::vector<unsigned char> &bytes,
                    std::size_t *offset, StableSmokeRecord *record)
{
    std::uint32_t view = 0, remove = 0, count = 0;
    if (record == NULL ||
        !SmokeGetString(bytes, offset, &record->name) ||
        !SmokeGetString(bytes, offset, &record->attribute) ||
        !SmokeGetU32(bytes, offset, &view) ||
        !SmokeGetVector(bytes, offset, &record->position) ||
        !SmokeGetDouble(bytes, offset, &record->previousTimeStamp) ||
        !SmokeGetU32(bytes, offset, &remove) ||
        !SmokeGetDouble(bytes, offset, &record->movingTimeStamp) ||
        !SmokeGetU32(bytes, offset, &count) || count == 0 ||
        count > Smoke::MAXSMOKEBLOB)
        return false;
    record->viewIterations =
        static_cast<int>(static_cast<std::int32_t>(view));
    record->removeOnMove = static_cast<int>(remove);
    record->blobs.clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableSmokeBlob blob;
        if (!SmokeGetBlob(bytes, offset, &blob))
            return false;
        record->blobs.push_back(blob);
    }
    return ValidateStableSmokeRecord(*record);
}

bool EncodeStableSmokeRecords(
    const std::vector<StableSmokeRecord> &records,
    std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumActiveWorldSmokes)
        return false;
    bytes->clear();
    SmokePutU32(bytes, kSmokeActiveWorldMagic);
    SmokePutU32(bytes, kSmokeActiveWorldVersion);
    SmokePutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ValidateStableSmokeRecord(records[index]) ||
            (index != 0 && records[index - 1].name > records[index].name) ||
            !SmokePutRecord(bytes, records[index]))
            return FailSmokeActiveWorld("SMK1 record encoding failed");
    return true;
}

bool DecodeStableSmokeRecords(
    const std::vector<unsigned char> &bytes,
    std::vector<StableSmokeRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !SmokeGetU32(bytes, &offset, &magic) ||
        !SmokeGetU32(bytes, &offset, &version) ||
        !SmokeGetU32(bytes, &offset, &count) ||
        magic != kSmokeActiveWorldMagic ||
        version != kSmokeActiveWorldVersion ||
        count > kMaximumActiveWorldSmokes)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableSmokeRecord record;
        if (!SmokeGetRecord(bytes, &offset, &record) ||
            (!records->empty() && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

int DrainPrivateSmokeEvents(SimulationContext *context,
                            const KR_ObjectID &object)
{
    if (context == NULL || SmokeIsNul(object))
        return 0;
    int removed = 0;
    while (context->removeEvent(fou_EVC_MOVING, object) == 1)
        ++removed;
    while (context->removeEvent(fou_EVCMD_START, object) == 1)
        ++removed;
    while (context->removeEvent(fou_EVCMD_START_WITHDIR, object) == 1)
        ++removed;
    return removed;
}

void RemoveSmokeIfPresent(SimulationContext *context,
                          const KR_ObjectID &object)
{
    if (context != NULL && !SmokeIsNul(object) &&
        context->isExist(object))
        context->removeObject(object);
}

void SmokeSubjectHashBytes(unsigned long long &hash, const void *data,
                           int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kSmokeSubjectHashPrime;
    }
}

void SmokeSubjectHashString(unsigned long long &hash, const char *value)
{
    SmokeSubjectHashBytes(hash, value,
                          static_cast<int>(strlen(value)) + 1);
}

}  // namespace

void SmokeSubjectState_Link()
{
}

bool SmokeSubjectState_TableReady(SimulationContext *context,
                                  int expectedCapacity)
{
    if (context == NULL || expectedCapacity <= 0 ||
        g_arena.getContext() != context)
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Smoke");
    return table != ct_NULLID &&
           table == __classTable.getClassTableID() &&
           __classTable.capacity() == expectedCapacity &&
           __classTable.isRendering();
}

int SmokeSubjectState_Capacity()
{
    return __classTable.capacity();
}

int SmokeSubjectState_LiveCount()
{
    int count = 0;
    for (ct_Subject *subject = __classTable.findFirstSubject();
         subject != NULL;
         subject = __classTable.findNextSubject(subject))
        ++count;
    return count;
}

unsigned long long SmokeSubjectState_Fingerprint(SimulationContext *context)
{
    const int capacity = SmokeSubjectState_Capacity();
    if (!SmokeSubjectState_TableReady(context, capacity) ||
        SmokeSubjectState_LiveCount() != 0)
        return 0;
    unsigned long long hash = kSmokeSubjectHashOffset;
    SmokeSubjectHashString(hash, "Smoke");
    SmokeSubjectHashBytes(hash, &capacity, sizeof(capacity));
    const int rendering = __classTable.isRendering() ? 1 : 0;
    SmokeSubjectHashBytes(hash, &rendering, sizeof(rendering));
#ifdef RR2NW_SMOKE_SUBJECT_ONLY
    const int simulation = 0;
#else
    const int simulation = 1;
#endif
    SmokeSubjectHashBytes(hash, &simulation, sizeof(simulation));
    const int terrainPlacement = 1;
    SmokeSubjectHashBytes(hash, &terrainPlacement,
                          sizeof(terrainPlacement));
#ifdef RR2NW_SMOKE_RENDER_DISABLED
    const int visibleRendering = 0;
#else
    const int visibleRendering = 1;
#endif
    SmokeSubjectHashBytes(hash, &visibleRendering,
                          sizeof(visibleRendering));
    return hash;
}

bool SmokeSubjectState_ProbeLifecycle(SimulationContext *context)
{
    const int capacity = SmokeSubjectState_Capacity();
    static const char kProbeName[] = "Smoke.Lifecycle.Probe";
    if (!SmokeSubjectState_TableReady(context, capacity) ||
        SmokeSubjectState_LiveCount() != 0 ||
        context->isExist(kProbeName))
        return false;

    KR_ObjectID object =
        g_arena.newObject(__classTable.getClassTableID(), kProbeName);
    Smoke *smoke = static_cast<Smoke *>(__classTable.findFirstSubject());
    if (object.isNUL() || smoke == NULL || smoke->getObjectID() != object ||
        SmokeSubjectState_LiveCount() != 1)
    {
        if (!object.isNUL())
            context->removeObject(object);
        return false;
    }

    bool blobsInitialized = true;
    for (int i = 0; i < Smoke::MAXSMOKEBLOB; ++i)
    {
        const SmokeBlob &blob = smoke->m_blob[i];
        blobsInitialized = blobsInitialized && blob.m_phase == 0 &&
                           blob.m_color == 0 && blob.u0 == 0 &&
                           blob.v0 == 0 && blob.u1 == 0 && blob.v1 == 0 &&
                           blob.m_radius == 0 && blob.m_alpha == 0 &&
                           blob.m_ref == NULL;
    }
    const bool initialized = smoke->m_attr == NULL && smoke->m_cnt == 0 &&
                             smoke->m_viewObj.m_master == smoke &&
                             !smoke->m_viewObj.m_visible &&
                             smoke->m_viewObj.m_z == 0 &&
                             smoke->m_setRemove == 0 &&
                             smoke->m_smokeAttrID.isNUL() &&
                             blobsInitialized;
    context->removeObject(object);
    return initialized && !context->isExist(kProbeName) &&
           SmokeSubjectState_LiveCount() == 0;
}

bool SmokeSubjectState_ProbeSimulationLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp)
{
    if (!SmokeSubjectState_SimulationSupported(context, attributeName) ||
        SmokeSubjectState_LiveCount() != 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoke *attribute = static_cast<AttributeSmoke *>(
        __attrTable.searchAttribute(attributeID));

    static const char kProbeName[] = "Smoke.Simulation.Probe";
    if (context->isExist(kProbeName))
        return false;
    KR_ObjectID object =
        g_arena.newObject(__classTable.getClassTableID(), kProbeName);
    Smoke *smoke = static_cast<Smoke *>(__classTable.findFirstSubject());
    if (object.isNUL() || smoke == NULL || smoke->getObjectID() != object ||
        SmokeSubjectState_LiveCount() != 1)
    {
        if (!object.isNUL())
            context->removeObject(object);
        return false;
    }

    const CFVector3 position(17.0, 3.0, -19.0);
    CFVector3 expectedPosition = position;
    if (!SmokePlaceOnTerrain(attribute, expectedPosition))
    {
        context->removeObject(object);
        return false;
    }
    KR_Event event;
    event.source = g_arena.getObjectID();
    event.destination = object;
    event.timeStamp = timeStamp < 0.1 ? 0.1 : timeStamp;
    const double startTimeStamp = event.timeStamp;
    event.label = attribute->m_onLand ? fou_EVCMD_START_WITHDIR
                                      : fou_EVCMD_START;
    event.data.open(EDO_WRITE)
              .putObjectID(attributeID)
              .putDouble(position.x)
              .putDouble(position.y)
              .putDouble(position.z);
    if (attribute->m_onLand)
        event.data.putDouble(0.0).putDouble(1.0).putDouble(0.0);
    event.data.close();
    context->sendEventNow(event);

    bool blobsStarted = context->isExist(kProbeName) &&
                        smoke->m_cnt == attribute->m_maxBlob;
    for (int i = 0; blobsStarted && i < smoke->m_cnt; ++i)
    {
        const SmokeBlob &blob = smoke->m_blob[i];
        blobsStarted = blob.m_phase == 0 && blob.m_radius > 0 &&
                       blob.m_alpha > 0 &&
                       blob.m_maxTimeLife > 0;
    }
    const bool started = blobsStarted && smoke->m_attr == attribute &&
                         smoke->m_smokeAttrID == attributeID &&
                         smoke->m_pos == expectedPosition &&
                         smoke->m_prevTimeStamp == startTimeStamp &&
                         smoke->m_setRemove == 0;
    const bool firstMoveScheduled =
        context->removeEvent(fou_EVC_MOVING, object) != 0;
    if (!started || !firstMoveScheduled)
    {
        context->removeEvent(fou_EVC_MOVING, object);
        if (context->isExist(kProbeName))
            context->removeObject(object);
        return false;
    }

    const CFVector3 previousPosition = smoke->m_blob[0].m_position;
    event.label = fou_EVC_MOVING;
    event.source = object;
    event.destination = object;
    event.timeStamp = startTimeStamp + attribute->m_timeIncrement;
    const double moveTimeStamp = event.timeStamp;
    context->sendEventNow(event);
    const bool moved = context->isExist(kProbeName) && smoke->m_cnt > 0 &&
                       smoke->m_blob[0].m_phase > 0 &&
                       smoke->m_blob[0].m_position != previousPosition &&
                       smoke->m_prevTimeStamp == moveTimeStamp;
    const bool secondMoveScheduled =
        context->removeEvent(fou_EVC_MOVING, object) != 0;
    if (!moved || !secondMoveScheduled)
    {
        context->removeEvent(fou_EVC_MOVING, object);
        if (context->isExist(kProbeName))
            context->removeObject(object);
        return false;
    }

    smoke->onHide(event.timeStamp);
    event.timeStamp += attribute->m_timeIncrement;
    context->sendEventNow(event);
    return !context->isExist(kProbeName) &&
           SmokeSubjectState_LiveCount() == 0 &&
           context->removeEvent(fou_EVC_MOVING, object) == 0;
}

bool SmokeSubjectState_SimulationSupported(
    SimulationContext *context, const char *attributeName)
{
    const int capacity = SmokeSubjectState_Capacity();
    if (!SmokeSubjectState_TableReady(context, capacity) ||
        attributeName == NULL || attributeName[0] == 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoke *attribute = attributeID.isNUL()
        ? NULL
        : static_cast<AttributeSmoke *>(
              __attrTable.searchAttribute(attributeID));
    return SmokeAttributeCanSimulate(attribute) &&
           SmokeTerrainReady(attribute);
}

bool SmokeSubjectState_RenderingSupported(
    SimulationContext *context, const char *attributeName)
{
#ifdef RR2NW_SMOKE_RENDER_DISABLED
    (void)context;
    (void)attributeName;
    return false;
#else
    if (!SmokeSubjectState_SimulationSupported(context, attributeName) ||
        _pGRDrawAlphaSprite == NULL)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoke *attribute = attributeID.isNUL()
        ? NULL
        : static_cast<AttributeSmoke *>(
              __attrTable.searchAttribute(attributeID));
    return attribute != NULL && attribute->m_cacheImage != NULL;
#endif
}

void SmokeActiveWorldState_Link()
{
    SmokeSubjectState_Link();
}

const char *SmokeActiveWorldState_LastFailure()
{
    return g_smokeActiveWorldFailure.c_str();
}

int SmokeActiveWorldState_BlobCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableSmokeRecord> records;
    if (!DecodeStableSmokeRecords(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        count += static_cast<int>(records[index].blobs.size());
    return count;
}

int SmokeActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableSmokeRecord> records;
    return DecodeStableSmokeRecords(bytes, &records)
        ? static_cast<int>(records.size()) : -1;
}

unsigned long long SmokeActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!SmokeActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = kSmokeSubjectHashOffset;
    if (!bytes.empty())
        SmokeSubjectHashBytes(hash, &bytes[0],
                              static_cast<int>(bytes.size()));
    return hash;
}

bool SmokeActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_smokeActiveWorldFailure.clear();
    std::vector<StableSmokeRecord> records;
    if (!CollectStableSmokeRecords(context, &records))
    {
        if (g_smokeActiveWorldFailure.empty())
            FailSmokeActiveWorld("Smoke stable roster collection failed");
        return false;
    }
    return EncodeStableSmokeRecords(records, bytes);
}

bool SmokeActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableSmokeRecord> records;
    return DecodeStableSmokeRecords(bytes, &records);
}

bool SmokeActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return SmokeActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool SmokeActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableSmokeRecord> records;
    std::vector<Smoke *> objects;
    if (owners == NULL || !DecodeStableSmokeRecords(bytes, &records) ||
        !CollectStableSmokeRoster(context, &objects) ||
        !StableSmokeRosterMatches(objects, context, records))
        return false;
    owners->clear();
    for (std::size_t index = 0; index < objects.size(); ++index)
        owners->push_back(objects[index]->getObjectID());
    return true;
}

bool SmokeActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableSmokeRecord> records;
    std::vector<Smoke *> objects;
    if (created == NULL || !DecodeStableSmokeRecords(bytes, &records) ||
        !CollectStableSmokeRoster(context, &objects))
        return false;
    if (!created->empty())
        return FailSmokeActiveWorld("Smoke created-owner list is not empty");
    if (!objects.empty())
        return StableSmokeRosterMatches(objects, context, records);
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Smoke");
    if ((!records.empty() && table == ct_NULLID) ||
        static_cast<int>(records.size()) >
            __classTable.capacity() - SmokeSubjectState_LiveCount())
        return FailSmokeActiveWorld(
            "Smoke owner table has insufficient capacity");
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        KR_ObjectID object =
            g_arena.newObject(table, records[index].name.c_str());
        if (SmokeIsNul(object) || __classTable.find(object) == NULL)
        {
            SmokeActiveWorldState_RemoveStableOwners(context, created);
            return FailSmokeActiveWorld("Smoke owner allocation failed");
        }
        created->push_back(object);
    }
    objects.clear();
    if (!CollectStableSmokeRoster(context, &objects) ||
        !StableSmokeRosterMatches(objects, context, records))
    {
        SmokeActiveWorldState_RemoveStableOwners(context, created);
        return FailSmokeActiveWorld(
            "Smoke allocated roster is not canonical");
    }
    return true;
}

bool SmokeActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableSmokeRecord> records;
    std::vector<Smoke *> objects;
    if (context == NULL || !DecodeStableSmokeRecords(bytes, &records) ||
        !CollectStableSmokeRoster(context, &objects) ||
        !StableSmokeRosterMatches(objects, context, records))
        return false;
    std::vector<AttributeSmoke *> attributes(records.size(), NULL);
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const KR_ObjectID attributeID =
            context->searchObject(records[index].attribute.c_str());
        attributes[index] = SmokeIsNul(attributeID) ? NULL :
            static_cast<AttributeSmoke *>(
                __attrTable.searchAttribute(attributeID));
        if (!SmokeAttributeCanSimulate(attributes[index]))
            return FailSmokeActiveWorld(
                "SMK1 SmokeAttr dependency is unresolved");
        if (records[index].blobs.size() >
            static_cast<std::size_t>(attributes[index]->m_maxBlob))
            return FailSmokeActiveWorld(
                "SMK1 blob roster exceeds the resolved SmokeAttr");
        for (std::size_t blob = 0;
             blob < records[index].blobs.size(); ++blob)
            if (records[index].blobs[blob].phase >
                attributes[index]->m_maxTimeLife)
                return FailSmokeActiveWorld(
                    "SMK1 blob phase exceeds the resolved SmokeAttr");
    }
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        if (objects[index]->m_dynamicPublished)
            return FailSmokeActiveWorld(
                "SMK1 cannot replace a frame-published Smoke");
        DrainPrivateSmokeEvents(context, objects[index]->getObjectID());
        objects[index]->resetTransientState();
    }
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        Smoke *object = objects[index];
        const StableSmokeRecord &record = records[index];
        object->m_smokeAttrID = attributes[index]->getObjectID();
        object->m_viewIter = record.viewIterations;
        object->m_pos = record.position;
        object->m_prevTimeStamp = record.previousTimeStamp;
        object->m_setRemove = record.removeOnMove;
        object->m_attr = attributes[index];
        object->m_cnt = static_cast<int>(record.blobs.size());
        object->m_viewObj.m_master = object;
        object->m_viewObj.m_visible = false;
        object->m_viewObj.m_z = 0;
        for (int blob = 0; blob < object->m_cnt; ++blob)
            ApplyStableSmokeBlob(record.blobs[blob],
                                 attributes[index]->m_cacheImage,
                                 &object->m_blob[blob]);
        KR_Event moving;
        moving.label = fou_EVC_MOVING;
        moving.source = object->getObjectID();
        moving.destination = object->getObjectID();
        moving.timeStamp = record.movingTimeStamp;
        moving.data.open(EDO_WRITE).close();
        context->addEvent(moving);
    }
    std::vector<unsigned char> current;
    if (!SmokeActiveWorldState_CaptureStable(context, &current) ||
        current != bytes)
        return FailSmokeActiveWorld("SMK1 canonical recapture differs");
    return true;
}

void SmokeActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            DrainPrivateSmokeEvents(context, *object);
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    created->clear();
}

bool SmokeActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, const char *attributeName,
    double timeStamp, SmokeActiveWorldProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    g_smokeActiveWorldFailure.clear();
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || SmokeSubjectState_LiveCount() != 0 ||
        !SmokeSubjectState_RenderingSupported(context, attributeName))
        return FailSmokeActiveWorld(
            "Smoke active-world probe requires an empty ready table");
    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeSmoke *attribute = SmokeIsNul(attributeID) ? NULL :
        static_cast<AttributeSmoke *>(
            __attrTable.searchAttribute(attributeID));
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Smoke");
    if (!SmokeAttributeCanSimulate(attribute) ||
        subjectTable == ct_NULLID || attribute->m_maxBlob != 1)
        return FailSmokeActiveWorld(
            "Smoke active-world probe attribute/table is unavailable");

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    KR_ObjectID originalFirst = KR_ObjectID::NUL();
    KR_ObjectID originalSecond = KR_ObjectID::NUL();
    KR_ObjectID stagedFirst = KR_ObjectID::NUL();
    KR_ObjectID stagedSecond = KR_ObjectID::NUL();
    KR_ObjectID restoredFirst = KR_ObjectID::NUL();
    KR_ObjectID restoredSecond = KR_ObjectID::NUL();
    std::vector<KR_ObjectID> originalOwners;
    std::vector<KR_ObjectID> staged;
    std::vector<KR_ObjectID> restored;
    bool success = false;

    const auto startSmoke =
        [context, subjectTable, attributeID](
            const CFVector3 &position, const CFVector3 &direction,
            double startTime, KR_ObjectID *owner)
        {
            *owner = g_arena.newObject(
                subjectTable, "Smoke.ActiveWorld.Probe");
            if (SmokeIsNul(*owner) || !context->isExist(*owner))
                return false;
            KR_Event event;
            event.label = fou_EVCMD_START_WITHDIR;
            event.source = g_arena.getObjectID();
            event.destination = *owner;
            event.timeStamp = startTime;
            event.data.open(EDO_WRITE)
                      .putObjectID(attributeID)
                      .putDouble(position.x)
                      .putDouble(position.y)
                      .putDouble(position.z)
                      .putDouble(direction.x)
                      .putDouble(direction.y)
                      .putDouble(direction.z)
                      .close();
            context->sendEventNow(event);
            return context->isExist(*owner) != 0;
        };
    const auto advanceSmoke =
        [context](const KR_ObjectID &owner)
        {
            Smoke *object = static_cast<Smoke *>(__classTable.find(owner));
            KR_Event moving[2];
            if (object == NULL || context->copyEvents(
                    fou_EVC_MOVING, owner, moving, 2) != 1 ||
                context->removeEvent(fou_EVC_MOVING, owner) != 1)
                return false;
            const double previous = object->m_prevTimeStamp;
            context->sendEventNow(moving[0]);
            return context->isExist(owner) &&
                   object->m_prevTimeStamp > previous;
        };

    do
    {
        if (!startSmoke(CFVector3(4096.0, 10000.0, -4096.0),
                        CFVector3(0.0, 1.0, 0.0), ts,
                        &originalFirst) ||
            !startSmoke(CFVector3(4104.0, 10008.0, -4104.0),
                        CFVector3(1.0, 1.0, 0.0), ts + 0.01,
                        &originalSecond) ||
            originalFirst == originalSecond)
        {
            FailSmokeActiveWorld(
                "Smoke active-world duplicate-name start failed");
            break;
        }
        if (!advanceSmoke(originalFirst) ||
            !advanceSmoke(originalSecond) ||
            !advanceSmoke(originalSecond))
        {
            FailSmokeActiveWorld(
                "Smoke active-world move setup failed");
            break;
        }

        std::vector<unsigned char> bytes;
        if (!SmokeActiveWorldState_CaptureStable(context, &bytes) ||
            SmokeActiveWorldState_BlobCount(bytes) != 2 ||
            SmokeActiveWorldState_SchedulerEventCount(bytes) != 2 ||
            !SmokeActiveWorldState_CollectStableOwners(
                context, bytes, &originalOwners) ||
            originalOwners.size() != 2 ||
            originalOwners[0] != originalFirst ||
            originalOwners[1] != originalSecond)
            break;
        unsigned long long fingerprint = kSmokeSubjectHashOffset;
        SmokeSubjectHashBytes(fingerprint, &bytes[0],
                              static_cast<int>(bytes.size()));
        SmokeActiveWorldState_RemoveStableOwners(
            context, &originalOwners);
        if (SmokeSubjectState_LiveCount() != 0)
        {
            FailSmokeActiveWorld("SMK1 original teardown failed");
            break;
        }

        if (!SmokeActiveWorldState_CreateStableOwners(
                context, bytes, &staged) || staged.size() != 2 ||
            staged[0] == originalFirst ||
            staged[0] == originalSecond ||
            staged[1] == originalFirst ||
            staged[1] == originalSecond ||
            !SmokeActiveWorldState_ApplyStableReferences(context, bytes) ||
            !SmokeActiveWorldState_MatchesStable(context, bytes))
        {
            FailSmokeActiveWorld("SMK1 staged reconstruction failed");
            break;
        }
        stagedFirst = staged[0];
        stagedSecond = staged[1];
        SmokeActiveWorldState_RemoveStableOwners(context, &staged);
        if (SmokeSubjectState_LiveCount() != 0)
        {
            FailSmokeActiveWorld(
                "SMK1 staged rollback retained state");
            break;
        }

        if (!SmokeActiveWorldState_CreateStableOwners(
                context, bytes, &restored) || restored.size() != 2 ||
            restored[0] == originalFirst ||
            restored[0] == originalSecond ||
            restored[0] == stagedFirst ||
            restored[0] == stagedSecond ||
            restored[1] == originalFirst ||
            restored[1] == originalSecond ||
            restored[1] == stagedFirst ||
            restored[1] == stagedSecond ||
            !SmokeActiveWorldState_ApplyStableReferences(context, bytes) ||
            !SmokeActiveWorldState_MatchesStable(context, bytes))
        {
            FailSmokeActiveWorld("SMK1 final reconstruction failed");
            break;
        }
        restoredFirst = restored[0];
        restoredSecond = restored[1];
        Smoke *resumed = static_cast<Smoke *>(
            __classTable.find(restoredFirst));
        Smoke *unmoved = static_cast<Smoke *>(
            __classTable.find(restoredSecond));
        if (resumed == NULL || unmoved == NULL ||
            resumed->m_cnt != 1 || unmoved->m_cnt != 1)
        {
            FailSmokeActiveWorld("SMK1 restored blob state is unavailable");
            break;
        }
        const double resumedPhase = resumed->m_blob[0].m_phase;
        const double unmovedPhase = unmoved->m_blob[0].m_phase;
        const CFVector3 resumedPosition = resumed->m_blob[0].m_position;
        if (!advanceSmoke(restoredFirst) ||
            resumed->m_blob[0].m_phase <= resumedPhase ||
            resumed->m_blob[0].m_position == resumedPosition ||
            unmoved->m_blob[0].m_phase != unmovedPhase ||
            context->copyEvents(
                fou_EVC_MOVING, restoredFirst, NULL, 0) != 1)
        {
            FailSmokeActiveWorld(
                "SMK1 restored Smoke did not resume movement");
            break;
        }

        summary->capturedOwners = 2;
        summary->capturedBlobs = 2;
        summary->schedulerEvents = 2;
        summary->stagedRollbacks = 1;
        summary->reconstructedOwners = 2;
        summary->stableRoundTrips = 2;
        summary->resumedMoves = 1;
        summary->fingerprint = fingerprint;
        success = true;
    } while (false);

    SmokeActiveWorldState_RemoveStableOwners(context, &restored);
    SmokeActiveWorldState_RemoveStableOwners(context, &staged);
    SmokeActiveWorldState_RemoveStableOwners(context, &originalOwners);
    RemoveSmokeIfPresent(context, originalSecond);
    RemoveSmokeIfPresent(context, originalFirst);
    const int lateEvents =
        DrainPrivateSmokeEvents(context, originalFirst) +
        DrainPrivateSmokeEvents(context, originalSecond) +
        DrainPrivateSmokeEvents(context, stagedFirst) +
        DrainPrivateSmokeEvents(context, stagedSecond) +
        DrainPrivateSmokeEvents(context, restoredFirst) +
        DrainPrivateSmokeEvents(context, restoredSecond);
    const bool clean = SmokeSubjectState_LiveCount() == 0 &&
        lateEvents == 0;
    if (!success || !clean)
    {
        std::memset(summary, 0, sizeof(*summary));
        if (g_smokeActiveWorldFailure.empty())
            FailSmokeActiveWorld("SMK1 probe rollback was not clean");
        return false;
    }
    return true;
}
/* End of file C:\NW\ARENA\OBASE\Smoke\Smoke.cpp */
