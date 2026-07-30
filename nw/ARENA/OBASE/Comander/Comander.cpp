#include "kernel/h/echo.h"
#include "kernel/h/context.h"
#include "kernel/h/setofid.h"
#include "storage/h/subject.h"

#include "message/comanmsg.h"
#include "message/groupmsg.h"
#include "enum/spaceenum.h"

#include "i/route.i"
#include "i/commander.i"

#define HANDLE int
#include "storage\h\savefile.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "CommanderState.h"



#define MAX_MEMBER 250
#define GROUP_MAX_ROUTE_NUM 4

s_ELN commanderMsgNames[] =
 {
   s_ELN(COMMANDER_ADD_MEMBER_N,"COMMANDER_ADD_MEMBER_N"),
   s_ELN(com_EV_GROUP_REACHED,"com_EV_GROUP_REACHED"),
   s_ELN(com_EV_GROUP_TARGET_DESTROYED,"com_EV_GROUP_TARGET_DESTROYED"),
   s_ELN(com_EV_GROUP_YOUR_MY_MEMBER,"com_EV_GROUP_YOUR_MY_MEMBER"),
   s_ELN(com_EV_PLANE0,"com_EV_PLANE0"),
   s_ELN()
 };

s_ELNTable commanderMsgTable("Commander messages",commanderMsgNames);

class GroupMember{
public:
    GroupMember() { reset(); }

	KR_ObjectID m_id;
	int m_patrol;
	int m_routeCnt;
	int m_curRoute;
	struct Route
        {
	    KR_ObjectID id;
	    int repCnt;
	    int repeated;
	    int patrol;
	    int curNode;
	    int nodeCnt;
	    int dir;
	    double waitNext;

            void reset()
            {
                id       = KR_ObjectID::NUL();
                repCnt   = 0;
                repeated = 0;
                waitNext = 0;
                nodeCnt  = 0;
                curNode  = 0; 
                dir = 1; 
                patrol = FALSE;
            }

            Route() 
            { 
                reset();
            }
	} 
          m_route[GROUP_MAX_ROUTE_NUM];

        void reset()
        {
            m_id = KR_ObjectID::NUL();
            m_patrol   = 0;
            m_routeCnt = 0;
            m_curRoute = 0;

            for( int i = 0; i < GROUP_MAX_ROUTE_NUM; ++i )
                 m_route[i].reset();
        }
};


typedef struct {
	GroupMember  m_member[MAX_MEMBER];
    int          m_memberQnty;

    double       m_xMoving,m_zMoving;
    int          m_wMoving,m_dMoving;
	
	KR_SetOfID	 m_hostileCommanders;
	KR_SetOfID	 m_friendlyCommanders;

} CommanderData;


class com_Commander:	public ICommander,
						public ct_Object,
						public CommanderData
 {
 public:    

	void *queryInterface( int interNum );

    virtual void addNotify();
    virtual int  receiveEvent(KR_Event &event);

    void LoadRoute(int rNum, const char *fName, const char *rName, int partol);
    int  loadRoute(const char *rName, const char *fName);
    void addRoute(int mNum,const char *rName, int patrol);
    void clrRoute(int mNum);
    void setToStart(int mNum,double ts);

	virtual bool	shouldDump () { return true; } 	
	virtual bool	dump(PIN_SaveFile & sf);
	virtual bool	load(PIN_SaveFile & sf);
	virtual void	loadNotify();     


    virtual void       setFriendly(KR_ObjectID &);
    virtual void       setHostile (KR_ObjectID &);

    virtual int       isFriendly(KR_ObjectID &);
    virtual int       isHostile (KR_ObjectID &);


	//virtual void	  removeMember(KR_ObjectID &);

 };

class com_CommanderTable: public ct_ClassTable
 {
 private:
    com_Commander *m_table;
 public:
    com_CommanderTable()
    {
       registerClass("Commander");
    }
    virtual  void allocObjects( int objectQnty );
    virtual  void freeObjects();
    virtual  ct_Object  *getObjectPTR( int index );
 };

com_CommanderTable CommanderTable;

 /******************************
  *
  *       Commander
  *
  ******************************/

IRouteObject *newRoute( SimulationContext *c, const char *rname )
{
    KR_ObjectID rID = g_arena.newObject("Route",rname);
    if(  rID.isNUL()  )
         return 0;
    return (IRouteObject *)(c->queryInterface(rID,IRouteObjectIID));
}


void *com_Commander::queryInterface( int interNum )
{
    switch( interNum )
    {
    case IUnknownIID:   return (KR_Object  *) this;
    case ICommanderIID: return (ICommander *) this;
    }

    return 0;
}


//============================================================================
void com_Commander::addNotify()
 {
    int i;
    m_memberQnty = 0;
    m_xMoving =  2060;
    m_zMoving = -2390;
    m_wMoving = 100;
    m_dMoving = 100;

    for( i = 0; i < MAX_MEMBER; ++i )
         m_member[i].reset();
 }
//============================================================================

/*void com_Commander::removeMember(KR_ObjectID &mID)
{
	for (int i = 0; i < m_memberQnty; i++)
		if (mID == m_member[i].m_id)
		{
			for (int j = i; j < m_memberQnty - 1; j++)
			{
				m_member[j] = m_member[j+1];
			}
			m_memberQnty--;
			break;
		}
}*/

//============================================================================
void com_Commander::LoadRoute(int rNum, const char *fName, const char *rName, int patrol){
	IRouteObject *route;

	route = newRoute(context, rName);
        if(  route==0  )
             return;

	route->Load(fName);
	m_member[rNum].m_route[m_member[rNum].m_routeCnt].id      = route->getID();
	m_member[rNum].m_route[m_member[rNum].m_routeCnt].nodeCnt = route->GetNodeCnt();
	m_member[rNum].m_route[m_member[rNum].m_routeCnt].patrol  = patrol;
	m_member[rNum].m_routeCnt++;
}

void SetMemberPos( com_Commander *com,double ts,KR_ObjectID memID, const CFVector3 &node)
{
    KR_Event event;
    event.timeStamp   = ts;
    event.source      = com->getObjectID();
    event.destination = memID;
    event.label       = tg_EVCMD_SET_POSITION;
    
    event.data.open(EDO_WRITE)
                 .putDouble(node.x)
				 .putDouble(node.y)
                 .putDouble(node.z)
              .close();
    
    com->context->sendEventNow( event );
}

void GotoMemberPos( com_Commander *com,double ts,KR_ObjectID memID, const CFVector3 &node)
{
    KR_Event event;
    event.timeStamp   = ts;
	event.source      = com->getObjectID();
    event.destination = memID;
    event.label       = tg_EVCMD_GO_TO;
    //{{PUT_EVENT(tg_EVCMD_GO_TO)

    event.data.open(EDO_WRITE)
                   .descend( VECTOR_LAND, 0 )
                     .putDouble(node.x)
                     .putDouble(node.z)
                   .ascend()
              .close();
    //}}END_OF_PUT_EVENT(tg_EVCMD_GO_TO)
    com->context->sendEventNow( event );
}

//============================================================================
void com_Commander::addRoute(int mNum,const char *rName, int patrol)
 {
        if(  mNum < 0 || mNum >= m_memberQnty )
        {
             echo("com_Commander::addRoute: Range check error!");
             return;
        }

        KR_ObjectID rID(context->searchObject(rName));
        if(  rID.isNUL()  )
        {
             echo("com_Commander::addRoute: Unknown route %s", rName);
             return;
        }
	IRouteObject *route = (IRouteObject *)(context->queryInterface(rID,IRouteObjectIID));
        if(  route==NULL  )
        {
             echo("com_Commander::addRoute: This [%s] not route",rName);
             return;
        }

        int &rCnt = m_member[mNum].m_routeCnt;
        if(  rCnt >= GROUP_MAX_ROUTE_NUM )
        {
             echo("com_Commander::addRoute:Route Table overflow!");
             return;
        }
        route->AddRef();
	m_member[mNum].m_route[rCnt].id      = route->getID();
	m_member[mNum].m_route[rCnt].nodeCnt = route->GetNodeCnt();
	m_member[mNum].m_route[rCnt].patrol  = patrol;
	rCnt++;
 }

//============================================================================
int  com_Commander::loadRoute(const char *rName, const char *fName)
 {
    IRouteObject *route;

    route = newRoute(context, rName);
    if(  route != 0 )
    {
         route->Load(fName);
         return 1;
    }
    return 0;
 }

//============================================================================
void com_Commander::clrRoute(int mNum)
 {
    if(  mNum < 0 || mNum >= m_memberQnty )
    {
         echo("com_Commander::addRoute: Range check error!");
         return;
    }
    m_member[mNum].m_routeCnt = 0;
 }


//============================================================================
void com_Commander::setToStart(int mNum,double ts)
 {
   if(  mNum < 0 || mNum >= m_memberQnty  )
   {
        echo("com_Commander::setToStart: member range error");
        return;
   }
   if(  m_member[mNum].m_routeCnt <= 0  )
   {
        echo("com_Commander::setToStart: member has no route");
        return;
   }

   IRouteObject *
   riFace =(IRouteObject *)(getContext()->queryInterface( m_member[mNum].m_route[0].id, IRouteObjectIID));

   if(  riFace == 0  )
   {
        echo("com_Commander::setToStart: route not found");
        return;
   }
   if(  riFace->GetNodeCnt() < 2  )
   {
        echo("com_Commander::setToStart: route has less than 2 nodes");
        return;
   }

   SetMemberPos (this,ts,m_member[mNum].m_id,       riFace->GetNode(0));
   GotoMemberPos(this,ts,m_member[mNum].m_id,       riFace->GetNode(1));
//echo("Group #0[%s] send to [%.2f, %.2f, %.2f]", context->searchObject(m_member[i].m_id),node.x, node.y, node.z); 

 }

void com_Commander::setFriendly(KR_ObjectID & oID)
{
	m_friendlyCommanders.add ( oID );
	m_hostileCommanders .del ( oID );
	
}

void com_Commander::setHostile (KR_ObjectID & oID)
{
	m_friendlyCommanders.del ( oID );
	m_hostileCommanders .add ( oID );
}

int com_Commander::isFriendly(KR_ObjectID & oID)
{
	if (oID == getObjectID())
		return 1;

	return (m_friendlyCommanders.find(oID) != -1);
}

int com_Commander::isHostile (KR_ObjectID & oID)
{
	if (oID == getObjectID())
		return 0;

	return (m_hostileCommanders.find(oID) != -1);
}


//============================================================================
int com_Commander::receiveEvent(KR_Event &event)
 {
    char msg[MAX_SYMBOLIC_LENGHT+1];
	char fName[255], rName[255];
	int i;
	IRouteObject *riFace;
	CFVector3    node;

    switch( event.label )
    {
    case com_EV_SETHOSTILECOMMANDER:
		{
			KR_ObjectID oID;
			
			event.data.open(EDO_READ)
				.getObjectID(oID)     
			.close();

			setHostile(oID);
		}
		
		break;
		
    case com_EV_SETFRIENDLYCOMMANDER:
		{
			KR_ObjectID oID;
			
			event.data.open(EDO_READ)
				.getObjectID(oID)
			.close();
			setFriendly(oID);
		}
		
		break;
		
    case KR_WAKE_UP:
		m_memberQnty = 0;
		break;
		
    case GROUP_I_AM_DEAD:
		for(i = 0; i < m_memberQnty; ++i )
		{
			if(  m_member[i].m_id== event.source  )
			{
				for( int j = 0; j < m_member[i].m_routeCnt; ++j )
				{
					IRouteObject *route = (IRouteObject *)
						(context->queryInterface(
						m_member[i].m_route[j].id,
						IRouteObjectIID)
						);
					if(  route!=0  )
						route->DelRef();
					
				}
				
				for(; i < m_memberQnty-1; ++i )
					m_member[i] = m_member[i+1];
				
				m_memberQnty--;
				echo("Commander: group dead");
				break;
			}
		}
		break;
		
		
    case com_EV_GROUP_AREA:
		event.data.open(EDO_READ)
			.getDouble(m_xMoving)
			.getDouble(m_zMoving)
			.getInt(m_wMoving)
			.getInt(m_dMoving)
			.close();
		break;
		
    case COMMANDER_ADD_MEMBER_N:
		{
            event.data.open(EDO_READ)
				.getStr(msg,sizeof(msg))
				.close();
            KR_ObjectID oid( context->searchObject( msg ));
            if( oid.id==-1 )
            {
				echo("com_Commander: Object %s not found",msg);
				break;
            }
			
            if( m_memberQnty<MAX_MEMBER )
            {
				m_member[m_memberQnty].reset();
				m_member[m_memberQnty].m_id = oid;
				++m_memberQnty;
				event.destination = oid;
				event.source      = getObjectID();
				event.label       = com_EV_GROUP_YOUR_MY_MEMBER;				
				event.data.open(EDO_WRITE)
						 .putObjectID(event.source)                          
                      .close();

				context->sendEventNow( event );
            }
            else echo("com_Commander: Members overflow");
			
		}
		break;
		
    case com_EV_SET_ROUTE:
		{
			char memName[40];
			int  patrol;
			int  add = 0,
				isSetToStart = 0;
			
			event.data.open(EDO_READ)
				.getInt(add);
			if(  add  )
			{
				event.data
					.getStr(memName,sizeof(memName))
					.getStr(rName,sizeof(rName))
					.getStr(fName,sizeof(fName))
					.getInt(patrol)
					.getInt(isSetToStart)
					.close();
				
				for( i = 0; i < m_memberQnty; ++i )
					if(  strcmp(context->searchObject(m_member[i].m_id),
						memName)==0  )
					{
						if(  !context->isExist(rName)  )
							if(  !loadRoute( rName, fName )  )
								break;
							
							addRoute( i, rName, patrol);
							
							if(  isSetToStart  )
                                setToStart(i,event.timeStamp);
							
							break;
					}
			}
			else 
			{
				event.data.getInt(i).close();
				clrRoute(i);
			}
			
		}
		break;
		
		
case com_EV_GROUP_REACHED:
	for(i = 0; i < m_memberQnty; ++i )
	{
		GroupMember &mem = m_member[i];
		if(  mem.m_id != event.source ) 
			continue;
		
		int &curRoute = mem.m_curRoute;
		int  routeCnt = mem.m_routeCnt;
		if(  routeCnt <= 0  )
		{
			echo("Commander: member has no routes");
			continue;
		}
		if(  curRoute < 0 || curRoute >= routeCnt  )
			curRoute = 0;
		
		mem.m_patrol = mem.m_route[curRoute].patrol;
		
		if(  mem.m_patrol  )
		{
			int &curNode  = mem.m_route[curRoute].curNode;
			int  nodeCnt  = mem.m_route[curRoute].nodeCnt;
			int &dir      = mem.m_route[curRoute].dir;
			
			if(  nodeCnt <= 0  )
			{
				echo("Commander: patrol route has no nodes");
				continue;
			}
			
			switch( dir )
			{
			case 1:
				curNode++;
				if(  curNode >=  nodeCnt )
				{
					dir = 0;
					curNode--;
				}
				break;
				
			case 0:
				curNode--;
				if(  curNode < 0  )
				{
					dir = 1;
					curNode = 0;
				}
				break;
			}
		}
		else
		{
			int &curNode  = mem.m_route[curRoute].curNode;
			int  nodeCnt  = mem.m_route[curRoute].nodeCnt;
			
			if(  nodeCnt <= 0  )
			{
				echo("Commander: route has no nodes");
				continue;
			}
			
			if(  curNode < nodeCnt-1  )
				curNode++;
			else 
			{
				if(  curRoute < routeCnt-1  )
				{
					curRoute++;
					mem.m_route[curRoute].curNode = 0;
					//echo( "End of route" );
				}
			}
		}
		
		riFace =(IRouteObject *)(getContext()->queryInterface( mem.m_route[curRoute].id, IRouteObjectIID));
		if(  riFace == 0  )
		{
			echo("Commander: route interface lost");
			continue;
		}
		
		int nodeCnt = riFace->GetNodeCnt();
		if(  nodeCnt <= 0  )
		{
			echo("Commander: route has no nodes");
			continue;
		}
		
		if(  mem.m_route[curRoute].curNode < 0  )
			mem.m_route[curRoute].curNode = 0;
		else
		if(  mem.m_route[curRoute].curNode >= nodeCnt  )
			mem.m_route[curRoute].curNode = nodeCnt-1;
		
		GotoMemberPos(this,event.timeStamp,mem.m_id,node=riFace->GetNode(mem.m_route[curRoute].curNode));
		
		//echo("Group send to [%.2f, %.2f, %.2f]", node.x, node.y, node.z); 
	} 
	break;
		
    default: return 0;
    }
    return 1;
 }


bool	com_Commander::dump(PIN_SaveFile & sf)
{
		if (!sf.WriteData( (char *) m_member, sizeof(CommanderData)  ))
			return false;
		
		return true;
}

bool	com_Commander::load(PIN_SaveFile & sf)
{
		if (!sf.GetData( (char *) m_member, sizeof(CommanderData)  ))
			return false;
		
		return true;
}

void	com_Commander::loadNotify()
{
}


 /******************************
  *
  *      Commander table
  *
  ******************************/

//============================================================================
void com_CommanderTable::allocObjects( int objectQnty )
 {
    m_table = new com_Commander[objectQnty];
    if( m_table==NULL )
         m_maxObjectQnty = 0;
 }

//============================================================================
void com_CommanderTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

//============================================================================
ct_Object *com_CommanderTable::getObjectPTR( int index )
 {
    if( index<0 || index>=m_maxObjectQnty )
    {
         echo("com_CommanderTable::SearchObject: Bad index");
         return NULL;
    }
    return &(m_table[index]);
 }

namespace {

struct StableCommanderRecord
{
    std::string name;
    std::vector<std::string> members;
    std::vector<std::string> hostile;
    std::vector<std::string> friendly;

    bool operator==(const StableCommanderRecord &other) const
    {
        return name == other.name && members == other.members &&
               hostile == other.hostile && friendly == other.friendly;
    }
};

struct CommanderRoster
{
    SimulationContext *context;
    std::vector<com_Commander *> objects;
    bool valid;
};

bool CollectCommander(const KR_ObjectID object, void *user)
{
    CommanderRoster *roster = static_cast<CommanderRoster *>(user);
    ICommander *interfaceObject = static_cast<ICommander *>(
        roster->context->queryInterface(object, ICommanderIID));
    com_Commander *commander = dynamic_cast<com_Commander *>(interfaceObject);
    if (commander == NULL || roster->context->searchObject(object) == NULL)
    {
        roster->valid = false;
        return false;
    }
    roster->objects.push_back(commander);
    return true;
}

com_Commander *ResolveCommander(SimulationContext *context,
                                const KR_ObjectID &object)
{
    if (context == NULL || !context->isExist(object))
        return NULL;
    ICommander *interfaceObject = static_cast<ICommander *>(
        context->queryInterface(object, ICommanderIID));
    return dynamic_cast<com_Commander *>(interfaceObject);
}

std::string SymbolicName(SimulationContext *context,
                         const KR_ObjectID &object)
{
    const char *name = context == NULL ? NULL : context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

bool CollectStableRecords(SimulationContext *context,
                          std::vector<StableCommanderRecord> *records)
{
    if (context == NULL || records == NULL)
        return false;
    CommanderRoster roster = {context, std::vector<com_Commander *>(), true};
    const ct_ClassTableID table = g_arena.searchSeanceClassTable("Commander");
    if (table == ct_NULLID)
        return false;
    g_arena.userFind(table, CollectCommander, &roster);
    if (!roster.valid)
        return false;

    records->clear();
    for (std::size_t index = 0; index < roster.objects.size(); ++index)
    {
        com_Commander *commander = roster.objects[index];
        StableCommanderRecord record;
        record.name = SymbolicName(context, commander->getObjectID());
        if (record.name.empty() || commander->m_memberQnty < 0 ||
            commander->m_memberQnty > MAX_MEMBER)
            return false;
        for (int member = 0; member < commander->m_memberQnty; ++member)
        {
            const std::string name =
                SymbolicName(context, commander->m_member[member].m_id);
            if (name.empty())
                return false;
            record.members.push_back(name);
        }
        for (int hostile = 0;
             hostile < commander->m_hostileCommanders.getCount(); ++hostile)
        {
            const std::string name = SymbolicName(
                context, commander->m_hostileCommanders[hostile]);
            if (name.empty())
                return false;
            record.hostile.push_back(name);
        }
        for (int friendly = 0;
             friendly < commander->m_friendlyCommanders.getCount(); ++friendly)
        {
            const std::string name = SymbolicName(
                context, commander->m_friendlyCommanders[friendly]);
            if (name.empty())
                return false;
            record.friendly.push_back(name);
        }
        std::sort(record.members.begin(), record.members.end());
        std::sort(record.hostile.begin(), record.hostile.end());
        std::sort(record.friendly.begin(), record.friendly.end());
        records->push_back(record);
    }
    std::sort(records->begin(), records->end(),
              [](const StableCommanderRecord &left,
                 const StableCommanderRecord &right)
              {
                  return left.name < right.name;
              });
    return true;
}

void PutU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

bool GetU32(const std::vector<unsigned char> &bytes, std::size_t *offset,
            std::uint32_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 4)
        return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
    return true;
}

bool PutString(std::vector<unsigned char> *bytes, const std::string &value)
{
    if (bytes == NULL || value.size() > MAX_SYMBOLIC_LENGHT)
        return false;
    PutU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
}

bool GetString(const std::vector<unsigned char> &bytes, std::size_t *offset,
               std::string *value)
{
    std::uint32_t size = 0;
    if (value == NULL || !GetU32(bytes, offset, &size) ||
        size > MAX_SYMBOLIC_LENGHT || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    value->assign(reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return true;
}

bool PutStrings(std::vector<unsigned char> *bytes,
                const std::vector<std::string> &values)
{
    if (values.size() > MAX_MEMBER)
        return false;
    PutU32(bytes, static_cast<std::uint32_t>(values.size()));
    for (std::size_t index = 0; index < values.size(); ++index)
        if (!PutString(bytes, values[index]))
            return false;
    return true;
}

bool GetStrings(const std::vector<unsigned char> &bytes, std::size_t *offset,
                std::vector<std::string> *values)
{
    std::uint32_t count = 0;
    if (values == NULL || !GetU32(bytes, offset, &count) ||
        count > MAX_MEMBER)
        return false;
    values->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::string value;
        if (!GetString(bytes, offset, &value))
            return false;
        values->push_back(value);
    }
    return true;
}

bool EncodeStableRecords(const std::vector<StableCommanderRecord> &records,
                         std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > 512)
        return false;
    bytes->clear();
    PutU32(bytes, 0x52444d43u); // CMDR, little endian
    PutU32(bytes, 1u);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!PutString(bytes, records[index].name) ||
            !PutStrings(bytes, records[index].members) ||
            !PutStrings(bytes, records[index].hostile) ||
            !PutStrings(bytes, records[index].friendly))
            return false;
    return true;
}

bool DecodeStableRecords(const std::vector<unsigned char> &bytes,
                         std::vector<StableCommanderRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) || magic != 0x52444d43u ||
        version != 1u || count > 512)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableCommanderRecord record;
        if (!GetString(bytes, &offset, &record.name) ||
            !GetStrings(bytes, &offset, &record.members) ||
            !GetStrings(bytes, &offset, &record.hostile) ||
            !GetStrings(bytes, &offset, &record.friendly))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

void HashBytes(unsigned long long *hash, const void *data, std::size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        *hash ^= bytes[index];
        *hash *= 1099511628211ull;
    }
}

} // namespace

void CommanderState_Link()
{
    (void)CommanderTable.getClassTableID();
}

int CommanderState_LiveCount(SimulationContext *context)
{
    std::vector<StableCommanderRecord> records;
    return CollectStableRecords(context, &records)
        ? static_cast<int>(records.size()) : -1;
}

int CommanderState_HostileLinkCount(SimulationContext *context)
{
    CommanderRoster roster = {
        context, std::vector<com_Commander *>(), context != NULL};
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID : g_arena.searchSeanceClassTable("Commander");
    if (table == ct_NULLID)
        return -1;
    g_arena.userFind(table, CollectCommander, &roster);
    if (!roster.valid)
        return -1;
    int links = 0;
    for (std::size_t index = 0; index < roster.objects.size(); ++index)
        links += roster.objects[index]->m_hostileCommanders.getCount();
    return links;
}

int CommanderState_MemberCount(SimulationContext *context,
                               const KR_ObjectID &commander)
{
    com_Commander *resolved = ResolveCommander(context, commander);
    return resolved == NULL ? -1 : resolved->m_memberQnty;
}

bool CommanderState_HasMember(SimulationContext *context,
                              const KR_ObjectID &commander,
                              const KR_ObjectID &member)
{
    com_Commander *resolved = ResolveCommander(context, commander);
    if (resolved == NULL)
        return false;
    for (int index = 0; index < resolved->m_memberQnty; ++index)
        if (resolved->m_member[index].m_id == member)
            return true;
    return false;
}

bool CommanderState_IsHostile(SimulationContext *context,
                              const KR_ObjectID &commander,
                              const KR_ObjectID &relativeCommander)
{
    com_Commander *resolved = ResolveCommander(context, commander);
    KR_ObjectID mutableRelative = relativeCommander;
    return resolved != NULL && resolved->isHostile(mutableRelative) != 0;
}

unsigned long long CommanderState_Fingerprint(SimulationContext *context)
{
    std::vector<StableCommanderRecord> records;
    std::vector<unsigned char> bytes;
    if (!CollectStableRecords(context, &records) ||
        !EncodeStableRecords(records, &bytes))
        return 0;
    unsigned long long hash = 14695981039346656037ull;
    if (!bytes.empty())
        HashBytes(&hash, &bytes[0], bytes.size());
    return hash;
}

bool CommanderState_StableRoundTrip(SimulationContext *context)
{
    std::vector<StableCommanderRecord> before, after;
    std::vector<unsigned char> bytes;
    return CollectStableRecords(context, &before) &&
           EncodeStableRecords(before, &bytes) &&
           DecodeStableRecords(bytes, &after) && before == after;
}

