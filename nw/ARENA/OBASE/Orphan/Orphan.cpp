/*
* File  : C:\NW\ARENA\OBASE\Orphan\Orphan.cpp
* Autor :
* Ver   1.0 
*/
#include "Orphan.h"
#include "OrphanSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "kernel/h/session.h"
#include "message/skinmsg.h"
#include "message/Unitmsg.h"
#include "message/vehiclemsg.h"
#include "zav.h"
#include "h/phisics.h"
#include "h/vehicle.h"
#include "../taxi/taxi.h"
#include "message/sndmsg.h"
#include "storage/h/savefile.h"

#ifndef RR2NW_ORPHAN_ATTRIBUTE_STATE_EXTERNAL
#include "OrphanAttributeState.inl"
#endif

namespace {

const unsigned long long kOrphanSubjectHashOffset =
    14695981039346656037ull;
const unsigned long long kOrphanSubjectHashPrime = 1099511628211ull;
int g_orphanSubjectCapacity = 0;
SOrphanSubjectRuntimeTelemetry g_orphanTelemetry = {};

}


static void AnimateCallBack1(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref);
static void AnimateCallBack2(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref);


//===========================================================================
class OrphanTable : public ct_SubjectTable
{
private:
    Orphan *m_table;
public:
    OrphanTable()
    {
        m_table = NULL;
        registerClass( "Orphan" );
    }
    ~OrphanTable()
    {
        delete [] m_table;
        m_table = NULL;
    }
	
    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    virtual bool       isRendering();
    virtual bool       isAudible();	
};

bool OrphanTable::isRendering()
{
	return true;
}

bool OrphanTable::isAudible()
{
	return true;
}


static OrphanTable  __classTable;


/*********************************
*
*   Orphan implementation
*
*********************************/



//============================================================
Orphan::Orphan()
: m_viewDynObj(m_skin)
{
    m_orphanAttrID = KR_ObjectID::NUL();
    m_snd = KR_ObjectID::NUL();
    m_ctsndID = ct_NULLID;
    m_speed = CFVector3(0.0, 0.0, 0.0);
    m_damage = 0.0;
    m_lastEventTime = 0.0;
    m_dir.LoadIdentity();
    m_lastMovePos = CFVector3(0.0, 0.0, 0.0);
    m_lastMoveDeltaT = 0.0;
    m_lastMoveTimeStamp = 0.0;
    m_taxiAttr = NULL;
	m_attr = NULL;
    m_wav = NULL;
}

//============================================================
Orphan::~Orphan()
{
}


void Orphan::KillMe(CFVector3 & newPos, double ts)
{
	if (!runtimeReady())
	{
		context->removeObject(getObjectID());
		return;
	}
	++g_orphanTelemetry.impacts;
	createExplosion(newPos, ts,
					getObjectID(),
					m_attr->m_cacheExplosionTable,
					m_attr->m_cacheExplAttr
					);
	++g_orphanTelemetry.explosionStarts;

	double clzTime;
	KR_ObjectID oID;
	CFVector3 dSpeed(0,-9.8,0);


	if(	checkCollision( 
			newPos ,				// начало движения
			dSpeed,    // напрвление со скоростью
				1,          // g_walkAttr0.fRadius,      // радиус
				1,          // время для проверки
				getObjectID(),  // кого игнорировать
				clzTime,                   // время, через которое стукнемся
				oID                        // объект, о который стукнемся
				) && clzTime < m_attr->m_collisionT)	
	{
		// мы на земле, создать труп
		// если мы на земле а не на динамическом объекте
		
		if (oID.isNUL())
			createCorpse(	getPosition() + dSpeed * clzTime,
							ts,
							getObjectID(),
							m_taxiAttr->m_cacheCorpseTable, 
							m_taxiAttr->m_cacheCorpseAttr);
	}

			
	context->removeObject( getObjectID() );
}



bool Orphan::setOrphanAttr()
{
	ct_Attribute *attr = __attrTaxiTable.searchAttribute(m_orphanAttrID);
	m_taxiAttr = NULL;
	m_attr = NULL;
	m_wav = NULL;
	m_snd = KR_ObjectID::NUL();
	m_ctsndID = ct_NULLID;

	if( attr==NULL )
	{
		echo( "Orphan::receiveEvent: Unknown attribute %s",
		context == NULL ? "<no context>" :
		context->searchObject(m_orphanAttrID));
		return false;
	}
	AttributeTaxi *taxiAttr = static_cast<AttributeTaxi *>(attr);
	if (taxiAttr->m_cacheSkin == NULL || taxiAttr->m_skinID.isNUL() ||
		taxiAttr->m_attrForVehicle.isNUL())
		return false;

	AttributeOrphan *orphanAttr = static_cast<AttributeOrphan *>(
		__attrOrphanTable.searchAttribute(
			context->searchObject("Orphan.Attr.Default")));
	ISkin *skin = static_cast<ISkin *>(
		context->queryInterface(taxiAttr->m_skinID, ISkinIID));
	AttributeVehicle *vehicleAttr = static_cast<AttributeVehicle *>(
		__attrVehicleTable.searchAttribute(taxiAttr->m_attrForVehicle));
	if (orphanAttr == NULL || skin == NULL ||
		vehicleAttr == NULL ||
		!OrphanAttributeState_RuntimeReady(context))
		return false;

	m_taxiAttr = taxiAttr;
	m_attr = orphanAttr;
	
	
	// Sound stuff, we can determine sound scheme for the orphan from
	// appropriate vehicle engine sound, which is accessable through
	// m_attrForVehicleName attribute of taxi
	
	
	if (!SetSoundAttr(getObjectID(), context, vehicleAttr->m_soundName,
		m_ctsndID, (void *)&m_wav))
		m_wav = NULL;
	if (m_wav)
	{
		updateSound(getObjectID(), context, m_ctsndID, m_wav, m_snd);
		m_audibleThisFrame = 1;
		onEnterAudibleZone(m_lastEventTime);
		++g_orphanTelemetry.soundStarts;
	}

	m_skin.Attach(m_taxiAttr->m_cacheSkin);
	m_skin.GetDirModify().LoadIdentity();
	
	m_skin.SetUserAttrib(this);
	if(  strcmp(m_taxiAttr->m_skinName,"sk.Taxi.cln_f01")==0 )
		m_skin.SetAnimationCallback(AnimateCallBack1); 
	else                       
	if(  strcmp(m_taxiAttr->m_skinName,"sk.Taxi.cln_f08")==0 )
		m_skin.SetAnimationCallback(AnimateCallBack2); 

        if( skin->isAutoAnim()  )
            skin->skinSetAnimAuto(&m_skin);
	return runtimeReady();
}


//============================================================
int Orphan::receiveEvent( KR_Event &event )
{
    switch( event.label )
    {
    case t_EVC_MOVING:
		{
			if (!runtimeReady() || !std::isfinite(event.timeStamp) ||
				event.timeStamp <= m_lastEventTime)
				return 0;
			++g_orphanTelemetry.moveEvents;
			double dt = event.timeStamp - m_lastEventTime;
			m_lastMovePos    = getPosition();
                        m_lastMoveDeltaT = dt;
                        m_lastMoveTimeStamp = event.timeStamp;

			double clzTime;
			KR_ObjectID oID;
			
			CFVector3 newPos(getPosition()+m_speed * dt);
                        m_speed.y += -5*dt;
			if( 
				checkCollision( 
				newPos ,    // начало движения
				m_speed,    // напрвление со скоростью
				1,          // g_walkAttr0.fRadius,      // радиус
				1,          // время для проверки
				getObjectID(),  // кого игнорировать
				clzTime,                   // время, через которое стукнемся
				oID                        // объект, о который стукнемся
				) && clzTime < m_attr->m_collisionT)
			     KillMe(newPos, event.timeStamp);
			else
			{
				m_lastEventTime = event.timeStamp;
				//event.timeStamp += m_attr->m_deltaT;

				double haze = CViewFigure::HazeMax();
				double delay = m_attr->m_deltaT;
				if (g_vehicle != NULL && std::isfinite(haze) && haze > 0.001)
					delay *= (Abs(g_vehicle->getPos()-getPosition())+
						      haze*0.2)/haze;
				if (!std::isfinite(delay) || delay <= 0.0)
					delay = m_attr->m_deltaT;
				event.timeStamp += delay;
				
				issueEvent(event);					
				setPosition(newPos);
				
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


				if (m_damage < m_attr->m_smokeDamage )
				{					
					createSmoke( getPosition(),
						         getObjectID(),
							     m_lastEventTime,
								 m_attr->m_smokeTableID,
								m_attr->m_smokeAttrID
								);
					++g_orphanTelemetry.smokeStarts;
				}

				
			}
		}
		break;

    case KR_WAKE_UP:
		break;
		
		// FIXME	
    case t_EV_ONCOLLISION:
		if (!runtimeReady())
			return 0;
		++g_orphanTelemetry.impacts;
		createExplosion(getPosition(), event.timeStamp,
			getObjectID(),
			m_attr->m_cacheExplosionTable,
			m_attr->m_cacheExplAttr
			);
		++g_orphanTelemetry.explosionStarts;
		
		context->removeObject( getObjectID() );
		break;
		
		
    case EV_VEHICLE_DROP_TAXI:
		{
			CFVector3 pos;
			int bcnt;
			const int expectedSize = static_cast<int>(
				sizeof(KR_ObjectID) + sizeof(double) * 4 + sizeof(int));
			if (event.data.size() != expectedSize ||
				!std::isfinite(event.timeStamp) || event.timeStamp < 0.1)
			{
				++g_orphanTelemetry.rejectedDrops;
				context->removeObject(getObjectID());
				return 0;
			}

			event.data.open(EDO_READ)
				.getObjectID(m_orphanAttrID)
				.getDouble(pos.x)
				.getDouble(pos.y)
				.getDouble(pos.z)
				.getDouble(m_damage)
				.getInt(bcnt)
				.close();
			if (m_orphanAttrID.isNUL() || g_vehicle == NULL ||
				!std::isfinite(pos.x) ||
				!std::isfinite(pos.y) || !std::isfinite(pos.z) ||
				!std::isfinite(m_damage))
			{
				++g_orphanTelemetry.rejectedDrops;
				context->removeObject(getObjectID());
				return 0;
			}
			
			m_lastMovePos    = pos;
                        m_lastMoveDeltaT = 0;
                        m_lastMoveTimeStamp = event.timeStamp;
			m_lastEventTime = event.timeStamp;

			if (!setOrphanAttr())
			{
				++g_orphanTelemetry.rejectedDrops;
				context->removeObject(getObjectID());
				return 0;
			}
			++g_orphanTelemetry.acceptedDrops;


			//CFMatrix3x4 newDir;
			m_dir.LoadIdentity().LoadTransposed(g_vehicle->GetDir());
			
			SetDir(m_dir);
			setPosition(pos);
				
				event.label			= t_EVC_MOVING;
				event.source		= getObjectID();
				event.destination	= getObjectID();
				event.timeStamp	    = m_lastEventTime + m_attr->m_deltaT;
				
				issueEvent(event);
				
				m_speed = g_vehicle->Speed();
				

				
				if (Abs2(m_speed) < m_attr->m_minSpeed)
				{
					IDynamicObject *dobj=g_vehicle;
					CFMatrix3x4 m;
					dobj->getMatrix(m);

					m_speed = - m.Column(2) * 30;
					m_speed.y = -10;
				}
			const int liveObjects = OrphanSubjectState_LiveCount();
			if (liveObjects > g_orphanTelemetry.peakLiveObjects)
				g_orphanTelemetry.peakLiveObjects = liveObjects;

		}
		break;
		
    case KR_SET_ATTR: ASSERT(0);
		break;
		
    default: return 0;
    }
    return 1;
 }
 
 //============================================================
 void Orphan::addNotify()
 {
	 ct_Subject::addNotify();
	 m_orphanAttrID = KR_ObjectID::NUL();
	 m_snd = KR_ObjectID::NUL();
	 m_ctsndID = ct_NULLID;
	 m_speed = CFVector3(0.0, 0.0, 0.0);
	 m_damage = 0.0;
	 m_lastEventTime = 0.0;
	 m_dir.LoadIdentity();
	 m_lastMovePos = CFVector3(0.0, 0.0, 0.0);
	 m_lastMoveDeltaT = 0.0;
	 m_lastMoveTimeStamp = 0.0;
	 m_taxiAttr = NULL;
	 m_attr = NULL;
	 m_wav = NULL;
	 m_audibleThisFrame = 0;
 }
 
 //============================================================
 void Orphan::removeNotify()
 {
	 if(  !m_snd.isNUL()  )
         context->removeObject( m_snd );
	 m_snd = KR_ObjectID::NUL();
	 m_taxiAttr = NULL;
	 m_attr = NULL;
	 m_wav = NULL;
	 
	 ct_Subject::removeNotify();
 }
 
 //============================================================
 void Orphan::draw()
 {
 }
 
 /*************************************
 *
 *   OrphanTable implementation
 *
 *************************************/
 
 //============================================================
 void OrphanTable::allocObjects( int objectQnty )
 {
	 m_table = new (std::nothrow) Orphan[ objectQnty ];

	 if(  m_table == NULL  )
         m_maxObjectQnty = 0;
	 g_orphanSubjectCapacity = m_table == NULL ? 0 : objectQnty;
	 std::memset(&g_orphanTelemetry, 0, sizeof(g_orphanTelemetry));
 }
 
 //============================================================
 void OrphanTable::freeObjects()
 {
	 delete [] m_table;
	 m_table = NULL;
	 m_maxObjectQnty = 0;
	 g_orphanSubjectCapacity = 0;
	 std::memset(&g_orphanTelemetry, 0, sizeof(g_orphanTelemetry));
 }
 
 //============================================================
 ct_Object *OrphanTable::getObjectPTR( int index )
 {
	 s_ASSERT( index >= 0 && index < m_maxObjectQnty ,"OrphanTable::getObjectPTR");
	 return &(m_table[ index ]);
 }
 
 
 
 //============================================================
 void Orphan::render   ( CViewDynamicList &list, double ts)
 {
	 if (!runtimeReady())
		 return;
	 CFMatrix3x4 &m = m_skin.GetDirModify();
         CFVector3 p(getPosition());
	 if (!std::isfinite(ts) || !std::isfinite(p.x) ||
		 !std::isfinite(p.y) || !std::isfinite(p.z))
		 return;

         if(  m_lastMoveDeltaT >0.001 && m_lastMoveDeltaT < 0.2 )
	 {
		 double ratio = (ts-m_lastMoveTimeStamp)/m_lastMoveDeltaT;
		 if (!std::isfinite(ratio))
			 return;
		 if (ratio < 0.0) ratio = 0.0;
		 if (ratio > 1.0) ratio = 1.0;
		 p += (getPosition()-m_lastMovePos)*ratio;
	 }
	 if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
		 !std::isfinite(p.z))
		 return;
	 m.LoadOffset(p);

	 m_viewDynObj.prepareToRender();
	 list.Load( &m_viewDynObj );
	 ++g_orphanTelemetry.renderFrames;
 }
 
 
 //============================================================
 
 void Orphan::endRender( CViewScene *scene )
 {
	 scene->RemoveLandDynamic( &m_viewDynObj );
 }
 
 //============================================================
 CFVector3  Orphan::realPosition() {  return getPosition();  }
 
 
 
 
 /*********************************
 *
 *   Orphan interfaces
 *
 *********************************/
 
 // IDynamicObject interface
 
 //============================================================
 CFVector3  Orphan::getPos      ()
 {
	 return getPosition();
 }
 
 //============================================================
 double     Orphan::getHAngle   ()
 {
	 return M_PI/2; // FIXME
 }
 
 //============================================================
 CFVector3  Orphan::getUpVector ()
 {
	 return CFVector3(0,1,0); // FIXME
 }
 
 //============================================================
 CFVector3  Orphan::getCenter   () // Относительно 0 объекта
 {
	 return m_skin.Model()->Center();
 }

 //============================================================
 double     Orphan::getRadius   () // Относительно центра
 {
	 return m_skin.Model()->Radius();
 }
 
 //============================================================
 double     Orphan::getRadius0  () // Относительно 0 объекта
 {
	 return m_skin.Model()->Radius0();
 }
 
 //============================================================
 CFVector3  Orphan::getMoveDir  () // Направление движения
 {
	 return CFVector3(0,1,0);
 }
 
 //============================================================
 double     Orphan::getMoveSpeed() // Скорость
 {
	 return 0;
 }
 
 //============================================================
 void      Orphan::getMatrix   ( CFMatrix3x4 &m )
 {
	 m = GetDir();
	 m.LoadOffset(getPosition());
 }
 
 // IUnit interface
 
 //============================================================
 
 void *Orphan::queryInterface( int interNum )
 {
	 switch( interNum )
	 {
	 case IUnknownIID:       return (KR_Object*)this;
	 case IDynamicObjectIID: return (IDynamicObject*)this;
	 }
	 return 0;
 }
 
 
 //============================================================


 void Orphan::onExitAudibleZone(double ts) 
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
 
 void Orphan::onEnterAudibleZone(double ts) 
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
 
 
 static
	 void AnimateCallBack1(CViewObjectBaseSet *,CViewObjectBase */*pBase*/,CViewObjectRef */*ref*/)
 {
 }
 
 static
	 void AnimateCallBack2(CViewObjectBaseSet *,CViewObjectBase */*pBase*/,CViewObjectRef */*ref*/)
 {
 }
 
 
 void Orphan::onHide(double)
 {
	 context->removeObject( getObjectID() );
 }


bool	Orphan::dump(PIN_SaveFile & sf)
{
		if (!ct_Subject::dump(sf))
			return false;

		if (!sf.WriteData( (char *) & m_orphanAttrID, sizeof(OrphanData)  ))
			return false;

		return true;
}

bool	Orphan::load(PIN_SaveFile & sf)
{
		if (!ct_Subject::load(sf))
			return false;


		if (!sf.GetData( (char *) & m_orphanAttrID, sizeof(OrphanData)  ))
			return false;
		
		return true;
}

void	Orphan::loadNotify()
{
	ct_Subject::loadNotify();
	if (setOrphanAttr())
		SetDir(m_dir);
}

bool Orphan::runtimeReady() const
{
	return context != NULL && m_taxiAttr != NULL && m_attr != NULL &&
		m_taxiAttr->m_cacheSkin != NULL &&
		!m_taxiAttr->m_skinID.isNUL() &&
		!m_taxiAttr->m_attrForVehicle.isNUL() &&
		m_skin.Model() != NULL &&
		OrphanAttributeState_RuntimeReady(context);
}

namespace {

struct OrphanSubjectRecord
{
	KR_ObjectID object;
	Orphan *orphan;
	std::string taxiAttribute;
};

struct OrphanSubjectCollector
{
	SimulationContext *context;
	std::vector<OrphanSubjectRecord> records;
	bool valid;
};

void OrphanSubjectHashBytes(unsigned long long &hash,
							const void *data, int size)
{
	const unsigned char *bytes = static_cast<const unsigned char *>(data);
	for (int index = 0; index < size; ++index)
	{
		hash ^= bytes[index];
		hash *= kOrphanSubjectHashPrime;
	}
}

void OrphanSubjectHashString(unsigned long long &hash, const char *value)
{
	if (value == NULL)
		value = "";
	OrphanSubjectHashBytes(hash, value,
		static_cast<int>(std::strlen(value)) + 1);
}

void OrphanSubjectHashVector(unsigned long long &hash,
							 const CFVector3 &value)
{
	OrphanSubjectHashBytes(hash, &value.x, sizeof(value.x));
	OrphanSubjectHashBytes(hash, &value.y, sizeof(value.y));
	OrphanSubjectHashBytes(hash, &value.z, sizeof(value.z));
}

Orphan *ResolveOrphan(SimulationContext *context, KR_ObjectID object)
{
	if (context == NULL || object.isNUL() || !context->isExist(object))
		return NULL;
	IDynamicObject *dynamicObject = static_cast<IDynamicObject *>(
		context->queryInterface(object, IDynamicObjectIID));
	return dynamicObject == NULL ? NULL :
		dynamic_cast<Orphan *>(dynamicObject);
}

bool CountOrphanSubject(const KR_ObjectID, void *user)
{
	++(*static_cast<int *>(user));
	return true;
}

bool CollectOrphanSubject(const KR_ObjectID object, void *user)
{
	OrphanSubjectCollector *collector =
		static_cast<OrphanSubjectCollector *>(user);
	Orphan *orphan = ResolveOrphan(collector->context, object);
	const char *attribute = orphan == NULL ? NULL :
		collector->context->searchObject(orphan->m_orphanAttrID);
	if (orphan == NULL || attribute == NULL || !orphan->runtimeReady())
	{
		collector->valid = false;
		return false;
	}
	OrphanSubjectRecord record = {object, orphan, attribute};
	collector->records.push_back(record);
	return true;
}

bool OrphanSubjectRecordLess(const OrphanSubjectRecord &left,
							 const OrphanSubjectRecord &right)
{
	if (left.taxiAttribute != right.taxiAttribute)
		return left.taxiAttribute < right.taxiAttribute;
	const CFVector3 leftPosition = left.orphan->getPosition();
	const CFVector3 rightPosition = right.orphan->getPosition();
	if (leftPosition.x != rightPosition.x)
		return leftPosition.x < rightPosition.x;
	if (leftPosition.y != rightPosition.y)
		return leftPosition.y < rightPosition.y;
	return leftPosition.z < rightPosition.z;
}

bool CollectOrphanSubjects(SimulationContext *context,
						   OrphanSubjectCollector &collector)
{
	const ct_ClassTableID table =
		g_arena.searchSeanceClassTable("Orphan");
	if (context == NULL || g_arena.getContext() != context ||
		table == ct_NULLID || g_orphanSubjectCapacity <= 0)
		return false;
	collector.context = context;
	collector.valid = true;
	g_arena.userFind(table, CollectOrphanSubject, &collector);
	if (!collector.valid)
		return false;
	std::sort(collector.records.begin(), collector.records.end(),
		OrphanSubjectRecordLess);
	return true;
}

}  // namespace

void OrphanSubjectState_Link()
{
}

bool OrphanSubjectState_CreateTable(SimulationContext *context,
								int capacity)
{
	if (context == NULL || capacity <= 0 ||
		g_arena.getContext() != context)
		return false;
	const ct_ClassTableID table =
		g_arena.addClassTable("Orphan", capacity);
	return table != ct_NULLID &&
		g_arena.searchSeanceClassTable("Orphan") == table &&
		g_orphanSubjectCapacity == capacity;
}

bool OrphanSubjectState_TableReady(SimulationContext *context,
							   int expectedCapacity)
{
	return context != NULL && g_arena.getContext() == context &&
		expectedCapacity > 0 &&
		g_orphanSubjectCapacity == expectedCapacity &&
		g_arena.searchSeanceClassTable("Orphan") != ct_NULLID;
}

int OrphanSubjectState_Capacity()
{
	return g_arena.searchSeanceClassTable("Orphan") == ct_NULLID ? 0 :
		g_orphanSubjectCapacity;
}

int OrphanSubjectState_LiveCount()
{
	const ct_ClassTableID table =
		g_arena.searchSeanceClassTable("Orphan");
	if (table == ct_NULLID || g_orphanSubjectCapacity <= 0)
		return 0;
	int count = 0;
	g_arena.userFind(table, CountOrphanSubject, &count);
	return count;
}

bool OrphanSubjectState_AllReady(SimulationContext *context)
{
	OrphanSubjectCollector collector = {};
	return CollectOrphanSubjects(context, collector);
}

unsigned long long OrphanSubjectState_Fingerprint(
	SimulationContext *context)
{
	OrphanSubjectCollector collector = {};
	if (!CollectOrphanSubjects(context, collector))
		return 0;
	unsigned long long hash = kOrphanSubjectHashOffset;
	OrphanSubjectHashString(hash, "Orphan");
	OrphanSubjectHashBytes(hash, &g_orphanSubjectCapacity,
		sizeof(g_orphanSubjectCapacity));
	const int count = static_cast<int>(collector.records.size());
	OrphanSubjectHashBytes(hash, &count, sizeof(count));
	for (std::size_t index = 0; index < collector.records.size(); ++index)
	{
		Orphan *orphan = collector.records[index].orphan;
		OrphanSubjectHashString(hash,
			collector.records[index].taxiAttribute.c_str());
		OrphanSubjectHashVector(hash, orphan->getPosition());
		OrphanSubjectHashVector(hash, orphan->m_speed);
		OrphanSubjectHashBytes(hash, &orphan->m_damage,
			sizeof(orphan->m_damage));
		const int sound = orphan->m_snd.isNUL() ? 0 : 1;
		OrphanSubjectHashBytes(hash, &sound, sizeof(sound));
	}
	return hash;
}

bool OrphanSubjectState_RuntimeTelemetry(
	SimulationContext *context,
	SOrphanSubjectRuntimeTelemetry *telemetry)
{
	if (telemetry == NULL || context == NULL ||
		g_arena.getContext() != context ||
		!OrphanSubjectState_TableReady(context,
			g_orphanSubjectCapacity))
		return false;
	*telemetry = g_orphanTelemetry;
	return true;
}

KR_ObjectID OrphanSubjectState_FirstObject(SimulationContext *context)
{
	OrphanSubjectCollector collector = {};
	return CollectOrphanSubjects(context, collector) &&
		!collector.records.empty() ? collector.records.front().object :
		KR_ObjectID::NUL();
}

 /* End of file C:\NW\ARENA\OBASE\Orphan\Orphan.cpp */
