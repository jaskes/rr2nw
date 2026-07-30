/*
 * File  : D:\GAME\OBASE\GROUP\group.cpp
 * Autor : Suavik
 * Ver   1.0
 */
#include "draw.h"
#include "group.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "kernel/h/session.h"
#include "message/groupmsg.h"
#include "message/comanmsg.h"

#include "message/unitmsg.h"
#include "i/dynobj.i"
#include "i/unit.i"
#include "i/commander.i"
#include "message/dcrossmsg.h"
#include "storage/h/strgdefs.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "TankGroupState.h"

#define HANDLE int
#include "storage/h/savefile.h"

#ifdef  __TRACE_NW__
#include "afxwin.h"
#else
class CDC{};
#endif

static AttributeTankGroup __defaultAttr;

#define ECHO(x)

 //===========================================================================
//strg_SUBJECT_TABLE_IMPLEMENTATION(TankGroup,0)

class TankGroupTable : public ct_SubjectTable
{
 private:
    TankGroup *m_table;
 public:
    TankGroupTable()
    {
        m_table = NULL;
        registerClass( "TankGroup" );
        m_defaultAddMode = CT_KILLINVISIBLE; 
    }
    ~TankGroupTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    TankGroup         *resolve(const KR_ObjectID &object);
};


 //===========================================================================
class AttributeTableTG : public ct_AttributeTable
{
 protected:
    AttributeTankGroup *m_table;

 public:
    AttributeTableTG()
    {
       m_table = NULL;
       registerClass( "TankGroupTable" );
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
};
strg_TABLE_FUNC_IMPLEMENTATION(AttributeTableTG,AttributeTankGroup)

 /*********************************
  *
  *   TankGroup implementation
  *
  *********************************/

 //============================================================
void TankGroupTable::allocObjects( int objectQnty )
 {
    m_table = new TankGroup[ objectQnty ];
    
    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void TankGroupTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *TankGroupTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index < m_maxObjectQnty ,"TankGroup::getObjectPTR");
    return &(m_table[ index ]);
 }

TankGroup *TankGroupTable::resolve(const KR_ObjectID &object)
 {
    for (int index = 0; index < m_maxObjectQnty; ++index)
        if (m_table[index].getObjectID() == object)
            return &m_table[index];
    return NULL;
 }


static AttributeTableTG __attrTable;
static TankGroupTable   __classTable;

 //============================================================
TankGroup::TankGroup()
 {
    m_attr 	= &__defaultAttr;
    m_attrID    = KR_ObjectID::NUL();
 }

 //============================================================
TankGroup::~TankGroup()
 {
 }

 //============================================================
int TankGroup::receiveEvent( KR_Event &event )
 {
    if(  !EVENTHANDLER( event )  )
    switch( event.label )
    {
    case KR_WAKE_UP:
            event.label  = tg_EVC_FIND_ENEMY;
            event.source = getObjectID();
            issueEvent( event );
            break;

    case KR_SET_ATTR:
            {
            //KR_ObjectID oID;
            event.data.open(EDO_READ)
                        .getObjectID(m_attrID)
                      .close();
            ct_Attribute *attr = __attrTable.searchAttribute(m_attrID);
            if( attr==NULL )
                 echo( "TankGroup::receiveEvent: Unknown attribute %s",
                       context->searchObject(m_attrID));
            else m_attr = (AttributeTankGroup*)attr;
            }
            break;

    case GROUP_ADD_MEMBER:
            {
            KR_ObjectID oID;

            event.data.open(EDO_READ)
                       .getObjectID(oID)
                      .close();

            m_members.add(oID);

            s_ASSERT(!m_commander.isNUL(),"Unknown commander");
            event.destination = oID;
            event.source      = getObjectID();
            event.label       = com_EV_GROUP_YOUR_MY_MEMBER;
            event.data.open(EDO_WRITE)
						 .putObjectID(event.source) 
                         .putObjectID(m_commander)
                      .close();
            context->sendEventNow( event );
            }
            break;

    case GROUP_ADD_MEMBER_N:
            {
            char oname[80];

            event.data.open(EDO_READ)
                       .getStr(oname,sizeof(oname))
                      .close();
            KR_ObjectID oID( context->searchObject( oname ));
            if(  oID.isNUL()  )
            {
                 echo("Group: Object %s not found");
                 break;
            }
            m_members.add(oID);
            s_ASSERT(!m_commander.isNUL(),"Unknown commander");
            event.destination = oID;
            event.source      = getObjectID();
            event.label       = com_EV_GROUP_YOUR_MY_MEMBER;
            event.data.open(EDO_WRITE)
						 .putObjectID(event.source) 
                         .putObjectID(m_commander)
                      .close();
            context->sendEventNow( event );
            }
            break;

    case GROUP_DEL_MEMBER:
            {
            KR_ObjectID oID;

            event.data.open(EDO_READ)
                        .getObjectID(oID)
                      .close();
            m_members.del(oID);
            }
            break;

    case com_EV_GROUP_YOUR_MY_MEMBER:
            //m_commander = event.source;
            event.data.open(EDO_READ)
                        .getObjectID(m_commander )
                      .close();
            s_ASSERT(!m_commander.isNUL(),"Group::com_EV_GROUP_YOUR_MY_MEMBER");
            break;

    case tg_EVC_FIND_ENEMY:
            forgetLostTargets(m_lockedTarget);
            findEnemyFC(event);
            break;

    case tg_EV_TARGET_LOST:
    case t_EV_TARGET_LOST:
            unitTargetLost(event);
            break;

    case tg_EVCMD_SET_POSITION:
     {
           double x,y,z, ts;
           event.data.open(EDO_READ)
                       .getDouble(x)
                       .getDouble(y)
                       .getDouble(z)
                     .close();
           m_position =  CFVector3(x,y,z);

           ts = event.timeStamp;

           for(int i = 0; i < m_members.getCount(); ++i)
           {
                event.label      = t_EVCMD_SET_POSITION;
                event.source     = getObjectID();
                event.destination= m_members[i];
                event.timeStamp  = ts;

                event.data.open(EDO_WRITE)
                            .putDouble(x+18*i)
                            .putDouble(y)
                            .putDouble(z)
                           .close();

                context->sendEventNow(event);
          }

          //echo("Group position set to [%.2f, %.2f, %.2f]", x, y, z);
     }
	break;

    default: 
            return 0;
    }
    return 1;
 }

 //============================================================
void TankGroup::addNotify()
 {
    loadStateTransitionTable();
    ct_Subject::addNotify();

    // Subject-table slots are reused after removeObject().  The original
    // initialization only reset the active-state machine and position, so a
    // reconstructed group inherited dead member/target IDs and movement data
    // from its previous incarnation.
    m_lockedTarget.clr();
    m_members.clr();
    m_attr = &__defaultAttr;
    m_attrID = KR_ObjectID::NUL();
    d_mov = MovingData();
    startInitialize();
    resetState();

    m_position =  CFVector3(2060,0,-2390);
 }

 //============================================================
void TankGroup::removeNotify()
 {
    KR_Event event;
    event.label       = GROUP_I_AM_DEAD;
    event.destination = m_commander;
    event.source      = getObjectID();
    event.timeStamp   = Session::m_moment;
    //issueEvent(event);
    context->sendEventNow(event);

    ct_Subject::removeNotify();
    // insert your code this
 }

void TankGroup::forcedRemoveNotify() 
{

	// Tell our members that they are can be free now
	// Go beserk, guys ;-)
	
	KR_Event event;
	KR_ObjectID oID = getObjectID();

	int number = m_members.getCount();

	for (int i = 0; i < number; i++)
	{		
		
		event.destination = m_members[i];
		event.source      = oID;
		event.timeStamp   = Session::m_moment;
		event.label       = com_EV_I_AM_DEAD;
		context->sendEventNow( event );
	}

//	ICommander *com = (ICommander *) context->queryInterface(m_commander,ICommanderIID);
//	com->removeMember(oID);

}

 //============================================================

int TankGroup::findVarior(const CFVector2 &enemyCoord,double &minDist)
{
    int nearest = -1;
    minDist=1e10;

    for( int i = 0; i < m_members.getCount(); ++i )
    {
         IDynamicObject *dobj = (IDynamicObject *)(context->queryInterface(m_members[i],IDynamicObjectIID));
         IUnit          *uobj = (IUnit          *)(context->queryInterface(m_members[i],IUnitIID         ));

         if(  dobj==0 || uobj==0  )
         {
              m_members.del(i);
              --i;
              continue;
         }

         CFVector3 pos(dobj->getPos());
         CFVector2 c(pos.x,pos.z);

         double dist = Abs(c-enemyCoord)*uobj->desireShoot()
                      /(Abs( m_position-pos )+10);
         /*
         char b[100];
         
         sprintf(b,"dist=%lg",dist);
         dc_CreateCross(0,pos,40,0,0,1,1,0,b);
         */
         if(  dist < minDist  )
         {
              nearest = i;
              minDist = dist;
         }
         
    }

    if(  m_members.getCount()==0  )
    {
         context->removeObject(getObjectID());
         return -1;
    }

    return nearest;
}

 /*******************************
  *
  * Основная команда - перейти всей группой в новую
  * точку
  * Необходимо запустить 2 цикла:
  * tg_EVC_FIND_ENEMY
  * tg_EVC_MOVING
  *
  *******************************/
void TankGroup::gotoF( KR_Event &event )
{
    ECHO("gotoF");
    double vecx,
           vecz;

    //{{GET_EVENT(tg_EVCMD_GO_TO)
    event.data.open(EDO_READ)
                   .descend( VECTOR_LAND, 0 )
                     .getDouble(vecx)
                     .getDouble(vecz)
                   .ascend()
              .close();
    //}}END_OF_GET_EVENT(tg_EVCMD_GO_TO)
    d_mov.m_crd = CFVector3(vecx,0,vecz);

    gotoMOVING( event );
}

void TankGroup::attackEnemy(KR_ObjectID enemyID, double ts)
{
    CFVector2 enemyCoord;
    double minDist = 1e10;
    int    nearest = -1;

    IDynamicObject *edobj = (IDynamicObject *)(context->queryInterface(enemyID,IDynamicObjectIID));
    if(  edobj==0  )
         return;
    

    CFVector3 enemyPos(edobj->getPos());
    enemyCoord.x = enemyPos.x;
    enemyCoord.y = enemyPos.z;

    if(  (nearest = findVarior(enemyCoord,minDist)) >= 0  )
    {
         KR_Event event;

         event.label       = t_EV_ATTACK;
         event.source      = getObjectID();
         event.destination = m_members[nearest];
         event.timeStamp   = ts;
         //{{PUT_EVENT(t_EV_ATTACK)
         event.data.open(EDO_WRITE)
                        .putObjectID(enemyID)
                   .close();
         //}}END_OF_PUT_EVENT(t_EV_ATTACK)
         KR_Event ev;
         ev.getCopy(event);
         context->sendEventNow( ev );

         //dc_CreateCross(0,m_position,10,0,0,1,1,0,"ATTACK");
    }
}


 /*******************************
  *
  * Мы прибыли на место.
  * Возможно необходимо уведомить об этом
  * командира
  *
  *******************************/
void TankGroup::move_ReachedF( KR_Event &event )
{
    ECHO("move_ReachedF");

    //{{GET_EVENT(tg_EV_REACHED)
    //}}END_OF_GET_EVENT(tg_EV_REACHED)

    event.source      = getObjectID();
    event.destination = m_commander;
    event.label       = com_EV_GROUP_REACHED;
    //{{PUT_EVENT(com_EV_GROUP_REACHED)
    //}}END_OF_PUT_EVENT(com_EV_GROUP_REACHED)
    issueEvent( event );

    CFVector2 memCoord;
    //dc_CreateCross(0,m_position,10,0,0,1,1,0,"WAIT");
     /*
      * Просим подождать
      */
    for( int i = 0; i < m_members.getCount(); ++i )
    {
         const KR_ObjectID curMember = m_members[i];
         IDynamicObject *dobj = (IDynamicObject *)(context->queryInterface(curMember,IDynamicObjectIID));

         if(  dobj==0  )
         {
              m_members.del(i);
              --i;
              continue;
         }


         CFVector3 p(dobj->getPos());
         memCoord.x = p.x;
         memCoord.y = p.z;
         
         /*
          * Если член группы Близко просим подождать
          */
         //dc_CreateCross(0,p,10,0,0,1,1,0,"WAIT");
         double moveSpeed   = 0.1,
                xVec        = m_position.x-memCoord.x,
                yVec        = m_position.z-memCoord.y,
                maxMoveDist = 20;

         event.source      = getObjectID();
         event.destination = curMember;
         event.label       = t_EV_MOVE;
         //event.timeStamp   = curTimeStamp;
         //{{PUT_EVENT(t_EV_MOVE)
         event.data.open(EDO_WRITE)
                        .putDouble(moveSpeed)
                        .descend( VECTOR_LAND, 0 )
                          .putDouble(xVec)
                          .putDouble(yVec)
                        .ascend()
                        .putDouble(maxMoveDist)
                   .close();
         //}}END_OF_PUT_EVENT(t_EV_MOVE)
         context->sendEventNow( event );
    }

    if(  m_members.getCount()==0  )
         context->removeObject(getObjectID());

}

 /*******************************
  *
  * При получении события сканируется окружение
  * на предмет наличия врагов и опять шлется событие
  * tg_EVC_FIND_ENEMY. Если враг найден, то шлетс
  * событие tg_EV_TARGET_LOCKED
  *
  *******************************/
void TankGroup::findEnemyFC( KR_Event &event )
{
    //echo("findEnemyFC");

    KR_ObjectID enemyID = findEnemy( event.timeStamp );
    if(  !enemyID.isNUL()  )
    {
         m_lockedTarget.add(enemyID);
         attackEnemy( enemyID, event.timeStamp );

         //dc_CreateCross(0,m_position,40,0,0,1,1,0,"FIND ENEMY");
    }

    event.source      = getObjectID();
    event.destination = getObjectID();
    event.timeStamp  += GROUP_FIND_TARGET_TIME_DELTA;
    event.label       = tg_EVC_FIND_ENEMY;
         //{{PUT_EVENT(tg_EVC_FIND_ENEMY)
         //}}END_OF_PUT_EVENT(tg_EVC_FIND_ENEMY)

    issueEvent( event );
}

 /*******************************
  *
  * Движение по заданной трассе к указанной точке
  * Для продолжения цикла испускается tg_EVC_MOVING,
  * по прибытии на место - tg_EV_REACHED
  *
  *******************************/
void TankGroup::movingFC( KR_Event &event )
{
//    echo("movingFC  %i",m_currentState);

    s_ENTRY(TankGroup::movingFC)
    double prevTimeStamp;


    //{{GET_EVENT(tg_EVC_MOVING)
    event.data.open(EDO_READ)
                   .getDouble(prevTimeStamp)
              .close();
    //}}END_OF_GET_EVENT(tg_EVC_MOVING)


    if(  moveToPosition( prevTimeStamp, event.timeStamp,
                         CFVector2( d_mov.m_crd.x, d_mov.m_crd.z )  )  
      )
    {
        /*
         char b[100];
         
         sprintf(b,"%i",m_currentState);
         dc_CreateCross(0,realPosition(),10,0,0,1,1,0,b);
         */
         /*
          * Repeat operation
          */
         prevTimeStamp    = event.timeStamp;

         event.label      = tg_EVC_MOVING;
         event.source     = getObjectID();
         event.timeStamp += GROUP_MOVING_TIME_DELTA;
         //{{PUT_EVENT(tg_EVC_MOVING)
         event.data.open(EDO_WRITE)
                        .putDouble(prevTimeStamp)
                   .close();
         //}}END_OF_PUT_EVENT(tg_EVC_MOVING)
         issueEvent( event );
    }
}

 /*******************************
  *
  * Вызывается при потере юнитом цели
  *
  *******************************/
void TankGroup::unitTargetLost( KR_Event &event )
{
    KR_ObjectID enemyID;
    event.data.open(EDO_READ)
                .getObjectID(enemyID)
              .close();

    m_lockedTarget.del(enemyID);
//    dc_CreateCross(0,m_position,40,0,0,1,1,0,"TARGET LOST");

}

//{{ACTIONF_IMPLEMENTATION

void TankGroup::loadStateTransitionTable()
 {
    //{{TRANSLATION_TABLE
    static KR_ActiveObject::StateTransitionTableElem row0[1] =
    {
        {tg_EVCMD_GO_TO, ST_NEW_STATE,  1, (ACTION)gotoF         }
    };
    static KR_ActiveObject::StateTransitionTableElem row1[2] =
    {
        {tg_EVC_MOVING , ST_NEW_STATE,  1, (ACTION)movingFC      },
        {tg_EV_REACHED , ST_NEW_STATE,  0, (ACTION)move_ReachedF }
    };
    //}}END_OF_TRANSLATION_TABLE{{
    static StateElem STT[2]={
      StateElem(1,row0),
      StateElem(2,row1)
    };
    m_stateTable = STT;
    m_stateQnty  = 2;
    //}}END_OF_STATE_TABLE
 }


bool	TankGroup::dump(PIN_SaveFile & sf)
{
    if (!ct_Subject::dump(sf) ||
		!KR_ActiveObject::dump(sf) ||
		!m_lockedTarget.dump(sf) ||
		!m_members.dump(sf) ||
		!sf.WriteData((char *) &m_position, sizeof(CFVector3)) ||
		!sf.WriteData((char *) &m_commander, sizeof(KR_ObjectID)) ||
		!sf.WriteData((char *) &m_attrID, sizeof(KR_ObjectID)) ||
		!sf.WriteData((char *) &d_mov, sizeof(MovingData)))
		return false;

	return true;
}
bool	TankGroup::load(PIN_SaveFile & sf)
{
    if (!ct_Subject::load(sf) ||
		!KR_ActiveObject::load(sf) ||
		!m_lockedTarget.load(sf) ||
		!m_members.load(sf) ||
		!sf.GetData((char *) &m_position, sizeof(CFVector3)) ||
		!sf.GetData((char *) &m_commander, sizeof(KR_ObjectID)) ||
		!sf.GetData((char *) &m_attrID, sizeof(KR_ObjectID)) ||
		!sf.GetData((char *) &d_mov, sizeof(MovingData)))
		return false;

	return true;
}

void	TankGroup::loadNotify()
{
	loadStateTransitionTable();
    ct_Subject::loadNotify();

	ct_Attribute *attr = __attrTable.searchAttribute(m_attrID);
	if( attr==NULL )
	{
		m_attr = &__defaultAttr;
		//echo( "TankGroup::receiveEvent: Unknown attribute %s",
		//context->searchObject(m_attrID));
	}
	else 
		m_attr = (AttributeTankGroup*)attr;
}

namespace {

const unsigned long long kTankGroupHashOffset = 14695981039346656037ull;
const unsigned long long kTankGroupHashPrime = 1099511628211ull;

struct StableTankGroupRecord
{
    std::string name;
    std::string commander;
    std::string attribute;
    std::vector<std::string> members;
    CFVector3 position;
    CFVector3 destination;

    bool operator==(const StableTankGroupRecord &other) const
    {
        return name == other.name && commander == other.commander &&
               attribute == other.attribute && members == other.members &&
               position.x == other.position.x &&
               position.y == other.position.y &&
               position.z == other.position.z &&
               destination.x == other.destination.x &&
               destination.y == other.destination.y &&
               destination.z == other.destination.z;
    }
};

struct StableTankGroupRoster
{
    SimulationContext *context;
    std::vector<StableTankGroupRecord> records;
    bool valid;
};

std::string TankGroupSymbolicName(SimulationContext *context,
                                  const KR_ObjectID &object)
{
    if (context == NULL)
        return std::string();
    KR_ObjectID mutableObject = object;
    if (mutableObject.isNUL())
        return std::string();
    const char *name = context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

bool CollectStableTankGroup(const KR_ObjectID object, void *user)
{
    StableTankGroupRoster *roster =
        static_cast<StableTankGroupRoster *>(user);
    TankGroup *group = __classTable.resolve(object);
    if (group == NULL)
    {
        roster->valid = false;
        return false;
    }
    StableTankGroupRecord record;
    record.name = TankGroupSymbolicName(roster->context, object);
    record.commander =
        TankGroupSymbolicName(roster->context, group->m_commander);
    record.attribute =
        TankGroupSymbolicName(roster->context, group->m_attrID);
    record.position = group->m_position;
    record.destination = group->d_mov.m_crd;
    if (record.name.empty() || record.commander.empty())
    {
        roster->valid = false;
        return false;
    }
    for (int index = 0; index < group->m_members.getCount(); ++index)
    {
        const std::string member =
            TankGroupSymbolicName(roster->context, group->m_members[index]);
        if (member.empty())
        {
            roster->valid = false;
            return false;
        }
        record.members.push_back(member);
    }
    std::sort(record.members.begin(), record.members.end());
    roster->records.push_back(record);
    return true;
}

bool CollectStableTankGroups(SimulationContext *context,
                             std::vector<StableTankGroupRecord> *records)
{
    if (context == NULL || records == NULL)
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("TankGroup");
    if (table == ct_NULLID)
        return false;
    StableTankGroupRoster roster = {
        context, std::vector<StableTankGroupRecord>(), true};
    g_arena.userFind(table, CollectStableTankGroup, &roster);
    if (!roster.valid)
        return false;
    std::sort(roster.records.begin(), roster.records.end(),
              [](const StableTankGroupRecord &left,
                 const StableTankGroupRecord &right)
              {
                  return left.name < right.name;
              });
    records->swap(roster.records);
    return true;
}

void PutTankGroupU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void PutTankGroupU64(std::vector<unsigned char> *bytes, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

bool GetTankGroupU32(const std::vector<unsigned char> &bytes,
                     size_t *offset, std::uint32_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 4)
        return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
    return true;
}

bool GetTankGroupU64(const std::vector<unsigned char> &bytes,
                     size_t *offset, std::uint64_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 8)
        return false;
    *value = 0;
    for (int shift = 0; shift < 64; shift += 8)
        *value |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
    return true;
}

bool PutTankGroupString(std::vector<unsigned char> *bytes,
                        const std::string &value)
{
    if (bytes == NULL || value.size() > 4096)
        return false;
    PutTankGroupU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
}

bool GetTankGroupString(const std::vector<unsigned char> &bytes,
                        size_t *offset, std::string *value)
{
    std::uint32_t size = 0;
    if (value == NULL || !GetTankGroupU32(bytes, offset, &size) ||
        size > 4096 || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    if (size == 0)
        value->clear();
    else
        value->assign(reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return true;
}

void PutTankGroupDouble(std::vector<unsigned char> *bytes, double value)
{
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double must be 64-bit");
    memcpy(&bits, &value, sizeof(bits));
    PutTankGroupU64(bytes, bits);
}

bool GetTankGroupDouble(const std::vector<unsigned char> &bytes,
                        size_t *offset, double *value)
{
    std::uint64_t bits = 0;
    if (value == NULL || !GetTankGroupU64(bytes, offset, &bits))
        return false;
    memcpy(value, &bits, sizeof(bits));
    return true;
}

void PutTankGroupVector(std::vector<unsigned char> *bytes,
                        const CFVector3 &value)
{
    PutTankGroupDouble(bytes, value.x);
    PutTankGroupDouble(bytes, value.y);
    PutTankGroupDouble(bytes, value.z);
}

bool GetTankGroupVector(const std::vector<unsigned char> &bytes,
                        size_t *offset, CFVector3 *value)
{
    return value != NULL &&
           GetTankGroupDouble(bytes, offset, &value->x) &&
           GetTankGroupDouble(bytes, offset, &value->y) &&
           GetTankGroupDouble(bytes, offset, &value->z);
}

bool EncodeStableTankGroups(
    const std::vector<StableTankGroupRecord> &records,
    std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > 512)
        return false;
    bytes->clear();
    PutTankGroupU32(bytes, 0x50524754u); // TGRP, little endian
    PutTankGroupU32(bytes, 1u);
    PutTankGroupU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (size_t index = 0; index < records.size(); ++index)
    {
        const StableTankGroupRecord &record = records[index];
        if (!PutTankGroupString(bytes, record.name) ||
            !PutTankGroupString(bytes, record.commander) ||
            !PutTankGroupString(bytes, record.attribute) ||
            record.members.size() > KR_SetOfID::MAX_ID_CNT)
            return false;
        PutTankGroupU32(
            bytes, static_cast<std::uint32_t>(record.members.size()));
        for (size_t member = 0; member < record.members.size(); ++member)
            if (!PutTankGroupString(bytes, record.members[member]))
                return false;
        PutTankGroupVector(bytes, record.position);
        PutTankGroupVector(bytes, record.destination);
    }
    return true;
}

bool DecodeStableTankGroups(const std::vector<unsigned char> &bytes,
                            std::vector<StableTankGroupRecord> *records)
{
    size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetTankGroupU32(bytes, &offset, &magic) ||
        !GetTankGroupU32(bytes, &offset, &version) ||
        !GetTankGroupU32(bytes, &offset, &count) ||
        magic != 0x50524754u || version != 1u || count > 512)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableTankGroupRecord record;
        std::uint32_t members = 0;
        if (!GetTankGroupString(bytes, &offset, &record.name) ||
            !GetTankGroupString(bytes, &offset, &record.commander) ||
            !GetTankGroupString(bytes, &offset, &record.attribute) ||
            !GetTankGroupU32(bytes, &offset, &members) ||
            members > KR_SetOfID::MAX_ID_CNT)
            return false;
        for (std::uint32_t member = 0; member < members; ++member)
        {
            std::string name;
            if (!GetTankGroupString(bytes, &offset, &name))
                return false;
            record.members.push_back(name);
        }
        if (!GetTankGroupVector(bytes, &offset, &record.position) ||
            !GetTankGroupVector(bytes, &offset, &record.destination))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

void HashTankGroupBytes(unsigned long long *hash, const void *data,
                        size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (size_t index = 0; index < size; ++index)
    {
        *hash ^= bytes[index];
        *hash *= kTankGroupHashPrime;
    }
}

} // namespace

void TankGroupState_Link()
{
    (void)__classTable.getClassTableID();
}

int TankGroupState_LiveCount(SimulationContext *context)
{
    std::vector<StableTankGroupRecord> records;
    return CollectStableTankGroups(context, &records)
        ? static_cast<int>(records.size()) : -1;
}

int TankGroupState_MemberCount(SimulationContext *context,
                               const KR_ObjectID &group)
{
    TankGroup *resolved = context == NULL ? NULL : __classTable.resolve(group);
    return resolved == NULL ? -1 : resolved->m_members.getCount();
}

bool TankGroupState_HasMember(SimulationContext *context,
                              const KR_ObjectID &group,
                              const KR_ObjectID &member)
{
    TankGroup *resolved = context == NULL ? NULL : __classTable.resolve(group);
    return resolved != NULL && resolved->m_members.find(member) >= 0;
}

KR_ObjectID TankGroupState_Commander(SimulationContext *context,
                                     const KR_ObjectID &group)
{
    TankGroup *resolved = context == NULL ? NULL : __classTable.resolve(group);
    return resolved == NULL ? KR_ObjectID::NUL() : resolved->m_commander;
}

unsigned long long TankGroupState_Fingerprint(SimulationContext *context)
{
    std::vector<StableTankGroupRecord> records;
    std::vector<unsigned char> bytes;
    if (!CollectStableTankGroups(context, &records) ||
        !EncodeStableTankGroups(records, &bytes))
        return 0;
    unsigned long long hash = kTankGroupHashOffset;
    if (!bytes.empty())
        HashTankGroupBytes(&hash, &bytes[0], bytes.size());
    return hash;
}

bool TankGroupState_StableRoundTrip(SimulationContext *context,
                                    const KR_ObjectID &group)
{
    TankGroup *resolved = context == NULL ? NULL : __classTable.resolve(group);
    if (resolved == NULL)
        return false;
    StableTankGroupRecord before;
    before.name = TankGroupSymbolicName(context, resolved->getObjectID());
    before.commander = TankGroupSymbolicName(context, resolved->m_commander);
    before.attribute = TankGroupSymbolicName(context, resolved->m_attrID);
    before.position = resolved->m_position;
    before.destination = resolved->d_mov.m_crd;
    for (int index = 0; index < resolved->m_members.getCount(); ++index)
        before.members.push_back(
            TankGroupSymbolicName(context, resolved->m_members[index]));
    std::sort(before.members.begin(), before.members.end());
    std::vector<StableTankGroupRecord> beforeRecords(1, before);
    std::vector<StableTankGroupRecord> afterRecords;
    std::vector<unsigned char> bytes;
    return !before.name.empty() && !before.commander.empty() &&
           !before.members.empty() &&
           EncodeStableTankGroups(beforeRecords, &bytes) &&
           DecodeStableTankGroups(bytes, &afterRecords) &&
           beforeRecords == afterRecords;
}

bool TankGroupState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    std::vector<StableTankGroupRecord> records;
    return CollectStableTankGroups(context, &records) &&
           EncodeStableTankGroups(records, bytes);
}

bool TankGroupState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableTankGroupRecord> records;
    return DecodeStableTankGroups(bytes, &records);
}

bool TankGroupState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableTankGroupRecord> current, expected;
    return CollectStableTankGroups(context, &current) &&
           DecodeStableTankGroups(bytes, &expected) && current == expected;
}

bool TankGroupState_ProbeScheduler(
    SimulationContext *context, const KR_ObjectID &group, double timeStamp,
    STankGroupSchedulerProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    summary->findEnemyCycles = 0;
    summary->movingCycles = 0;
    TankGroup *resolved = context == NULL ? NULL : __classTable.resolve(group);
    if (resolved == NULL || resolved->m_members.getCount() == 0)
        return false;

    KR_Event position;
    position.label = tg_EVCMD_SET_POSITION;
    position.source = g_arena.getObjectID();
    position.destination = group;
    position.timeStamp = timeStamp;
    position.data.open(EDO_WRITE)
                 .putDouble(512.0)
                 .putDouble(10000.0)
                 .putDouble(-512.0)
                 .close();
    if (resolved->receiveEvent(position) != 1)
        return false;

    const bool initialFind =
        context->removeEvent(tg_EVC_FIND_ENEMY, group) == 1;
    KR_Event findEnemy;
    findEnemy.label = tg_EVC_FIND_ENEMY;
    findEnemy.source = group;
    findEnemy.destination = group;
    findEnemy.timeStamp = timeStamp + 0.1;
    const bool findExecuted = initialFind &&
        resolved->receiveEvent(findEnemy) == 1;
    const bool findRepeated =
        context->removeEvent(tg_EVC_FIND_ENEMY, group) == 1;
    if (findExecuted && findRepeated)
        summary->findEnemyCycles = 1;

    KR_Event goTo;
    goTo.label = tg_EVCMD_GO_TO;
    goTo.source = g_arena.getObjectID();
    goTo.destination = group;
    goTo.timeStamp = timeStamp + 0.2;
    goTo.data.open(EDO_WRITE)
        .descend(VECTOR_LAND, 0)
          .putDouble(532.0)
          .putDouble(-512.0)
        .ascend()
        .close();
    const bool moveAccepted = resolved->receiveEvent(goTo) == 1;
    const bool moveScheduled =
        context->removeEvent(tg_EVC_MOVING, group) == 1;

    KR_Event moving;
    moving.label = tg_EVC_MOVING;
    moving.source = group;
    moving.destination = group;
    moving.timeStamp = timeStamp + 0.6;
    moving.data.open(EDO_WRITE).putDouble(timeStamp + 0.2).close();
    const bool moveExecuted = moveScheduled &&
        resolved->receiveEvent(moving) == 1;
    const bool moveRepeated =
        context->removeEvent(tg_EVC_MOVING, group) == 1;
    if (moveAccepted && moveExecuted && moveRepeated)
        summary->movingCycles = 1;

    return summary->findEnemyCycles == 1 && summary->movingCycles == 1;
}



/* End of file D:\GAME\OBASE\GROUP\group.cpp */
