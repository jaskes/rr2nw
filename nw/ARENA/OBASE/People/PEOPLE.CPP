/*
 * File  : C:\WinGame\OBASE\People\People.cpp
 * Autor :
 * Ver   1.0
 */
#include "People.h"
#include "PeopleContactResponse.h"
#include "PeopleObstacleRecovery.h"
#include "PeopleSupportSampling.h"
#include "PeopleRouteMotion.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "kernel/h/session.h"

#include "message/skinmsg.h"
#include "message/peopmsg.h"
#include "message/bulmsg.h"
#include "message/sndmsg.h"
#include "message/unitmsg.h"

#include "enum/spaceEnum.h"
#include "i/route.i"
#include "h/phisics.h"
#include "h/vehicle.h"
#include "h/ActorCadence.h"
#include "zav.h"
#include "Sound.h"
#include "obase/sound/wavobj.h"

static void AnimateCallBack0(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref);
static void AnimateCallBack2(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref);

enum
{
    NO_LAND  = 0,
    ON_LAND  = 1,
    ON_WATER = 2,
    ON_OBJ   = 3
};


void shoot( KR_ObjectID fromID,
            int bulletTable, int attrIndex,
            const CFVector3 &pos, const CFVector3 &dir,
            SimulationContext *context, double ts );


 //===========================================================================
class AttributePeople : public ct_Attribute
{
 public:
    CViewObjectModel*   m_cacheSkin              ;  //
    KR_ObjectID		m_skinID;
    virtual void        update(double ts);
    int                 m_bulletIndex;


    WAVObj            * m_wav;
    ct_ClassTableID     m_ctsndID;
    ct_ClassTableID     m_cacheExplosionTable;
    int                 m_cacheExplAttr;
    ct_ClassTableID     m_smokeTableID;
    KR_ObjectID         m_smokeAttrID;

    ct_ClassTableID     m_cacheCorpseTable;
    int                 m_cacheCorpseAttr;
    ct_ClassTableID     m_bulletTable;
    CFVector3           m_cannonOfs;


//{{ATTRIBUTE
    ct_AttrItem  m_array[38];
    ct_AttrStr      m_routeName              ;  // 
    double          m_timeInc                ;  // 
    int             m_onLand                 ;  // 
    ct_AttrStr      m_skinName               ;  // 
    double          m_speed                  ;  // 
    double          m_addRoll                ;  // 
    double          m_rollSpeed              ;  // 
    double          m_centerHeight           ;  // 
    ct_AttrStr      m_soundName              ;  // 
    ct_AttrStr      m_explAttrName           ;  // 
    ct_AttrStr      m_smokeAttrName          ;  // 
    ct_AttrStr      m_corpseAttrName         ;  // 
    double          m_initialDamage          ;  // 
    ct_AttrStr      m_bulletAttrName         ;  // 
    double          m_cannonSpeed            ;  // 
    double          m_cannonX                ;  // 
    double          m_cannonY                ;  // 
    double          m_cannonZ                ;  // 
    double          m_maxOutDist             ;  // ╨рёёЄю эшх, эр ъюЄюЁюх ьюцхь юЄющЄш
    int             m_stopIfAttack           ;  // 
    double          m_shootSkill             ;  //
    int             m_burstCount             ;  // ╩юышўхёЄтю яєы№ т т√яєёърхьющ юўхЁхфш
    double          m_deltaZeroSpeed         ;  // 
    double          m_maxDeep                ;  // 
    int             m_allwaysVertical        ;  // Retail spelling
    double          m_enableRadius           ;  //
    double          m_viewDist               ;  //
    double          m_power                  ;  //
    int             m_smeshable              ;  // Retail spelling
    int             m_killStyle              ;  //
    double          m_artYOffset             ;  //
    int             m_allwaysVisible         ;  // Retail spelling
    int             m_isRaceCar              ;  //
    double          m_axel0                  ;  // Retail spelling
    double          m_axel1                  ;  //
    double          m_axel2                  ;  //
    double          m_axel3                  ;  //
    double          m_maxRadius              ;  //

    AttributePeople()
    {
        strncpy(m_routeName,"", sizeof( ct_AttrStr )-1 );
        m_timeInc            = 0.04;
        m_onLand             = 1;
        strncpy(m_skinName,"", sizeof( ct_AttrStr )-1 );
        m_speed              = 10.0;
        m_addRoll            = 0;
        m_rollSpeed          = 3;
        m_centerHeight       = 0;
        strncpy(m_soundName,"", sizeof( ct_AttrStr )-1 );
        strncpy(m_explAttrName,"Unknown", sizeof( ct_AttrStr )-1 );
        strncpy(m_smokeAttrName,"Unknown", sizeof( ct_AttrStr )-1 );
        strncpy(m_corpseAttrName,"Corpse.Attr.Default", sizeof( ct_AttrStr )-1 );
        m_initialDamage      = 1.0;
        strncpy(m_bulletAttrName,"", sizeof( ct_AttrStr )-1 );
        m_cannonSpeed        = 0.8;
        m_cannonX            = 0;
        m_cannonY            = 1.4;
        m_cannonZ            = 0;
        m_maxOutDist         = 1e3;
        m_stopIfAttack       = 0;
        m_shootSkill         = 0.9;
        m_burstCount         = 5;
        m_deltaZeroSpeed     = 0.9;
        m_maxDeep            = 2.0;
        m_allwaysVertical    = 0;
        m_enableRadius       = 4.0;
        m_viewDist           = 200.0;
        m_power              = 0.1;
        m_smeshable          = 1;
        m_killStyle          = 0;
        m_artYOffset         = 10.0;
        m_allwaysVisible     = 0;
        m_isRaceCar          = 0;
        m_axel0              = 2.72;
        m_axel1              = 2.35;
        m_axel2              = 1.9;
        m_axel3              = 1.5;
        m_maxRadius          = 100.0;

        m_array[0].set("m_routeName",m_routeName);
        m_array[1].set("m_timeInc",m_timeInc);
        m_array[2].set("m_onLand",m_onLand);
        m_array[3].set("m_skinName",m_skinName);
        m_array[4].set("m_speed",m_speed);
        m_array[5].set("m_addRoll",m_addRoll);
        m_array[6].set("m_rollSpeed",m_rollSpeed);
        m_array[7].set("m_centerHeight",m_centerHeight);
        m_array[8].set("m_soundName",m_soundName);
        m_array[9].set("m_explAttrName",m_explAttrName);
        m_array[10].set("m_smokeAttrName",m_smokeAttrName);
        m_array[11].set("m_corpseAttrName",m_corpseAttrName);
        m_array[12].set("m_initialDamage",m_initialDamage);
        m_array[13].set("m_bulletAttrName",m_bulletAttrName);
        m_array[14].set("m_cannonSpeed",m_cannonSpeed);
        m_array[15].set("m_cannonX",m_cannonX);
        m_array[16].set("m_cannonY",m_cannonY);
        m_array[17].set("m_cannonZ",m_cannonZ);
        m_array[18].set("m_maxOutDist",m_maxOutDist);
        m_array[19].set("m_stopIfAttack",m_stopIfAttack);
        m_array[20].set("m_shootSkill",m_shootSkill);
        m_array[21].set("m_burstCount",m_burstCount);
        m_array[22].set("m_deltaZeroSpeed",m_deltaZeroSpeed);
        m_array[23].set("m_maxDeep",m_maxDeep);
        m_array[24].set("m_allwaysVertical",m_allwaysVertical);
        m_array[25].set("m_enableRadius",m_enableRadius);
        m_array[26].set("m_viewDist",m_viewDist);
        m_array[27].set("m_power",m_power);
        m_array[28].set("m_smeshable",m_smeshable);
        m_array[29].set("m_killStyle",m_killStyle);
        m_array[30].set("m_artYOffset",m_artYOffset);
        m_array[31].set("m_allwaysVisible",m_allwaysVisible);
        m_array[32].set("m_isRaceCar",m_isRaceCar);
        m_array[33].set("m_axel0",m_axel0);
        m_array[34].set("m_axel1",m_axel1);
        m_array[35].set("m_axel2",m_axel2);
        m_array[36].set("m_axel3",m_axel3);
        m_array[37].set("m_maxRadius",m_maxRadius);

        linkTable(m_array,38);
    }
//}}END_OF_ATTRIBUTE
};

//static AttributePeople __defaultAttr;

 //===========================================================================
class PeopleTable : public ct_SubjectTable
{
 private:
    People *m_table;
 public:
    PeopleTable()
    {
        m_table = NULL;
        registerClass( "People" );
        m_defaultAddMode = CT_KILLINVISIBLE;
    }
    ~PeopleTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    virtual bool       isRendering();
    virtual bool       isAudible();
    int                freeObjectCount() const;

};


bool PeopleTable::isRendering()
{
   return true;
}

bool PeopleTable::isAudible()
{
   return true;
}

int PeopleTable::freeObjectCount() const
{
    int count = 0;
    for (const ct_Object *object = m_freeList; object != NULL;
         object = object->next())
        ++count;
    return count;
}

 //===========================================================================
class AttributeTablePeople : public ct_AttributeTable
{
 protected:
    AttributePeople *m_table;

 public:
    AttributeTablePeople()
    {
       m_table = NULL;
       registerClass( "PeopleAttr" );
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
};

static PeopleTable  __classTable;
static AttributeTablePeople __attrTable;
 /*********************************
  *
  *   People implementation
  *
  *********************************/

inline void People::setHAngle( double hAngle )
{
    m_hAngle   = hAngle;
    m_rotateOy = m_attr->m_addRoll-M_PI/2-m_hAngle;
}

inline void People::setHAngleFixDir( double hAngle )
{
    m_hAngle   = hAngle;
    m_rotateOy = m_attr->m_addRoll-M_PI/2-m_hAngle;
    m_dir.x    = cos(m_hAngle)*m_attr->m_speed;
    m_dir.z    = sin(m_hAngle)*m_attr->m_speed;
}


 //============================================================
People::People()
    : m_viewDynObj(m_skin)
 {
    m_attr = 0;//&__defaultAttr;
 }

double People::movementSpeed() const
{
    return m_attr == 0 ? 0.0 : m_attr->m_speed;
}

double People::combatPower() const
{
    return m_attr == 0 ? 0.0 : m_attr->m_power;
}

 //============================================================
People::~People()
 {
 }


//==============================================================
void People::attackAllways(KR_Event &event, const CFVector3 &pos)
{
   m_nextNode = pos;
   event.timeStamp += 0.1;
}


//==============================================================
void People::nexNodeDefault(KR_Event &event)
{
    CFVector3 currentNode = m_nextNode;
    double    dist = 0.0;
    CFVector3 pos(getPosition());

    if(  m_attr == 0  )
         return;

    switch( m_attr->m_onLand )
    {
    case NO_LAND:   dist = Abs( currentNode-pos ); break;
    case ON_LAND:
    case ON_WATER:
    case ON_OBJ:    dist = hypot(currentNode.z-pos.z, currentNode.x-pos.x ); break;
    default: echo("People::Warning: unknown m_onLand");
    }

    if(  dist*(1.0+m_attr->m_rollSpeed)/(3.0+m_attr->m_speed/3.0)
       > 7.0  )
    {
         event.timeStamp += dist/m_attr->m_speed;
         while(  context->removeEvent(pe_EVC_NEXTNODE,getObjectID())==1  ) {}
         issueEvent( event );
         return;
    }

    IRouteObject *ri = (IRouteObject*)(context->queryInterface(m_routeID,IRouteObjectIID));
    if(  ri == 0  )
    {
         echo("People::nexNodeDefault: route interface lost");
         return;
    }
    if(  ri->GetNodeCnt() < 2  )
    {
         echo("People::nexNodeDefault: route has less than 2 nodes");
         return;
    }

    const int nodeCount = ri->GetNodeCnt();
    m_previousRouteNode = m_curNode;
    int nextRouteNode = m_curNode + 1;
    if(  nextRouteNode >= nodeCount  )
    {
         if(  m_startBackSpaceNode > 0  )
              nextRouteNode -= m_startBackSpaceNode + 1;
         else
         if(  m_startBackSpaceNode == 0  )
         {
              m_dir = CFVector3(0,0,0);
              return;
         }
         else nextRouteNode = 0;
    }
    if(  nextRouteNode < 0  )
         nextRouteNode = 0;
    else
    if(  nextRouteNode >= nodeCount  )
         nextRouteNode = nodeCount-1;

    m_curNode = nextRouteNode;
    CFVector3 nextNode( ri->GetNode(m_curNode) );
    m_nextNode = nextNode;
    m_stateNextNode[0] = nextNode;


    switch( m_attr->m_onLand )
    {
    case NO_LAND:   dist = Abs( nextNode-pos ); break;
    case ON_LAND:
    case ON_WATER:
    case ON_OBJ:    dist = hypot(nextNode.z-pos.z, nextNode.x-pos.x ); break;
    default: echo("People::Warning: unknown m_onLand");
    }

    event.timeStamp += dist/m_attr->m_speed;
    while(  context->removeEvent(pe_EVC_NEXTNODE,getObjectID())==1  ) {}
    issueEvent( event );
}


//==============================================================

void RotateVertical(
                     CFVector3       &v,
                     double           angle,
                     const CFVector3 &mpos,
                     CFVector3       &nextNode
                   )
{
    double hang  = atan2( v.x, v.z  );
    double len   = Abs( v );
    double cos_a = cos(angle)*len,
           sin_a = sin(angle)*len;
    v = CFVector3( cos_a*sin(hang), sin_a, cos_a*cos(hang) );
    nextNode = mpos + v;
}


//==============================================================
void People::manoeuvre(KR_Event &event,
                    const CFVector3 &/* pos */,
                    const CFVector3 &mpos
                   )
{
     double           deltaT  = 0.8+context->rnd_f(2.8);
     double           chkT    = 0.7;
     double           clzTime = 10;
     KR_ObjectID      oID;
     CFVector3        movedir = m_nextNode-mpos;

     if(  checkCollision(
                                mpos,    // эрўрыю фтшцхэш
                                Normal(movedir)*m_attr->m_speed,            // эряЁтыхэшх ёю ёъюЁюёЄ№■
                                20.0,      // Ёрфшєё
                                deltaT+chkT,     // тЁхь  фы  яЁютхЁъш
                                getObjectID(),     // ъюую шуэюЁшЁютрЄ№
                                clzTime, // тЁхь , ўхЁхч ъюЄюЁюх ёЄєъэхьё
                                oID      // юс·хъЄ, ю ъюЄюЁ√щ ёЄєъэхьё
                              ) )
     {
         if(  clzTime < deltaT  )
         {
              //
              // too near
              //
              if(  clzTime < chkT  )
              {
                   if(  m_nextNode.y > 150 && (!oID.isNUL()) )
                        RotateVertical(movedir,-30*M_PI/180, mpos, m_nextNode); //m_nextNode.y -= 20;
                   else RotateVertical(movedir,+30*M_PI/180, mpos, m_nextNode); //m_nextNode.y += 20;
                   deltaT = clzTime-chkT/2;
                   if(  deltaT<0.05  ) deltaT = 0.05;
              }
              else
              {
                   deltaT = clzTime-(chkT-0.05);
                   if(  deltaT<0.05  ) deltaT = 0.05;
                   if(  m_nextNode.y > 170 && (!oID.isNUL())  )
                        RotateVertical(movedir,-20*M_PI/180, mpos, m_nextNode);//m_nextNode.y -= 10;
                   else RotateVertical(movedir,+20*M_PI/180, mpos, m_nextNode);//m_nextNode.y += 10;
              }
         }
         else RotateVertical(movedir,+20*M_PI/180, mpos, m_nextNode);//m_nextNode.y += 10;
     }

     event.timeStamp += deltaT;
}


//==============================================================
void People::nexNodeAttackFar(KR_Event &event,
                              const CFVector3 &pos,
                              const CFVector3 &mpos,
                              IDynamicObject *dobj
                             )
{
    //echo("far");
    if(  m_attr->m_stopIfAttack )
         attackAllways( event, pos );
    else
    {
         PeopleRouteMotion_FarAttackTarget(
              pos, dobj->getMoveDir()*dobj->getMoveSpeed(), m_nextNode,
              m_attr->m_maxOutDist, &m_nextNode );

         manoeuvre(  event, pos, mpos );
    }
}


//==============================================================
void People::nexNodeAttackNear(KR_Event &event,
                              const CFVector3 &pos,
                              const CFVector3 &mpos,
                              IDynamicObject *dobj
                             )
{
    //echo("near");
    if(  m_attr->m_stopIfAttack  )
         attackAllways( event, pos );
    else
    {
         double range = 95;
         CFVector3 candidate = pos-dobj->getMoveDir()*20
                                   +CFVector3(context->rnd_f(-range,range),
                                    context->rnd_f(-10,5+25*(mpos.y<180)),
                                    context->rnd_f(-range,range));
         PeopleRouteMotion_NearAttackTarget(
              mpos, candidate, m_attr->m_maxOutDist, &m_nextNode );


         manoeuvre(  event, pos, mpos );
    }
}



void People::setPeopleAttr()
{
	m_attr = 0;
	m_askin = 0;

	ct_Attribute *attr = __attrTable.searchAttribute(m_peopleAttrID);
	if( attr==NULL )
	{
		const char *attrName = context->searchObject(m_peopleAttrID);
		if(  attrName == 0  )
			attrName = "<unknown>";
		echo( "People::receiveEvent: Unknown attribute %s", attrName);
		return;
	}
	else
		m_attr = (AttributePeople*)attr;


	updateSound( getObjectID(),
		context,
		m_attr->m_ctsndID,
		m_attr->m_wav,
		m_snd );



	m_skin.Attach(m_attr->m_cacheSkin);
	if(  m_skin.Model() == 0  )
	{
		echo("People::setPeopleAttr: skin model not loaded");
		m_attr = 0;
		return;
	}
	m_viewDynObj.BumpDef().fRadius = m_skin.Model()->Radius();
	m_askin = (ISkin*)(context->queryInterface(m_attr->m_skinID, ISkinIID));
	if(  m_askin == 0  )
		echo("People::setPeopleAttr: ISkin interface unavailable");

	m_skin.SetUserAttrib(this);
	if(  strcmp(m_attr->m_skinName,"sk.Peop.shiz_f0a")==0 )
		m_skin.SetAnimationCallback(AnimateCallBack0);
	else
//	if(  strcmp(m_attr->m_skinName,"sk.Taxi.cln_f01")==0 )
//		m_skin.SetAnimationCallback(AnimateCallBack1);
//	else
	if(  strcmp(m_attr->m_skinName,"sk.Taxi.cln_f08")==0 )
		m_skin.SetAnimationCallback(AnimateCallBack2);
}

bool People::restoreStableReferences(const KR_ObjectID &attribute,
                                     const KR_ObjectID &route)
{
    if (context == NULL || __attrTable.searchAttribute(attribute) == NULL)
        return false;

    IRouteObject *newRoute = static_cast<IRouteObject *>(
        context->queryInterface(route, IRouteObjectIID));
    if (newRoute == NULL || newRoute->GetNodeCnt() < 2)
        return false;

    // Take the new reference before releasing the old one.  The two IDs may
    // name the same Route and Route::DelRef destroys it at a zero count.
    newRoute->AddRef();
    if (!m_routeID.isNUL())
    {
        IRouteObject *oldRoute = static_cast<IRouteObject *>(
            context->queryInterface(m_routeID, IRouteObjectIID));
        if (oldRoute != NULL)
            oldRoute->DelRef();
    }
    if (!m_snd.isNUL() && context->isExist(m_snd))
        context->removeObject(m_snd);

    m_snd = KR_ObjectID::NUL();
    m_peopleAttrID = attribute;
    m_routeID = route;
    setPeopleAttr();
    if (!stableReferencesReady())
    {
        if (!m_snd.isNUL() && context->isExist(m_snd))
            context->removeObject(m_snd);
        m_snd = KR_ObjectID::NUL();
        m_attr = NULL;
        m_askin = NULL;
        m_peopleAttrID = KR_ObjectID::NUL();
        m_routeID = KR_ObjectID::NUL();
        newRoute->DelRef();
        return false;
    }
    return true;
}

bool People::stableReferencesReady() const
{
    KR_ObjectID attribute = m_peopleAttrID;
    KR_ObjectID routeID = m_routeID;
    if (context == NULL || m_attr == NULL || m_skin.Model() == NULL ||
        m_askin == NULL || attribute.isNUL() || routeID.isNUL() ||
        __attrTable.searchAttribute(m_peopleAttrID) == NULL)
        return false;
    IRouteObject *route = static_cast<IRouteObject *>(
        context->queryInterface(m_routeID, IRouteObjectIID));
    return route != NULL && route->GetNodeCnt() >= 2;
}

int PeopleFreeObjectCount()
{
    return __classTable.freeObjectCount();
}

bool PeopleAttributeExists(const KR_ObjectID &attribute)
{
    return __attrTable.searchAttribute(attribute) != NULL;
}

//==========================================================================
void People::crashExpl( CFVector3 pos, double time )
{
    createExplosion(
	             pos,
	             time,
	             getObjectID(),
	             m_attr->m_cacheExplosionTable,
	             m_attr->m_cacheExplAttr
	           );
}

//==========================================================================
void People::crashCreateCorpse( CFVector3 pos, double time )
{
    // мы на земле, создать труп
    // если мы на земле а не на динамическом объекте
    createCorpse( pos,
                  time,
     	          getObjectID(),
                  m_attr->m_cacheCorpseTable,
                  m_attr->m_cacheCorpseAttr
                );
}

//==========================================================================
void People::moveKilled_FALLOW_checkClz( KR_Event &event )
{
    double clzTime;
    KR_ObjectID oID;

    if(  !m_deleted  )
    if(  checkCollision(
                    getPosition(),    // начало движения
                    m_dir,            // напрвление со скоростью
                    getRadius(),      // радиус
                    m_calcPosInc,     // время для проверки
                    getObjectID(),    // кого игнорировать
                    clzTime,          // время, через которое стукнемся
                    oID      // объект, о который стукнемся
                ) )
    {
         CFVector3 crashPos(getPosition()+m_dir*clzTime);

         crashExpl( crashPos, m_prevTime+clzTime );

         if(  m_attr->m_cacheCorpseAttr != -1  &&  oID.isNUL()  )
                  // мы на земле, создать труп
                  // если мы на земле а не на динамическом объекте
                  crashCreateCorpse( crashPos, m_prevTime+clzTime );

         if(  !m_deleted  )
              g_arena.delObject(getObjectID(), m_prevTime+clzTime+0.1 );
         m_deleted = 1;
    }
    else
    {
         event.timeStamp += m_calcPosInc;
         issueEvent( event );
    }
}

//==========================================================================
void People::moveKilled_FALLOW_1( KR_Event &event, double deltaT )
{
    m_dir = m_dir*(1-deltaT)+CFVector3(0,-34,0)*deltaT;
    ct_Subject::setPosition(getPosition()+m_dir*deltaT);

    m_kill_deltaVAngle += 0.3*deltaT;
    if(  m_kill_deltaVAngle > 1  )
         m_kill_deltaVAngle = 1;

    m_rotateOx = m_kill_deltaVAngle*sin(m_kill_startPhase-(m_prevTime-m_kill_time)*4);
    moveKilled_FALLOW_checkClz( event );
}

//==========================================================================
void People::moveKilled_FALLOW_2( KR_Event &event, double deltaT )
{
    m_dir += CFVector3(0,-2,0)*deltaT;
    ct_Subject::setPosition(getPosition()+m_dir*deltaT);

    m_kill_deltaVAngle += 0.3*deltaT;

    m_rotateOz = m_kill_deltaVAngle;

    createSmoke(
                 getPosition(),
                 getObjectID(),
                 m_prevTime,
                 m_attr->m_smokeTableID,
                 m_attr->m_smokeAttrID
               );

     moveKilled_FALLOW_checkClz( event );
}

//==========================================================================
void People::moveKilled_FALLOW_SINK( KR_Event &event, double deltaT )
{
    m_dir = m_dir*(1-deltaT)+CFVector3(0,-1,0)*deltaT;
    ct_Subject::setPosition(getPosition()+m_dir*deltaT);

    m_kill_deltaVAngle += 0.15*deltaT;
    if(  m_kill_deltaVAngle > 0.7  )
         m_kill_deltaVAngle = 0.7;

    m_rotateOx = m_kill_deltaVAngle*sin(m_kill_startPhase-(m_prevTime-m_kill_time)*0.5);
    double clzTime;
    KR_ObjectID oID;

    if( checkCollision(
		          getPosition(),    // начало движения
		          m_dir,            // напрвление со скоростью
		          getRadius(),      // радиус
		          m_calcPosInc,     // время для проверки
		          getObjectID(),     // кого игнорировать
		          clzTime, // время, через которое стукнемся
		          oID      // объект, о который стукнемся
		          ))
         context->removeObject( getObjectID() );
    else
    {
         event.timeStamp += m_calcPosInc;
         issueEvent( event );
    }
}

 //============================================================
void People::moveKilled( KR_Event &event, double deltaT )
{
    switch( m_killed )
    {
    case KILL_FALLOW_1: // сбитый дирижабль
          moveKilled_FALLOW_1( event, deltaT );
          return;

    case KILL_FALLOW_2: // сбитый самолет
          moveKilled_FALLOW_2( event, deltaT );
          return;

    case KILL_SINK: // тонет кораблик
          moveKilled_FALLOW_SINK( event, deltaT );
          return;

    case KILL_NONE: 
          context->removeObject( getObjectID() );
          return;
    }
}


 //============================================================
int People::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case pe_EVC_MOVE:
     {
        double deltaT = event.timeStamp-m_prevTime;

        m_lastMovePos       = getPosition();
        m_lastMoveTimeStamp = event.timeStamp;
        m_lastMoveDeltaT    = deltaT;

        m_prevTime = event.timeStamp;

        if(  m_killed!=KILL_NONE   )
        {
             moveKilled( event, deltaT );
             return 1;
        }

        { //============== MOVING ===================================

             CFVector3 nextNode( m_nextNode );

             switch( m_attr->m_onLand )
             {
             case ON_LAND:
              {
                  CFVector3 pos(getPosition());
                  double step = deltaT;
                  if(  m_stoped  && (getState() == pe_STATE_ATTACK)  )
                       step = 0;
                  else
                  if(  m_attr->m_stopIfAttack
                    && Normal(CFVector3(m_dir.x,0,m_dir.z))*Normal(CFVector3(nextNode.x-pos.x,
                                                      0,
                                                      nextNode.z-pos.z)) < m_attr->m_deltaZeroSpeed  )
                       step = 0;

                  setMovingPosition(getPosition()+m_dir*step, deltaT);
                  nextNode = m_nextNode;
                  pos = getPosition();
                  double hAngle = atan2( nextNode.z-pos.z, nextNode.x-pos.x );
                  if( deltaT < 0.5 )
                  {
                      setHAngleFixDir(
                                interpolateAngle( m_hAngle, hAngle,
                                                 m_attr->m_rollSpeed,
                                                 deltaT ));

                  }
                  m_rotateOx = 0;

                  double height;
                  CFVector3 normal;
                  CViewScene::Current()->GetTerrain()->GetPlane(pos,normal,height);
                  pos.y = height;
                  setPosition(pos);
              }
              break;

             case ON_WATER:
              {
                  double step = deltaT;
                  CFVector3 pos(getPosition());

                  if(  m_stoped  && (getState() == pe_STATE_ATTACK)  )
                       step = 0;
                  else
                  if(  m_attr->m_stopIfAttack 
                    && Normal(m_dir)*Normal(nextNode-pos) < m_attr->m_deltaZeroSpeed  )
                       step = 0;

                  setMovingPosition(getPosition()+m_dir*step, deltaT);
                  nextNode = m_nextNode;
                  pos = getPosition();
                  
                  //m_pos += m_dir*deltaT;

                  double hAngle = atan2( nextNode.z-pos.z, nextNode.x-pos.x );
                  if( deltaT < 0.5 )
                      setHAngle(
                                  interpolateAngle( m_hAngle, hAngle,
                                                 m_attr->m_rollSpeed,
                                                 deltaT ));
                  
                  m_dir.x = cos(m_hAngle)*m_attr->m_speed;
                  m_dir.z = sin(m_hAngle)*m_attr->m_speed;
                  m_rotateOx = 0;
            
                  pos.y = ZAV_Scene()->GetTerrain()->Waterline();
                  double height;
                  CFVector3 normal;
                  CViewScene::Current()->GetTerrain()->GetPlane(pos,normal,height);
                  if(  pos.y-height >m_attr->m_maxDeep  )
                       setPosition(pos);
              }
              break;

             case ON_OBJ:
              {
                   double step = deltaT;
                   const CFVector3 start(getPosition());

                   if(  m_stoped  &&  (getState() == pe_STATE_ATTACK)  )
                        step = 0;
                   else
                   if(  m_attr->m_stopIfAttack
                     && !(PeopleRouteMotion_AllowsHorizontalStep(m_dir,start,nextNode,m_attr->m_deltaZeroSpeed))  )
                        step = 0;

                   SPeopleObstacleRecoveryRequest recoveryRequest = {};
                   recoveryRequest.objectRadius = getRadius();
                   recoveryRequest.maximumRadius = m_attr->m_maxRadius;
                   recoveryRequest.recoveryTime = m_obstacleRecoveryTime;
                   recoveryRequest.deltaTime = deltaT;
                   recoveryRequest.collisionTime = step;
                   recoveryRequest.visible = m_isVisible;
                   recoveryRequest.collisionDetected = 0;
                   recoveryRequest.contactCode = m_isClz;
                   recoveryRequest.detectedContactCode =
                       m_isClz != 0 ? m_isClz : 1;
                   recoveryRequest.stateDepth = m_stateSP;

                   SPeopleObstacleRecoveryResult preview = {};
                   int collisionDetected = 0;
                   double collisionTime = step;
                   if(  PeopleObstacleRecovery_Advance(recoveryRequest,&preview)
                     && preview.sweepEnabled && step>0  )
                   {
                        collisionDetected = checkStaticCollision(
                            start, m_dir, preview.sweepRadius, step,
                            collisionTime );
                   }

                   recoveryRequest.collisionDetected = collisionDetected;
                   recoveryRequest.collisionTime = collisionTime;
                   SPeopleObstacleRecoveryResult recovery = {};
                   if(  !PeopleObstacleRecovery_Advance(recoveryRequest,&recovery)  )
                   {
                        recovery.recoveryTime = 0;
                        recovery.travelTime = step;
                        recovery.popState = 0;
                        recovery.contactCode = 0;
                   }
                   m_obstacleRecoveryTime = recovery.recoveryTime;
                   m_isClz = recovery.contactCode;
                   if(  recovery.popState  )
                        popState();

                   if(  collisionDetected  )
                   {
                        const CFVector3 contact = start+m_dir*recovery.travelTime;
                        setRoutePosition(contact,
                            m_attr->m_speed*recovery.travelTime,deltaT,1);
                   }
                   else setMovingPosition(start+m_dir*step,deltaT);

                   nextNode = m_nextNode;
                   CFVector3 pos(getPosition());
                   double hAngle = atan2( nextNode.z-pos.z, nextNode.x-pos.x );
                   if( deltaT < 0.5 && !m_isClz )
                       setHAngleFixDir(
                                   interpolateAngle( m_hAngle, hAngle,
                                                  m_attr->m_rollSpeed,
                                                  deltaT ));

                   // May's front/rear support manifold. The geometry, sweep
                   // radius and vertical travel are recovered from the retail
                   // ON_OBJECTS branch; only the scene queries remain here.
                   {
                    SPeopleSupportSamplingRequest samplingRequest = {};
                    samplingRequest.objectRadius = getRadius();
                    samplingRequest.heading = m_hAngle;
                    samplingRequest.positionX = pos.x;
                    samplingRequest.positionY = pos.y;
                    samplingRequest.positionZ = pos.z;
                    SPeopleSupportSamplingResult sampling = {};
                    int contactCode = m_isClz;

                    if(  m_isVisible
                      && PeopleSupportSampling_Build(
                             samplingRequest,&sampling)  )
                    {
                         const CFVector3 sweepVelocity(
                             sampling.sweepVelocityX,
                             sampling.sweepVelocityY,
                             sampling.sweepVelocityZ);
                         const CFVector3 front(
                             sampling.frontX,
                             sampling.frontY,
                             sampling.frontZ);
                         const CFVector3 rear(
                             sampling.rearX,
                             sampling.rearY,
                             sampling.rearZ);
                         double frontTime = sampling.sweepTime;
                         double rearTime = sampling.sweepTime;
                         const int frontHit = checkStaticCollision(
                             front,sweepVelocity,sampling.sweepRadius,
                             sampling.sweepTime,frontTime);
                         const int rearHit = checkStaticCollision(
                             rear,sweepVelocity,sampling.sweepRadius,
                             sampling.sweepTime,rearTime);

                         KR_ObjectID rearID = KR_ObjectID::NUL();
                         if(  rearHit  )
                         {
                              double dynamicTime = sampling.sweepTime;
                              checkCollision(
                                  rear,sweepVelocity,sampling.sweepRadius,
                                  sampling.sweepTime,getObjectID(),
                                  dynamicTime,rearID);
                         }

                         if(  contactCode==0  )
                         {
                              // May's unsupported-front recovery enters the
                              // full-speed counter-clockwise class.
                              if(  !frontHit  ) contactCode = 2;
                              else
                              if(  !rearHit  ) contactCode = 3;
                              else
                              if(  !rearID.isNUL()  )
                              {
                                   IDynamicObject *support =
                                       (IDynamicObject*)context->queryInterface(
                                           rearID,IDynamicObjectIID);
                                   if(  support!=0  )
                                   {
                                        const CFVector3 supportPos(
                                            support->getPos());
                                        PeopleSupportSampling_ClassifyDynamicSide(
                                            pos.x,pos.z,
                                            supportPos.x,supportPos.z,
                                            m_hAngle,9,11,&contactCode);
                                   }
                              }
                         }

                         if(  frontHit && rearHit  )
                         {
                              const double frontHeight =
                                  front.y+sweepVelocity.y*frontTime-
                                  sampling.sweepRadius;
                              const double rearHeight =
                                  rear.y+sweepVelocity.y*rearTime-
                                  sampling.sweepRadius;
                              m_rotateOx = atan2(
                                  rearHeight-frontHeight,
                                  2*sampling.sampleOffset);
                              pos.y = (frontHeight+rearHeight)*0.5;
                              if(  rearID.isNUL()  ) setPosition(pos);
                         }
                    }

                    m_isClz = contactCode;
                    SPeopleContactResponseRequest contactRequest = {};
                    contactRequest.contactCode = contactCode;
                    contactRequest.heading = m_hAngle;
                    contactRequest.contactHeading = hAngle;
                    contactRequest.rollSpeed = m_attr->m_rollSpeed;
                    contactRequest.deltaTime = deltaT;
                    SPeopleContactResponseResult contactResponse = {};
                    if(  PeopleContactResponse_Advance(
                             contactRequest,&contactResponse)
                      && contactResponse.active  )
                         setHAngleFixDir(contactResponse.heading);
                   }
              }
               break;
             case NO_LAND:
              {
                  double step = deltaT;
                  CFVector3 pos(getPosition());

                  if(  m_stoped && (getState() == pe_STATE_ATTACK)  )
                       step = 0;
                  else
                  if(  m_attr->m_stopIfAttack 
                    && Normal(m_dir)*Normal(nextNode-pos) < m_attr->m_deltaZeroSpeed  )
                       step = 0;
                  setMovingPosition(getPosition()+m_dir*step, deltaT);
                  nextNode = m_nextNode;
                  pos = getPosition();

                  double dist2D = hypot( nextNode.x-pos.x, nextNode.z-pos.z );
                  double hAngle = atan2( nextNode.z-pos.z, nextNode.x-pos.x );
                  double vAngle = atan2( nextNode.y-pos.y, dist2D );

                  if( deltaT < 0.5 )
                  {
                      setHAngle( interpolateAngle( m_hAngle, hAngle,
                                                 m_attr->m_rollSpeed,
                                                 deltaT ));
                  
                      vAngle = interpolateAngle( m_rotateOx,vAngle, m_attr->m_rollSpeed,
                                                 deltaT);
                  }

                  m_rotateOx = vAngle;

                  double co = cos(m_rotateOx)*m_attr->m_speed;
                  m_dir.x = cos(m_hAngle)*co;
                  m_dir.z = sin(m_hAngle)*co;
                  m_dir.y = sin(m_rotateOx)*m_attr->m_speed;
              }
             }

             if(  getState()==pe_STATE_ATTACK  && 
                  event.timeStamp-m_prevShootTime > m_attr->m_cannonSpeed &&
                  m_attr->m_bulletIndex != -1
               )
                 onShoot(event.timeStamp);
        }
        
        const double haze = CViewFigure::HazeMax();
        const double cadenceScale = g_vehicle == 0
             ? 2.0
             : rr2nw::ActorCadenceScale(
                   Abs(g_vehicle->getPos()-getPosition()), haze);
        event.timeStamp += m_calcPosInc*cadenceScale;
        issueEvent( event );

        // Sound
         if(  !m_snd.isNUL() && m_audibleThisFrame)
         {
          CFVector3 pos(getPosition());
          event.label = snd_EV_MOVE_TO;
          event.destination = m_snd;
          event.source      = getObjectID();
          event.data.open(EDO_WRITE)
                     .putDouble(pos.x)
                     .putDouble(pos.y)
                     .putDouble(pos.z)
                   .close();
          context->sendEventNow( event );
         }

     }
         break;


    case pe_EVC_GROUNDED_NEXTNODE:
     {
         // May retail event 26012 is a fast grounded target/visibility
         // cadence. Route-segment progress remains owned by NEXTNODE until
         // the May movement helper below this frontier is recovered.
         event.timeStamp += 0.3;
         if(  m_stateSP > 0 && m_isVisible  )
         {
              KR_ObjectID enemyID = getEnemyID();
              IDynamicObject *dobj = enemyID.isNUL() ? 0 :
                   (IDynamicObject*)(context->queryInterface(
                        enemyID,IDynamicObjectIID));
              if(  dobj != 0  )
              {
                   m_nextNode = dobj->getPos();
                   event.timeStamp -= 0.1;
              }
              else popState();
         }
         else
         {
              if(  m_stateSP > 0  )
                   popState();
              if(  !m_isVisible &&
                   (m_attr == 0 || !m_attr->m_allwaysVisible)  )
                   event.timeStamp += 5.0;
         }
         issueEvent(event);
     }
     break;

    case pe_EVC_NEXTNODE:
     {
         for(;;)
         {
         switch( getState() )
         {
         case pe_STATE_DEFAULT: nexNodeDefault(event); break;
         case pe_STATE_ATTACK:
          {
            IDynamicObject *dobj = (IDynamicObject*)(context->queryInterface(getEnemyID(),IDynamicObjectIID));
            if(  dobj  )
            {
                 CFVector3 pos(dobj->getPos());
                 CFVector3 mpos(getPosition());

                 if(  Abs2(pos-mpos) > 35*35 )
                      nexNodeAttackFar (event, pos, mpos, dobj );
                 else nexNodeAttackNear(event, pos, mpos, dobj );

                 while(  context->removeEvent(pe_EVC_NEXTNODE,getObjectID())==1  ) {}
                 issueEvent( event );
            }
            else
            {
                 popState();
                 continue;
            }
          }
          break;
         }
         break;
         } // for(;;)
     }
     m_stoped = m_attr->m_stopIfAttack && (getState()==pe_STATE_ATTACK);
     break;
    case pe_EVC_FIND_ENEMY:
         {
             event.timeStamp += 2.0+(!m_isVisible)*5;
             issueEvent(event);
             
             if(  getState() == pe_STATE_DEFAULT  )
             {
                  ct_SubjectFindData fsd;
                  KR_ObjectID        eID = KR_ObjectID::NUL();
                  double             K   = -1e3;
                  double xMin,xMax, zMin, zMax;
                  CFVector3 pos(realPosition());
                  xMin = xMax = pos.x;
                  zMin = zMax = pos.z;
                  xMin -= m_attr->m_viewDist; xMax += m_attr->m_viewDist;
                  zMin -= m_attr->m_viewDist; zMax += m_attr->m_viewDist;

                  ct_Arena::findFirstSubject( fsd, xMin, zMin, xMax, zMax );

                  for( int i = 0; i < fsd.getCount(); ++i )
                  {
                       const KR_ObjectID  &enemyID = fsd[i];
                       IUnit *u =(IUnit *)(context->queryInterface( enemyID, IUnitIID ));

                       if(  u==0  )
                            continue;


                       if(  !u->isFriend(m_commanderID)  )
                       {
                             double     power = u->getPower();
                             IDynamicObject *dobj = (IDynamicObject *)
                             (context->queryInterface( enemyID, IDynamicObjectIID ));

                            if(  dobj != 0  )
                            {

                                 CFVector3 pos(dobj->getPos());
                                 double curK = power/(1+Abs(CFVector2(m_position.x,m_position.z)-CFVector2(pos.x,pos.z)));
                                 if(  curK > K  )
                                 {
                                      K   = curK;
                                      eID = enemyID;
                                 }
                           }
                       }
                  }

                  if(  !eID.isNUL()  )
                  {
                       pushState(pe_STATE_ATTACK,eID);
                  }
                  else popState();
             }


         }
         break;

    case pe_EV_STARTSHOW:
         m_isNotCreate = 0;
         break;

case pe_EVCMD_START:
case pe_EVCMD_START_EX:
 {
     //KR_ObjectID attrID;
     char        route[50];
     double      timeStart = 0;
     int         startNode = 0;
     int         backSpaceNode = -1;
     double      startMoveTime = 0;
     const int   extendedStart = event.label == pe_EVCMD_START_EX;

     m_lastMoveTimeStamp = event.timeStamp;
     m_lastMoveDeltaT    = 0;

     event.data.open(EDO_READ)
                 .getObjectID(m_peopleAttrID)
                 .getStr     (route,sizeof(route)-1)
                 .getDouble  (timeStart);
     // January scripts ended after timeStart.  May retail added startNode to
     // the normal payload and two more fields to pe_EVCMD_START_EX.  Accept
     // both layouts so archived January data remains usable.
     if(  event.data.remaining() > 0  )
          event.data.getInt(startNode);
     if(  extendedStart && event.data.remaining() > 0  )
          event.data.getInt(backSpaceNode);
     if(  extendedStart && event.data.remaining() > 0  )
          event.data.getDouble(startMoveTime);
     event.data.close();

     m_startBackSpaceNode = backSpaceNode;
     m_startMoveDelay = startMoveTime;
     double  viewTime = event.timeStamp+timeStart+context->rnd_f(0.1);

	 setPeopleAttr();
     if(  m_attr == 0  )
     {
          echo("People::receiveEvent: start skipped because attribute is invalid");
          break;
     }

     /*ct_Attribute *attr = __attrTable.searchAttribute(attrID);
     if( attr==NULL )
          echo( "People::receiveEvent: Unknown attribute %s",
                context->searchObject(attrID));
     else m_attr = (AttributePeople*)attr;*/


		m_damage = m_attr->m_initialDamage;


	/*updateSound( getObjectID(),
		     context,
		     m_attr->m_ctsndID,
		     m_attr->m_wav,
		     m_snd );



    m_skin.Attach(m_attr->m_cacheSkin);
    m_viewDynObj.BumpDef().fRadius = m_skin.Model()->Radius();
    m_askin = (ISkin*)(context->queryInterface(m_attr->m_skinID, ISkinIID));
    
    m_skin.SetUserAttrib(this);
    if(  strcmp(m_attr->m_skinName,"sk.Peop.shiz_f0a")==0 )
         m_skin.SetAnimationCallback(AnimateCallBack0); 
    else
    if(  strcmp(m_attr->m_skinName,"sk.Taxi.cln_f01")==0 )
         m_skin.SetAnimationCallback(AnimateCallBack1); 
    else
    if(  strcmp(m_attr->m_skinName,"sk.Taxi.cln_f08")==0 )
         m_skin.SetAnimationCallback(AnimateCallBack2); */

    if( m_askin != 0 && m_askin->isAutoAnim()  )
        m_askin->skinSetAnimAuto(&m_skin);

    if(  route[0] == 0  )
         m_routeID = context->searchObject( m_attr->m_routeName );
    else m_routeID = context->searchObject( route );
    m_prevTime = viewTime;

    m_calcPosInc = m_attr->m_timeInc;

    IRouteObject *ri = (IRouteObject*)
          (context->queryInterface(m_routeID,IRouteObjectIID));
    if(  ri == 0  )
    {
         echo("People::receiveEvent: route not found for people");
         m_routeID = KR_ObjectID::NUL();
         break;
    }
    if(  ri->GetNodeCnt() < 2  )
    {
         echo("People::receiveEvent: route has less than 2 nodes");
         m_routeID = KR_ObjectID::NUL();
         break;
    }

    ri->AddRef();

    {
         int nodeCount = ri->GetNodeCnt();
         if(  startNode < 0  )
              startNode = 0;
         else
         if(  startNode >= nodeCount  )
              startNode = nodeCount-1;

         m_previousRouteNode = startNode;
         m_curNode = (startNode+1)%nodeCount;
         setPosition( ri->GetNode(startNode) );
         CFVector3 nextNode(ri->GetNode(m_curNode));

         CFVector3 pos(getPosition());
         setHAngleFixDir( atan2( nextNode.z-pos.z, nextNode.x-pos.x ) );
         m_nextNode = nextNode;
         m_stateNextNode[0] = nextNode;
         // The May executable exposes the subject at viewTime but keeps it
         // stationary until pe_EV_STARTMOVE.  This is how mission scripts
         // stagger columns and aircraft without changing their spawn time.
         m_dir = CFVector3(0,0,0);
    }
    event.label       = pe_EV_STARTMOVE;
    event.source      = getObjectID();
    event.destination = getObjectID();
    event.timeStamp   = viewTime + m_startMoveDelay;
    issueEvent( event );
    event.label       = pe_EV_STARTSHOW;
    event.timeStamp   = viewTime;
    issueEvent( event );
 }
 m_lastMovePos = getPosition();
 break;

    case pe_EV_STARTMOVE:
     {
         IRouteObject *ri = (IRouteObject*)
              (context->queryInterface(m_routeID,IRouteObjectIID));
         if(  ri == 0 || m_attr == 0 || ri->GetNodeCnt() < 2  )
              break;

         CFVector3 pos(getPosition());
         CFVector3 nextNode(m_nextNode);
         setHAngleFixDir( atan2(nextNode.z-pos.z,nextNode.x-pos.x) );
         m_dir.y = 0;
         m_prevTime = event.timeStamp;
         m_lastMoveTimeStamp = event.timeStamp;
         m_lastMoveDeltaT = 0;

         if(  m_attr->m_speed > 1e-3  )
         {
              double dist = Abs(nextNode-pos);
              event.label = pe_EVC_NEXTNODE;
              event.timeStamp += dist/m_attr->m_speed;
              while(  context->removeEvent(pe_EVC_NEXTNODE,getObjectID())==1  ) {}
              issueEvent(event);

              if(  m_attr->m_onLand != NO_LAND && m_attr->m_onLand != 4  )
              {
                   event.label = pe_EVC_GROUNDED_NEXTNODE;
                   event.timeStamp = m_prevTime;
                   issueEvent(event);
              }
         }

         event.label = pe_EVC_MOVE;
         event.source = getObjectID();
         event.destination = getObjectID();
         event.timeStamp = m_prevTime;
         issueEvent(event);
     }
     break;

    case pe_EVCMD_STOP:
            context->removeEvent(pe_EVC_MOVE,getObjectID());
            break;

    case KR_WAKE_UP:
            break;

    case KR_SET_ATTR:
            s_ASSERTNQ("People:receiveEvent:case KR_SET_ATTR: do't supported");
            break;

case pe_EV_SETAUTOANIM:
        if(  m_askin != 0  )
            m_askin->skinSetAnimAuto(&m_skin);
        break;

    case t_EV_ONCOLLISION:
            break;

    default: return 0;
    }
    return 1;
 }

 //============================================================
void People::addNotify()
 {
    ct_Subject::addNotify();
    m_attr = 0;
    m_askin = NULL;
    m_damage = 1;
    m_killed = KILL_NONE;
    m_rotateOz = 0;
    m_deleted  = 0;
    m_nextNode = CFVector3(0,0,0);
    initState();
    m_prevShootTime  = 0;
    m_lastDamageTime = 0;
    m_correctScale   = 1;
    m_peopleAttrID   = KR_ObjectID::NUL();
    m_routeID        = KR_ObjectID::NUL();
    m_commanderID    = KR_ObjectID::NUL();
    m_snd            = KR_ObjectID::NUL();
    m_isNotCreate    = 1;
    m_prevStartShoot = 0;
    m_stoped         = 0;
    m_isClz          = 0;
    m_startBackSpaceNode = -1;
    m_startMoveDelay = 0;
    m_previousRouteNode = 0;
    m_routeDeviationTime = 0;
    m_obstacleRecoveryTime = 0;
    m_rotateOx = m_rotateOy = m_rotateOz = 0;
}

 //============================================================
void People::removeNotify()
 {    
    IRouteObject *ri = 0;

    if(  !m_routeID.isNUL()  )
         ri = (IRouteObject*)
              (context->queryInterface(m_routeID,IRouteObjectIID));

    if(  ri!=0  )
         ri->DelRef();

    m_routeID = KR_ObjectID::NUL();

    if(  !m_snd.isNUL()  )
         context->removeObject( m_snd );

    m_snd = KR_ObjectID::NUL();

    ct_Subject::removeNotify();
 }

 //============================================================
void People::draw()
{
}

 //============================================================


void People::onExitAudibleZone(double ts) 
{

        if(  !m_snd.isNUL()  )
        {  
          KR_Event event;
          event.destination = m_snd;
          event.source      = getObjectID();
          event.timeStamp   = ts;
          event.label       = snd_EV_END;
          context->sendEventNow( event );
         }
}

void People::onEnterAudibleZone(double ts) 
{

        // Sound
         if(  !m_snd.isNUL() )
         {
	  KR_Event event;

          event.destination = m_snd;
          event.source      = getObjectID();
	  event.timeStamp   = ts;
          event.label       = snd_EV_START;
          event.data.open(EDO_WRITE)
                 .putInt(0)
              .close();
          context->sendEventNow( event );
         } 
} 
  
 //============================================================
void People::onView(double)
{
    // The hidden scheduler remains authoritative.  Discard only its stale
    // presentation sample so the first near frame starts at the exact current
    // position rather than projecting an old far-cadence displacement.
    m_lastMovePos = getPosition();
    m_lastMoveDeltaT = 0;
}

 //============================================================
void People::render   ( CViewDynamicList &list, double ts)
{
    if(  m_isNotCreate  )
         return;


    m_calcPosInc = m_attr->m_timeInc;

    CFMatrix3x4 &m = m_skin.GetDirModify();
    m.LoadIdentity();
    m.RotateOzL(m_rotateOz);
    m.RotateOxL(m_rotateOx);
    m.RotateOyL(m_rotateOy);
    CFVector3 np = getPosition()+CFVector3(0,m_attr->m_centerHeight,0);
    const double prediction = rr2nw::ActorPresentationRatio(
         ts, m_lastMoveTimeStamp, m_lastMoveDeltaT);
    if(  prediction > 0  )
         np += (getPosition() - m_lastMovePos)*prediction;
    m.TranslateL( np );

    m_viewDynObj.prepareToRender();
    list.Load( &m_viewDynObj );
}


 //============================================================

void People::endRender( CViewScene *scene )
{
    if(  m_isNotCreate  )
         return;

    scene->RemoveLandDynamic( &m_viewDynObj );
}

 //============================================================
CFVector3     People::realPosition() {  return getPosition();  }


 /*************************************
  *
  *   PeopleTable implementation
  *
  *************************************/

/*
 //============================================================

bool PeopleTable::isRendering()
{
    return 1;
}
*/
 //============================================================
void PeopleTable::allocObjects( int objectQnty )
 {
    m_table = new People[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void PeopleTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *PeopleTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index <= m_maxObjectQnty ,"PeopleTable::getObjectPTR");
    return &(m_table[ index ]);
 }

 /*************************************
  *
  *   AttributeTable implementation
  *
  *************************************/

 //============================================================
void AttributeTablePeople::allocObjects( int objectQnty )
 {
    m_table = new AttributePeople[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void AttributeTablePeople::freeObjects()
 {
    delete [] m_table;
    m_table         = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *AttributeTablePeople::getObjectPTR( int index )
 {
    s_ASSERT(index>=0 && index <m_maxObjectQnty,"AttributeTable::getObjectPTR");
    return &(m_table[ index ]);
 }

 /*************************************
  *
  *   AttributePeople implementation
  *
  *************************************/

 //============================================================
bool People::isShooter()
{
    return m_attr->m_bulletIndex!=-1;
}

void AttributePeople::update(double ts)
{
    m_bulletTable = -1;
    m_bulletIndex = -1;
    if(  m_bulletAttrName[0]!=0  )
    {
         m_bulletTable=g_arena.searchSeanceClassTable("Bullet");

         m_bulletIndex = g_arena.getAttributeIndex(
                                       g_arena.searchSeanceClassTable("BulletAttr"),
                                       context->searchObject(m_bulletAttrName)
                                      );
    }

    m_cannonOfs = CFVector3(m_cannonX,m_cannonY,m_cannonZ);
   /*
    * Search and get skin
    */
    KR_ObjectID skinID =  context->searchObject( m_skinName );
    s_ASSERT( !skinID.isNUL(), "AttributePeople::update" );
    
    m_skinID = skinID;

    KR_Event event;
    event.timeStamp   = ts;
    event.label       = sk_EV_QUERY_MODEL_PTR;
    event.destination = skinID;
    context->sendEventNow( event );
    s_ASSERT(event.label==sk_EV_QUERY_MODEL_PTR_OK,"AttributePeople::update");
    event.data.open(EDO_READ)
                .get(&m_cacheSkin,sizeof(void*))
              .close();

    m_cacheExplosionTable = g_arena.searchSeanceClassTable( "Explosion" );
    m_cacheExplAttr = g_arena.getAttributeIndex( 
                                    g_arena.searchSeanceClassTable( "ExplosionAttr" ),
                                    context->searchObject(m_explAttrName) 
                                 );

    m_smokeTableID = g_arena.searchSeanceClassTable( "Smoke" );
    m_smokeAttrID = context->searchObject(m_smokeAttrName);

    if (!SetSoundAttr(getObjectID(), context,m_soundName,m_ctsndID, (void *)&m_wav))
	m_wav = NULL;

    m_cacheCorpseTable = g_arena.searchSeanceClassTable( "Corpse" );
    m_cacheCorpseAttr  = g_arena.getAttributeIndex( 
                                    g_arena.searchSeanceClassTable( "CorpseAttr" ),
                                    context->searchObject(m_corpseAttrName) 
                                 );


}

void People::onKilled( const CFVector3 &/*explPos*/, double ts )
{

     m_killed = m_attr->m_killStyle == 2
          ? KILL_SINK : KILL_FALLOW_2;
     if(  strcmp(m_attr->m_skinName, "sk.Peop.cln_S00" )==0  )
          m_killed = KILL_SINK;
     else
     if(  strcmp(m_attr->m_skinName, "sk.Taxi.cln_f01" )==0  )
          m_killed = KILL_FALLOW_2;
     m_kill_startPhase  = asin(m_rotateOx);
     m_kill_deltaVAngle = 0;
     m_kill_time        = ts;
}

/*
  ─шЁшцрсы№ ё ьрїр■∙шьш ъЁ√ы№ ьш
 */
static
void AnimateCallBack0(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref)
 {
    //(void)pBase;
    People &self  = *((People*)(ref->GetUserAttrib()));
        int reduc = pBase->ReductionNum();
    double a  = Session::m_moment;
    double c  = cos(a*3);
    double s  = sin(a*3);

    //pBase->KFSet().Mod0("VesloR")
    if(  c > 0  )
    {
         CFVector3 ax(-3.99,7.34,-2.69);
         self.m_askin->get0(1,reduc)->
               LoadIdentity()
               .RotateOz(M_PI/4,ax)
               .RotateOy( 0.8*s,ax)
              .Update();

         ax.x = 3.99;
         self.m_askin->get0(0,reduc)->
               LoadIdentity()
               .RotateOz(-M_PI/4,ax)
               .RotateOy(-0.8*s,ax)
              .Update();
    }
    else
    {
         CFVector3 ax(-3.99,7.34,-2.69);

         double ang = c*4;
         if(  c > M_PI/2  ) 
              c = M_PI/2;
         else
         if(  c < -M_PI/2  )
              c = -M_PI/2;

         self.m_askin->get0(1,reduc)->
               LoadIdentity()
               .RotateOx(c,ax)
               .RotateOz(M_PI/4,ax)
               .RotateOy(0.8*s,ax)
              .Update();
         ax.x = 3.99;
         self.m_askin->get0(0,reduc)->
               LoadIdentity()
               .RotateOx(-c,ax)
               .RotateOz(-M_PI/4,ax)
               .RotateOy(-0.8*s,ax)
              .Update();
    }

 }

//static
void AnimateCallBack1(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref)
 {
    //(void)pBase;
    People &self  = *((People*)(ref->GetUserAttrib()));
    int reduc = pBase->ReductionNum();
    double a  = Session::m_moment;

    CFVector3 ax(0, 2.5, -5.3);
    self.m_askin->get0(0,reduc)->
               LoadIdentity()
               .RotateOz(a*8,ax)
              .Update();
 }

static
void AnimateCallBack2(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref)
 {
    //(void)pBase;
    People &self  = *((People*)(ref->GetUserAttrib()));
    int reduc = pBase->ReductionNum();
    double a  = Session::m_moment;
    double p;

    CFVector3 ax;

    ax = CFVector3(-2.37,0.53,-1.30);
   

    p = -a*18*15/self.m_attr->m_speed;
    self.m_askin->get0(0,reduc)->
               LoadIdentity()
               .RotateOx(p,ax);
    self.m_askin->get0(1,reduc)->
               LoadIdentity()
               .RotateOx(p,ax);

    ax = CFVector3(0.624,0.98,1.67);
    p  = -a*12*15/self.m_attr->m_speed;
    self.m_askin->get0(2,reduc)->
               LoadIdentity()
               .RotateOx(p,ax);
    self.m_askin->get0(3,reduc)->
               LoadIdentity()
               .RotateOx(p,ax);

    ax = CFVector3(-2.37,3.05,-0.37);
    self.m_askin->get0(4,reduc)->
               LoadIdentity()
               .RotateOx(p,ax);

    double ofsy = 0.5*sin(-p),
           ofsz = 0.5*cos(-p);

    self.m_askin->get0(5,reduc)->
               LoadIdentity()
               .Translate( CFVector3(0,ofsy,ofsz) );

    self.m_askin->get (0,reduc)->
               Update();
 }

void shoot( KR_ObjectID fromID, 
            int bulletTable, int attrIndex, 
            const CFVector3 &pos, const CFVector3 &dir,
            SimulationContext *context, double ts )
{
    KR_Event event;

    event.label       = b_EV_START;
    event.source      = fromID;
    event.destination = g_arena.newObject( bulletTable , "B" );

    if(  !event.destination.isNUL()  )
    {
         event.data.open(EDO_WRITE)
                        .descend( VECTOR3D_F, 0 )
                          .putDouble(pos.x)
                          .putDouble(pos.y)
                          .putDouble(pos.z)
                        .ascend()
                        .descend( VECTOR3D_F, 0 )
                          .putDouble(dir.x)
                          .putDouble(dir.y)
                          .putDouble(dir.z)
                        .ascend()
                        .putInt(attrIndex)
                        .putObjectID(fromID)
                   .close();
         event.timeStamp = ts;
         context->sendEventNow( event );
    }
}

void People::onShoot(double ts)
{
      IDynamicObject *dobj = (IDynamicObject*)(context->queryInterface(getEnemyID(),IDynamicObjectIID));
      if(  dobj  )
      {
           CFMatrix3x4 m;
           getMatrix(m);
           m.TranslateR(m_attr->m_cannonOfs)
            .TranslateL(getPosition());

           CFVector3 pos(dobj->getPos());
           CFVector3 mpos(m.Offset());

           switch( m_attr->m_onLand )
           {
           case NO_LAND:
             if(  Abs2(pos-mpos) < 200*200 )
             {
                CFVector3 dir(getMoveDir());

                if(  dir*Normal(pos-mpos) > m_attr->m_shootSkill  )
                {
                     m_correctScale *= 0.8;
                     m_correctScale += 0.001;
                     shoot( getObjectID(),
                           m_attr->m_bulletTable, m_attr->m_bulletIndex,
                           mpos+dir*8, dir,
                           context,ts );

                     if(  m_attr->m_stopIfAttack  )
                     if(  m_prevStartShoot > m_attr->m_burstCount  )
                     {
                          m_prevStartShoot = 0;
                          popState();
                     }
                     else m_prevStartShoot++;
                     m_prevShootTime = ts;
                }
                else m_correctScale = 1;
             }
             else m_correctScale = 1;
             break;

           case ON_OBJ: case ON_LAND: case ON_WATER:
             if(  Abs2(pos-mpos) < 200*200 )
             {
                CFVector3 dir(getMoveDir());
                CFVector3 to(Normal(pos-mpos));

                if(  Normal(CFVector3(dir.x,0,dir.z))
                    *Normal(CFVector3(to.x ,0,to.z)) > m_attr->m_shootSkill  )
                {
                     m_correctScale *= 0.8;
                     m_correctScale += 0.001;
                     shoot( getObjectID(),
                           m_attr->m_bulletTable, m_attr->m_bulletIndex,
                           mpos, to,
                           context,ts );

                     if(  m_attr->m_stopIfAttack  )
                     if(  m_prevStartShoot > m_attr->m_burstCount  )
                     {
                          m_prevStartShoot = 0;
                          popState();
                     }
                     else m_prevStartShoot++;
                     m_prevShootTime = ts;
                }
                else m_correctScale = 1;
             }
             else m_correctScale = 1;
             break;
           }
      }
      else m_correctScale = 1;
}

void  People::pushState( int state, KR_ObjectID enemyID )
{
   if(  m_stateSP < MAX_STATE  )
   {
        m_state[m_stateSP] = state;
        m_stateNextNode[m_stateSP] = m_nextNode;

        if(  state==pe_STATE_ATTACK  )
        {
             m_returnPoint = getPosition();
             s_ASSERT(!enemyID.isNUL(),"People::pushState");
        }

        m_enemyID[m_stateSP] = enemyID;
        m_stateSP++;
   }
}

void People::popState()
{
   if(  m_stateSP > 0  )
   {
        --m_stateSP;
        m_nextNode = m_stateNextNode[m_stateSP];
   }
}



bool	People::dump(PIN_SaveFile & sf)
{
    if (!ct_Subject::dump(sf) ||
        !sf.WriteData( (char *) & m_peopleAttrID, sizeof(PeopleData)  ))
        return false;
		
    return true;
}

bool	People::load(PIN_SaveFile & sf)
{
    if (!ct_Subject::load(sf) ||
        !sf.GetData( (char *) & m_peopleAttrID, sizeof(PeopleData)  ))
        return false;
		
    return true;
}

void	People::loadNotify()
{
    ct_Subject::loadNotify();
    setPeopleAttr();
}


double g_distToSeg(   const CFVector3 &v,   const CFVector3 &p0,   const CFVector3 &p1    )
{
    CFVector3 vec = p1-p0;
    double    len2 = Abs2(vec);
    if(  len2 <= 1e-12  )
         return Abs(v-p0);
    double t = (vec*(v-p0))/len2;
    if(  t<0  ) t = 0;
    else
    if(  t>1  ) t = 1;
    return Abs(v-(p0+vec*t));
}

void g_toSeg( CFVector3 &v, const CFVector3 &p0, const CFVector3 &p1, double max )
{
    if(  !std::isfinite(v.x) || !std::isfinite(v.y) ||
         !std::isfinite(v.z) || !std::isfinite(max) || max<=0  )
         return;
    CFVector3 vec = p1-p0;
    double    len2 = Abs2(vec);
    if(  len2 <= 1e-12  )
    {
         CFVector3 delta = v-p0;
         double distance = Abs(delta);
         if(  distance>max && distance>1e-12  )
              v = p0+delta*(max/distance);
         return;
    }
    double t = (vec*(v-p0))/len2;
    if(  t<0  ) t = 0;
    else
    if(  t>1  ) t = 1;
    CFVector3 nearest = p0+vec*t;
    CFVector3 delta = v-nearest;
    double distance = Abs(delta);
    if(  distance>max && distance>1e-12  )
         v = nearest+delta*(max/distance);
}

class PeopleRouteObjectSource : public IPeopleRouteNodeSource
{
 public:
    PeopleRouteObjectSource(IRouteObject *route) : m_route(route) {}

    virtual int NodeCount() { return m_route->GetNodeCnt(); }
    virtual CFVector3 Node(int index) { return m_route->GetNode(index); }

 private:
    IRouteObject *m_route;
};

void People::setPosition(  const CFVector3 &vec  )
{
    setRoutePosition(vec,0.0,0.0,0);
}

void People::setMovingPosition( const CFVector3 &vec, double deltaT )
{
    CFVector3 delta = vec-getPosition();
    double movementDistance;
    if(  m_attr!=0 && m_attr->m_onLand!=NO_LAND  )
         movementDistance = hypot(delta.x,delta.z);
    else movementDistance = Abs(delta);
    if(  !std::isfinite(movementDistance)  )
         movementDistance = 0;
    setRoutePosition(vec,movementDistance,deltaT,1);
}

void People::setRoutePosition( const CFVector3 &vec,
                               double movementDistance,
                               double deltaT,
                               int moving )
{
    if(  m_attr == 0 || m_routeID.isNUL()  )
    {
         ct_Subject::setPosition( vec );
         return;
    }

    IRouteObject *ri = (IRouteObject*)(context->queryInterface(m_routeID,IRouteObjectIID));
    if(  ri == 0 || ri->GetNodeCnt() < 2  )
    {
         ct_Subject::setPosition( vec );
         return;
    }

    int n0 = m_previousRouteNode;
    int n1 = m_curNode;
    if(  n1<0 || n1>=ri->GetNodeCnt()  )
    {
         ct_Subject::setPosition( vec );
         return;
    }
    if(  n0<0 || n0>=ri->GetNodeCnt()  )
         n0 = (n1-1+ri->GetNodeCnt())%ri->GetNodeCnt();

    PeopleRouteObjectSource source(ri);
    SPeopleRouteMotionRequest request;
    request.candidate = vec;
    request.previousNode = n0;
    request.currentNode = n1;
    request.backSpaceNode = m_startBackSpaceNode;
    request.horizontal = m_attr->m_onLand == NO_LAND ? 0 : 1;
    request.maximumSegments = getState() == pe_STATE_DEFAULT ? 10 : 0;
    request.maximumCorridorDistance = m_attr->m_maxOutDist;
    request.movementDistance = movementDistance;
    request.centerToRoute = moving && !m_isClz;

    SPeopleRouteMotionResult result;
    if(  !PeopleRouteMotion_Advance(&source,request,&result)  )
    {
         CFVector3 fallback = vec;
         g_toSeg( fallback, ri->GetNode(n0), ri->GetNode(n1),
                  m_attr->m_maxOutDist );
         ct_Subject::setPosition( fallback );
         return;
    }

    if(  moving  )
    {
         if(  result.segmentsConsumed>0 || result.degenerateSegmentSeen  )
              m_routeDeviationTime = 0;
         else
         if(  result.outsideCorridor  )
         {
              if(  std::isfinite(deltaT) && deltaT>0  )
                   m_routeDeviationTime += deltaT;
              if(  m_routeDeviationTime>2.5  )
              {
                   m_routeDeviationTime = 0;
                   popState();
              }
         }
         else m_routeDeviationTime = 0;
    }

    if(  result.segmentsConsumed>0 && getState()==pe_STATE_DEFAULT  )
    {
         m_previousRouteNode = result.previousNode;
         m_curNode = result.currentNode;
         m_nextNode = ri->GetNode(m_curNode);
         m_stateNextNode[0] = m_nextNode;

         while(  context->removeEvent(pe_EVC_NEXTNODE,getObjectID())==1  ) {}
         if(  result.stopped  )
              m_dir = CFVector3(0,0,0);
         else
         if(  m_attr->m_speed>1e-3  )
         {
              double dist;
              if(  m_attr->m_onLand==NO_LAND  )
                   dist = Abs(m_nextNode-result.position);
              else dist = hypot(m_nextNode.z-result.position.z,
                                m_nextNode.x-result.position.x);
              if(  std::isfinite(dist)  )
              {
                   KR_Event routeEvent;
                   routeEvent.label = pe_EVC_NEXTNODE;
                   routeEvent.source = getObjectID();
                   routeEvent.destination = getObjectID();
                   routeEvent.timeStamp = m_lastMoveTimeStamp+
                                          dist/m_attr->m_speed;
                   issueEvent(routeEvent);
              }
         }
    }

    ct_Subject::setPosition( result.position );
}

/* End of file C:\WinGame\OBASE\People\People.cpp */
