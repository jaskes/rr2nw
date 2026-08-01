/*
 * File  : C:\NW\ARENA\OBASE\Taxi\Taxi.cpp
 * Autor :
 * Ver   1.0 
 */

#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "Taxi.h"
#include "TaxiSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/sndmsg.h"
#include "message/skinmsg.h"
#include "message/Unitmsg.h"
#include "message/vehiclemsg.h"
#include "kernel/h/session.h"
#include "message/dcrossmsg.h"

#include "zav.h"

#include "h/phisics.h"
#include "h/vehicle.h"
#include "h/olevel.h"
#include "storage/h/savefile.h"

static void AnimateCallBack1(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref);
static void AnimateCallBack2(CViewObjectBaseSet *,CViewObjectBase *pBase,CViewObjectRef *ref);

namespace {

const unsigned long long kTaxiSubjectHashOffset = 14695981039346656037ull;
const unsigned long long kTaxiSubjectHashPrime = 1099511628211ull;
int g_taxiSubjectCapacity = 0;

bool FiniteTaxiDirection(const CFMatrix3x4& direction)
{
    for (int row = 0; row < 3; ++row)
    {
        const CFVector3 value = direction.Row(row);
        if (!std::isfinite(value.x) || !std::isfinite(value.y) ||
            !std::isfinite(value.z))
            return false;
    }
    const CFVector3 offset = direction.Offset();
    return std::isfinite(offset.x) && std::isfinite(offset.y) &&
           std::isfinite(offset.z);
}

bool FiniteTaxiVector(const CFVector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool NormalizeTaxiSurfaceNormal(CFVector3 *normal)
{
    if (normal == NULL || !FiniteTaxiVector(*normal))
        return false;
    const double length = Abs(*normal);
    if (!std::isfinite(length) || length <= 1.0e-8)
        return false;
    *normal = *normal * (1.0 / length);
    if (normal->y < 0.0)
        *normal = *normal * -1.0;
    // A downward placement sweep must land on a surface that can support an
    // object. Side-wall hits fall back to the terrain plane below the probe.
    return normal->y >= 0.05;
}

}


 //===========================================================================
class TaxiTable : public ct_SubjectTable
{
 private:
    Taxi *m_table;
 public:
    TaxiTable()
    {
        m_table = NULL;
        registerClass( "Taxi" );
    }
    ~TaxiTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    virtual bool       isRendering();
	virtual bool	   isAudible();
};

bool TaxiTable::isRendering()
{
   return true;
}

bool TaxiTable::isAudible()
{
	return true;
}



static TaxiTable  __classTable;
#ifndef RR2NW_TAXI_ATTRIBUTE_STATE_EXTERNAL
AttributeTableTaxi __attrTaxiTable;
#endif

 /*********************************
  *
  *   Taxi implementation
  *
  *********************************/

 //============================================================
Taxi::Taxi()
    : m_viewDynObj(m_skin)
 {
    m_surfacePlacementReady = false;
    m_surfaceSweepHit = false;
    m_surfaceTerrainFallback = false;
    m_surfaceBumpKind = BF_NONE;
    m_surfaceSweepTime = 0.0;
    m_surfaceDropDistance = 0.0;
    m_surfaceOriginClearance = 0.0;
    m_surfaceModelBottomClearance = 0.0;
    m_surfaceRequestedPosition = CFVector3(0.0, 0.0, 0.0);
    m_surfacePosition = CFVector3(0.0, 0.0, 0.0);
    m_surfaceResolvedPosition = CFVector3(0.0, 0.0, 0.0);
    m_surfaceNormal = CFVector3(0.0, 1.0, 0.0);
    m_taxiAttrID = KR_ObjectID::NUL();
    m_snd = KR_ObjectID::NUL();
    m_ctsndID = ct_NULLID;
    m_damage = 0.0;
    m_bulletCnt = 0;
    m_taxiDir.LoadIdentity();
    // ct_Subject's legacy POD leaves these frame-lifecycle fields untouched.
    // A random last-move timestamp makes visibility/land-dynamic behavior
    // depend on pooled memory and cannot be serialized deterministically.
    m_audibleThisFrame = 0;
    m_isVisible = 0;
    m_lastMoveTimeStamp = 0.0;
    m_attr = 0;//&__defaultAttr;
    m_askin = 0;
    m_wav = 0;
 }

bool Taxi::placeOnSurface(const CFVector3 &requested, double hAngle)
{
    m_surfacePlacementReady = false;
    m_surfaceSweepHit = false;
    m_surfaceTerrainFallback = false;
    m_surfaceBumpKind = BF_NONE;
    m_surfaceSweepTime = 0.0;
    m_surfaceRequestedPosition = requested;
    if (!FiniteTaxiVector(requested) || !std::isfinite(hAngle) ||
        m_attr == NULL || m_skin.Model() == NULL)
        return false;

    CViewScene *scene = ZAV_Scene();
    if (scene == NULL || scene->Order() == NULL ||
        scene->GetTerrain() == NULL)
        return false;

    const double probeLift = 4.0;
    const double probeRadius = 1.0;
    CFVector3 surface;
    CFVector3 normal;
    SBumpDef def;
    def.start = requested + CFVector3(0.0, probeLift, 0.0);
    def.vel = CFVector3(0.0, -100.0, 0.0);
    def.vel1 = def.vel;
    def.fRadius = probeRadius;
    def.nBumpFlags = BF_NONE;
    def.fMass = 1.0;
    def.fTime = 500.0;
    def.pBonus = NULL;
    def.pBumpRef = NULL;
    const bool sweepHit = scene->Order()->Bump(def) &&
        std::isfinite(def.fTime) && def.fTime >= 0.0 &&
        def.fTime <= 500.0;
    if (sweepHit)
    {
        normal = def.vel1 - def.vel;
        if (NormalizeTaxiSurfaceNormal(&normal))
        {
            const CFVector3 sphereCenter = def.start + def.vel * def.fTime;
            surface = sphereCenter - normal * probeRadius;
            m_surfaceSweepHit = FiniteTaxiVector(surface);
            if (m_surfaceSweepHit)
            {
                m_surfaceBumpKind = def.nBumpFlags;
                m_surfaceSweepTime = def.fTime;
            }
        }
    }

    if (!m_surfaceSweepHit)
    {
        double terrainY = 0.0;
        scene->GetTerrain()->GetPlane(requested, normal, terrainY);
        if (!std::isfinite(terrainY) ||
            !NormalizeTaxiSurfaceNormal(&normal))
            return false;
        surface = CFVector3(requested.x, terrainY, requested.z);
        if (!FiniteTaxiVector(surface))
            return false;
        m_surfaceTerrainFallback = true;
        m_surfaceBumpKind = BF_BUMPLAND;
    }

    // Preserve the requested heading while aligning the parked model with the
    // supporting plane. GetMatrixByAngles uses the inverse legacy convention.
    GetMatrixByAngles(m_taxiDir, normal, -hAngle - M_PI_2);
    if (!FiniteTaxiDirection(m_taxiDir))
        return false;

    CViewObjectModel *model = m_skin.Model();
    CFVector3 localBottom = model->Center();
    localBottom.y -= model->Height() * 0.5;
    const CFVector3 rotatedBottom =
        m_taxiDir * localBottom + CFVector3(0.0, m_attr->m_yOffset, 0.0);
    const double bottomDistance = rotatedBottom * normal;
    if (!FiniteTaxiVector(localBottom) || !FiniteTaxiVector(rotatedBottom) ||
        !std::isfinite(bottomDistance))
        return false;

    const CFVector3 resolved = surface - normal * bottomDistance;
    const CFVector3 modelBottom = resolved + rotatedBottom;
    const double originClearance = (resolved - surface) * normal;
    const double modelBottomClearance = (modelBottom - surface) * normal;
    const double dropDistance = requested.y - resolved.y;
    if (!FiniteTaxiVector(resolved) || !std::isfinite(originClearance) ||
        !std::isfinite(modelBottomClearance) ||
        !std::isfinite(dropDistance) ||
        std::fabs(modelBottomClearance) > 1.0e-6)
        return false;

    SetDir(m_taxiDir);
    setPosition(resolved);
    m_surfacePosition = surface;
    m_surfaceResolvedPosition = resolved;
    m_surfaceNormal = normal;
    m_surfaceDropDistance = dropDistance;
    m_surfaceOriginClearance = originClearance;
    m_surfaceModelBottomClearance = modelBottomClearance;
    m_surfacePlacementReady = true;
    return true;
}

bool Taxi::inspectDebugSpawnPlacement(
    STaxiDebugSpawnPlacement *placement) const
{
    if (placement == NULL || !m_surfacePlacementReady)
        return false;
    *placement = STaxiDebugSpawnPlacement();
    placement->ready = 1;
    placement->sweepHit = m_surfaceSweepHit ? 1 : 0;
    placement->terrainFallback = m_surfaceTerrainFallback ? 1 : 0;
    placement->bumpKind = m_surfaceBumpKind;
    placement->sweepTime = m_surfaceSweepTime;
    placement->dropDistance = m_surfaceDropDistance;
    placement->originClearance = m_surfaceOriginClearance;
    placement->modelBottomClearance = m_surfaceModelBottomClearance;
    placement->requestedPosition = m_surfaceRequestedPosition;
    placement->surfacePosition = m_surfacePosition;
    placement->resolvedPosition = m_surfaceResolvedPosition;
    placement->surfaceNormal = m_surfaceNormal;
    return true;
}

 //============================================================
Taxi::~Taxi()
 {
 }


void Taxi::setPosition ( const CFVector3 &pos )
{
	ct_Subject::setPosition (pos);
	KR_Event event;
	
	// Sound
	if(  !m_snd.isNUL() && m_audibleThisFrame)
	{					
		event.label = snd_EV_MOVE_TO;
		event.timeStamp   = Session::m_moment;
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

void Taxi::onExitAudibleZone(double ts)
{
	if (!std::isfinite(ts) || ts < 0.1)
		ts = 0.1;

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

void Taxi::onEnterAudibleZone(double ts)
{
	if (!std::isfinite(ts) || ts < 0.1)
		ts = 0.1;

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



bool Taxi::setTaxiAttr()
{
        ct_Attribute *attr = __attrTaxiTable.searchAttribute(m_taxiAttrID);
        if( attr==NULL )
        {
             const char *name = context == NULL ? NULL :
                                context->searchObject(m_taxiAttrID);
             echo( "Taxi::receiveEvent: Unknown attribute %s",
                   name == NULL ? "<unknown>" : name);
             m_attr = NULL;
             return false;
        }
        m_attr = (AttributeTaxi*)attr;

        if (m_attr->m_cacheSkin == NULL || m_attr->m_skinID.isNUL() ||
            m_attr->m_attrForVehicle.isNUL())
            return false;

        m_skin.Attach(m_attr->m_cacheSkin);
		m_skin.GetDirModify().LoadIdentity();

		m_skin.SetUserAttrib(this);
        if(  strcmp(m_attr->m_skinName,"sk.Taxi.cln_f01")==0 )
             m_skin.SetAnimationCallback(AnimateCallBack1); 
        else
        if(  strcmp(m_attr->m_skinName,"sk.Taxi.cln_f08")==0 )
			m_skin.SetAnimationCallback(AnimateCallBack2); 

	m_askin = (ISkin*)(context->queryInterface(m_attr->m_skinID, ISkinIID));
        if (m_askin == NULL)
            return false;
        if( m_askin->isAutoAnim()  )
            m_askin->skinSetAnimAuto(&m_skin);

		if (m_attr->m_buzzing)
		{
			KR_ObjectID	vehicleAttrID = context->searchObject(m_attr->m_attrForVehicleName);
			
			attr =__attrVehicleTable.searchAttribute(vehicleAttrID);
			
			if( attr==NULL )
			{
				const char *name = context == NULL ? NULL :
				                   context->searchObject(vehicleAttrID);
				echo( "Vehicle::receiveEvent: Unknown attribute %s",
				      name == NULL ? "<unknown>" : name);
			}
			else 
			{
				AttributeVehicle * vehicleAttr = (AttributeVehicle *) attr;
				
				if (!SetSoundAttr(	getObjectID(), 
					context,
					vehicleAttr->m_soundName,
					m_ctsndID, (void *)&m_wav))
					m_wav = NULL;
				
				if (m_wav)
				{
					updateSound( getObjectID(),
						context,
						m_ctsndID,
						m_wav,
						m_snd );
					
					m_audibleThisFrame = 1;
					onEnterAudibleZone(Session::m_moment);
				}		
			}	// attribute found
		} // buzzing

        return m_skin.Model() != NULL;
}

 //============================================================
int Taxi::receiveEvent( KR_Event &event )
 {

	CFVector3 pos;


    switch( event.label )
    {
    case KR_WAKE_UP:
          break;

    // FIXME	
    case t_EV_ONCOLLISION:
	    break;

    case t_EVC_MOVING:
           {
#define TOUP 4.0
#if 1
             CFVector3 from(getPosition()+CFVector3(0,TOUP,0)),
                       toDir(0,-100,0);
             double    clzTime;
             KR_ObjectID oID;

             if( 
                 checkCollision( 
                     from,    // начало движения
                     toDir,    // напрвление со скоростью
                     1,                        //g_walkAttr0.fRadius,      // радиус
                     500,                        // время для проверки
                     getObjectID(),  // кого игнорировать
                     clzTime,                   // время, через которое стукнемся
                     oID                        // объект, о который стукнемся
                   ))
             {
                   KR_Event ev;

                   ev.label = t_EVC_MOVING;
                   ev.source = getObjectID();
                   ev.destination = getObjectID();
                   ev.timeStamp = event.timeStamp + 0.06+(!m_isVisible);
                   issueEvent( ev );

                   setPosition(from + clzTime*toDir+CFVector3(0,-1.0,0)  );
             }
#endif
           }
           break;

	case EV_VEHICLE_DROP_TAXI:
		{
			CFMatrix3x4 fallbackDirection;
			fallbackDirection.LoadTransposed(g_vehicle->GetDir());
			if (!FiniteTaxiDirection(fallbackDirection))
				fallbackDirection.LoadIdentity();
			
			//KR_ObjectID oID;
			event.data.open(EDO_READ)
				.getObjectID(m_taxiAttrID)
				.getDouble(pos.x)
				.getDouble(pos.y)
				.getDouble(pos.z)
				.getDouble(m_damage)
                                .getInt   (m_bulletCnt)
				.close();
			
			if (!setTaxiAttr())
				return 0;
			
			
			SBumpDef def;
			def.start		= pos+CFVector3(0,TOUP,0);
			def.vel			= CFVector3(0,-100,0);
			def.fRadius		= 1.;// FIXME
			def.nBumpFlags	= 0;
			def.fMass		= 1;// FIXME
			def.fTime		= 10000.;
			
			if (ZAV_Scene()->Order()->Bump(def))
			{
				CFVector3 normal = def.vel1 - def.vel;
				
				double  fAngleX, fAngleY, fAngleZ;
				
				//CFMatrix3x4 m_taxiDir;

				m_taxiDir.LoadIdentity()
					.LoadTransposed(g_vehicle->GetDir())
					.RestoreEuler(CFMatrix3x4::AXIS_OY,fAngleY,CFMatrix3x4::AXIS_OX,fAngleX,CFMatrix3x4::AXIS_OZ,fAngleZ);
				
				GetMatrixByAngles( m_taxiDir, normal, -fAngleY - M_PI_2);
				if (!FiniteTaxiDirection(m_taxiDir))
					m_taxiDir = fallbackDirection;
				
				
				SetDir(m_taxiDir);
				
				pos.y -= def.fTime*100-TOUP+1.0;// put to the ground
				setPosition(pos);

			
				switch(def.nBumpFlags)
				{
				case BF_BUMPSTATIC:
					{
						KR_Event ev;
						
						ev.label = t_EVC_MOVING;
						ev.source = getObjectID();
						ev.destination = getObjectID();
						ev.timeStamp = event.timeStamp + 0.06;
						issueEvent( ev );
					}
					break;
				}
			}
		}
     break;

    case KR_SET_ATTR:
		{
			
		
			KR_ObjectID oID;
			event.data.open(EDO_READ)
				.getObjectID(m_taxiAttrID)
				.getDouble(pos.x)
				.getDouble(pos.z)
				.close();
			
			if (!setTaxiAttr())
				return 0;
			
			m_damage = m_attr->m_initialDamage;
			
			pos.y =  500;
			CFVector3 from(pos),
				toDir(0,-100,0);
			double    clzTime;
			KR_ObjectID ignored;
			
			if(  g_vehicle == 0  )
				ignored = getObjectID();
			else 
				ignored = g_vehicle->getObjectID();
			
			if( 
				checkCollision( 
				from,    // начало движения
				toDir,    // напрвление со скоростью
				1,                        //g_walkAttr0.fRadius,      // радиус
				500,                        // время для проверки
				ignored,  // кого игнорировать
				clzTime,                   // время, через которое стукнемся
				oID                        // объект, о который стукнемся
				))
			{
				
				for( int i = 1; !oID.isNUL() && i <= 512; ++i )
				{
					from = pos+CFVector3(i,500,0);
					
					if(  !checkCollision( 
						from,    // начало движения
						toDir,    // напрвление со скоростью
						1,                        //g_walkAttr0.fRadius,      // радиус
						500,                        // время для проверки
						ignored,                   // кого игнорировать
						clzTime,                   // время, через которое стукнемся
						oID                        // объект, о который стукнемся
						))
					{
						double height;
						CFVector3 normal;
						CViewScene::Current()->GetTerrain()->GetPlane(m_position,normal,height);
						pos.y = height;
						setPosition(pos);
						break;
					}
					
				}
				
				setPosition(from + clzTime*toDir +CFVector3(0,-1,0)  );
			}
			else 
			{
				double height;
				CFVector3 normal;
				CViewScene::Current()->GetTerrain()->GetPlane(m_position,normal,height);
				pos.y = height;
				setPosition(pos);
			}
			
	
			
		}
     break;

    // May retail added taxi_SET_TO_POS immediately after the January
    // t_EV_SET_ATTR_POS label. The preserved retail nw.exe resolves the
    // external constant to 0x139A (5018) and consumes ObjectID plus four
    // doubles: engine-space x/y/z and horizontal angle.
    case 0x139A:
		{
			double hAngle = 0.0;
			event.data.open(EDO_READ)
				.getObjectID(m_taxiAttrID)
				.getDouble(pos.x)
				.getDouble(pos.y)
				.getDouble(pos.z)
				.getDouble(hAngle)
				.close();
			if (!std::isfinite(pos.x) || !std::isfinite(pos.y) ||
				!std::isfinite(pos.z) || !std::isfinite(hAngle) ||
				!setTaxiAttr())
				return 0;

            m_damage = m_attr->m_initialDamage;
            if (!placeOnSurface(pos, hAngle))
                return 0;
		}
		break;

    default: return 0;
    }
    return 1;
 }

 //============================================================
void Taxi::addNotify()
 {
    ct_Subject::addNotify();
	m_taxiDir.LoadIdentity();
    // insert your code this
    m_bulletCnt = g_levelAttr.m_maxSecBulletCnt;
 }

bool Taxi::runtimeReady() const
{
    KR_ObjectID taxiAttribute = m_taxiAttrID;
    KR_ObjectID skin = m_attr == NULL ? KR_ObjectID::NUL()
                                      : m_attr->m_skinID;
    KR_ObjectID vehicleAttribute =
        m_attr == NULL ? KR_ObjectID::NUL()
                       : m_attr->m_attrForVehicle;
    KR_ObjectID sound = m_snd;
    return context != NULL && m_attr != NULL &&
           m_attr->m_cacheSkin != NULL && !taxiAttribute.isNUL() &&
           !skin.isNUL() && !vehicleAttribute.isNUL() &&
           m_skin.Model() != NULL && m_askin != NULL &&
           std::isfinite(m_damage) &&
           (!m_attr->m_buzzing || (m_wav != NULL && !sound.isNUL()));
}

 //============================================================
void Taxi::removeNotify()
 {
	 if(  !m_snd.isNUL()  )
         context->removeObject( m_snd );

    ct_Subject::removeNotify();
    // Pooled Taxi slots are reused. Never let a failed later start remove a
    // SoundObj whose numeric ObjectID has already been recycled.
    m_snd = KR_ObjectID::NUL();
    m_ctsndID = ct_NULLID;
    m_wav = NULL;
    m_askin = NULL;
    m_attr = NULL;
    m_audibleThisFrame = 0;
 }

 //============================================================
void Taxi::draw()
 {
 }

 /*************************************
  *
  *   TaxiTable implementation
  *
  *************************************/

 //============================================================
void TaxiTable::allocObjects( int objectQnty )
 {
    m_table = new (std::nothrow) Taxi[ objectQnty ];

    if(  m_table == NULL  )
    {
         m_maxObjectQnty = 0;
         g_taxiSubjectCapacity = 0;
    }
    else
         g_taxiSubjectCapacity = objectQnty;
 }

 //============================================================
void TaxiTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
    g_taxiSubjectCapacity = 0;
 }

 //============================================================
ct_Object *TaxiTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index < m_maxObjectQnty ,"TaxiTable::getObjectPTR");
    return &(m_table[ index ]);
 }

 /*************************************
  *
  *   AttributeTable implementation
  *
  *************************************/

#ifndef RR2NW_TAXI_ATTRIBUTE_STATE_EXTERNAL

 //============================================================
void AttributeTableTaxi::allocObjects( int objectQnty )
 {
    m_table = new AttributeTaxi[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void AttributeTableTaxi::freeObjects()
 {
    delete [] m_table;
    m_table         = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *AttributeTableTaxi::getObjectPTR( int index )
 {
    s_ASSERT(index>=0 && index <m_maxObjectQnty,"AttributeTable::getObjectPTR");
    return &(m_table[ index ]);
 }

#endif


 //============================================================
void Taxi::render   ( CViewDynamicList &list, double )
{
    CFMatrix3x4 &m = m_skin.GetDirModify();
    m.LoadOffset(getPosition()+CFVector3(0,m_attr->m_yOffset,0));

    m_viewDynObj.prepareToRender();
    list.Load( &m_viewDynObj );
}


 //============================================================

void Taxi::endRender( CViewScene *scene )
{
    scene->RemoveLandDynamic( &m_viewDynObj );
}

 //============================================================
CFVector3  Taxi::realPosition() {  return getPosition();  }


 /*************************************
  *
  *   AttributeBird implementation
  *
  *************************************/

 //============================================================


#ifndef RR2NW_TAXI_ATTRIBUTE_STATE_EXTERNAL
void AttributeTaxi::update(double ts)
{

   /*
    * Search and get skin
    */
    KR_ObjectID skinID =  context->searchObject( m_skinName );
    s_ASSERT( !skinID.isNUL(), "AttributeTaxi::update" );
    
    m_skinID = skinID;

    KR_Event event;
    event.timeStamp   = ts;
    event.label       = sk_EV_QUERY_MODEL_PTR;
    event.destination = skinID;
    context->sendEventNow( event );
    s_ASSERT(event.label==sk_EV_QUERY_MODEL_PTR_OK,"AttributeTaxi::update");
    event.data.open(EDO_READ)
                .get(&m_cacheSkin,sizeof(void*))
              .close();

    m_attrForVehicle = context->searchObject(m_attrForVehicleName);

    m_cacheCorpseTable	= g_arena.searchSeanceClassTable( "Corpse" );
    m_cacheCorpseAttr	= g_arena.getAttributeIndex( 
                                    g_arena.searchSeanceClassTable( "CorpseAttr" ),
                                    context->searchObject(m_corpseAttrName) 
                                 );

}

#endif




 /*********************************
  *
  *   Taxi interfaces
  *
  *********************************/

    // IDynamicObject interface

 //============================================================
CFVector3  Taxi::getPos      ()
{
    return getPosition();
}

 //============================================================
double     Taxi::getHAngle   ()
{
    return M_PI/2; // FIXME
}

 //============================================================
CFVector3  Taxi::getUpVector ()
{
    return CFVector3(0,1,0); // FIXME
}

 //============================================================
CFVector3  Taxi::getCenter   () // Относительно 0 объекта
{
    return m_skin.Model()->Center();
}

 //============================================================
double     Taxi::getRadius   () // Относительно центра
{
    return m_skin.Model()->Radius();
}

 //============================================================
double     Taxi::getRadius0  () // Относительно 0 объекта
{
    return m_skin.Model()->Radius0();
}

 //============================================================
CFVector3  Taxi::getMoveDir  () // Направление движения
{
    return CFVector3(0,1,0);
}

 //============================================================
double     Taxi::getMoveSpeed() // Скорость
{
    return 0;
}

 //============================================================
void      Taxi::getMatrix   ( CFMatrix3x4 &m )
{
    m.LoadIdentity();
    //m.TranslateL( getPosition() );
}

    // IUnit interface
 //============================================================
double Taxi::getPower() // Сила юнита 0..10
{
    return 0.0;
}

 //============================================================
int    Taxi::isFriend( const KR_ObjectID & )
{
    return 1;
}

 //============================================================
double Taxi::getDamage() // Целостность от 0..1
{
    return m_damage;
}

 //============================================================
KR_ObjectID Taxi::getAttributeForVehicle()
{
    return m_attr->m_attrForVehicle;
}

 //============================================================
CFVector3  Taxi::taxiPos()
{
    return getPosition();
}

double Taxi::desireShoot()
{
    return 0;
}

void *Taxi::queryInterface( int interNum )
 {
    switch( interNum )
    {
    case IUnknownIID:       return (KR_Object*)this;
    case IDynamicObjectIID: return (IDynamicObject*)this;
    case IUnitIID:          return (IUnit*)this;
    case ITaxiIID:          return (ITaxi*)this;
    }

    return 0;
 }


void Taxi::setDamage  ( double d, const CFVector3 &, double ts, KR_ObjectID )
{
   m_damage -= d;
   echo( "damage=%lg", m_damage);
   if(  m_damage <= 0  )
   {
		createCorpse(	getPosition() , 
						ts,
						getObjectID(),
						m_attr->m_cacheCorpseTable, 
						m_attr->m_cacheCorpseAttr);
		
		context->removeObject(getObjectID() );
   }
}
 
KR_ObjectID Taxi::getCommander()
{
    return KR_ObjectID::NUL();
}



static
void AnimateCallBack1(CViewObjectBaseSet *,CViewObjectBase */*pBase*/,CViewObjectRef */*ref*/)
 {
    //(void)pBase;
 }

static
void AnimateCallBack2(CViewObjectBaseSet *,CViewObjectBase */*pBase*/,CViewObjectRef */*ref*/)
 {
    //(void)pBase;
 }

void Taxi::setCommander(KR_ObjectID)
{
    echo("Taxi::setCommander don't support");
}

void  Taxi::taxiSetBulletCnt(int cnt)
{
    m_bulletCnt = cnt;
}

int   Taxi::taxiGetBulletCnt()
{
    return m_bulletCnt;
}


bool	Taxi::dump(PIN_SaveFile & sf)
{
		if (!ct_Subject::dump(sf))
			return false;

		if (!sf.WriteData( (char *) & m_taxiAttrID, sizeof(TaxiData)  ))
			return false;

		return true;
}

bool	Taxi::load(PIN_SaveFile & sf)
{
		if (!ct_Subject::load(sf))
			return false;

		if (!sf.GetData( (char *) & m_taxiAttrID, sizeof(TaxiData)  ))
			return false;
		
		return true;
}

void	Taxi::loadNotify()
{
	ct_Subject::loadNotify();
	setTaxiAttr();
	SetDir(m_taxiDir);
	CFVector3 pos = getPosition();
	setPosition(pos);
}

namespace {

struct TaxiSubjectRecord
{
    KR_ObjectID object;
    Taxi *taxi;
    std::string taxiAttribute;
    std::string vehicleAttribute;
};

struct TaxiSubjectCollector
{
    SimulationContext *context;
    std::vector<TaxiSubjectRecord> records;
    bool valid;
};

void TaxiSubjectHashBytes(unsigned long long &hash,
                          const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= kTaxiSubjectHashPrime;
    }
}

void TaxiSubjectHashString(unsigned long long &hash, const char *value)
{
    if (value == NULL)
        value = "";
    TaxiSubjectHashBytes(hash, value,
                         static_cast<int>(std::strlen(value)) + 1);
}

void TaxiSubjectHashVector(unsigned long long &hash,
                           const CFVector3 &value)
{
    TaxiSubjectHashBytes(hash, &value.x, sizeof(value.x));
    TaxiSubjectHashBytes(hash, &value.y, sizeof(value.y));
    TaxiSubjectHashBytes(hash, &value.z, sizeof(value.z));
}

void TaxiSubjectHashMatrix(unsigned long long &hash,
                           const CFMatrix3x4 &value)
{
    TaxiSubjectHashVector(hash, value.Row(0));
    TaxiSubjectHashVector(hash, value.Row(1));
    TaxiSubjectHashVector(hash, value.Row(2));
    TaxiSubjectHashVector(hash, value.Offset());
}

Taxi *ResolveTaxi(SimulationContext *context, const KR_ObjectID &object)
{
    KR_ObjectID candidate = object;
    if (context == NULL || candidate.isNUL() ||
        !context->isExist(object))
        return NULL;
    ITaxi *taxiInterface = static_cast<ITaxi *>(
        context->queryInterface(object, ITaxiIID));
    return taxiInterface == NULL
               ? NULL
               : dynamic_cast<Taxi *>(taxiInterface);
}

bool CollectTaxiSubject(const KR_ObjectID object, void *user)
{
    TaxiSubjectCollector *collector =
        static_cast<TaxiSubjectCollector *>(user);
    Taxi *taxi = ResolveTaxi(collector->context, object);
    const char *taxiAttribute =
        taxi == NULL ? NULL : collector->context->searchObject(
                                  taxi->taxiAttributeID());
    const char *vehicleAttribute =
        taxi == NULL ? NULL : collector->context->searchObject(
                                  taxi->getAttributeForVehicle());
    if (taxi == NULL || taxiAttribute == NULL || vehicleAttribute == NULL ||
        !taxi->runtimeReady())
    {
        collector->valid = false;
        return false;
    }
    TaxiSubjectRecord record = {
        object, taxi, taxiAttribute, vehicleAttribute};
    collector->records.push_back(record);
    return true;
}

bool TaxiSubjectRecordLess(const TaxiSubjectRecord &left,
                           const TaxiSubjectRecord &right)
{
    if (left.taxiAttribute != right.taxiAttribute)
        return left.taxiAttribute < right.taxiAttribute;
    const CFVector3 leftPosition = left.taxi->taxiPos();
    const CFVector3 rightPosition = right.taxi->taxiPos();
    if (leftPosition.x != rightPosition.x)
        return leftPosition.x < rightPosition.x;
    if (leftPosition.y != rightPosition.y)
        return leftPosition.y < rightPosition.y;
    if (leftPosition.z != rightPosition.z)
        return leftPosition.z < rightPosition.z;
    return left.vehicleAttribute < right.vehicleAttribute;
}

bool CollectTaxiSubjects(SimulationContext *context,
                         TaxiSubjectCollector &collector)
{
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Taxi");
    if (context == NULL || g_arena.getContext() != context ||
        table == ct_NULLID || g_taxiSubjectCapacity <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    g_arena.userFind(table, CollectTaxiSubject, &collector);
    if (!collector.valid)
        return false;
    std::sort(collector.records.begin(), collector.records.end(),
              TaxiSubjectRecordLess);
    return true;
}

bool CaptureTaxiAttribute(const KR_ObjectID object, void *user)
{
    KR_ObjectID *attribute = static_cast<KR_ObjectID *>(user);
    if (attribute->isNUL())
        *attribute = object;
    return false;
}

struct TaxiDebugCatalogCollector
{
    SimulationContext *context;
    std::vector<STaxiDebugVehicleType> *catalog;
    bool valid;
};

bool CollectTaxiDebugVehicleType(const KR_ObjectID object, void *user)
{
    TaxiDebugCatalogCollector *collector =
        static_cast<TaxiDebugCatalogCollector *>(user);
    AttributeTaxi *attribute = static_cast<AttributeTaxi *>(
        __attrTaxiTable.searchAttribute(object));
    const char *taxiName = collector->context->searchObject(object);
    const char *vehicleName = attribute == NULL
                                  ? NULL
                                  : collector->context->searchObject(
                                        attribute->m_attrForVehicle);
    if (attribute == NULL || taxiName == NULL || taxiName[0] == '\0' ||
        attribute->m_attrForVehicle.isNUL() || vehicleName == NULL ||
        vehicleName[0] == '\0' ||
        __attrVehicleTable.searchAttribute(
            attribute->m_attrForVehicle) == NULL)
    {
        collector->valid = false;
        return false;
    }
    STaxiDebugVehicleType type;
    type.taxiAttribute = taxiName;
    type.vehicleAttribute = vehicleName;
    collector->catalog->push_back(type);
    return true;
}

bool SendTaxiStart(Taxi *taxi, const KR_ObjectID &attribute,
                   const CFVector3 &position, double angle,
                   double timeStamp)
{
    if (taxi == NULL)
        return false;
    KR_Event event;
    event.label = 0x139A;
    event.destination = taxi->getObjectID();
    event.source = taxi->getObjectID();
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(attribute)
              .putDouble(position.x)
              .putDouble(position.y)
              .putDouble(position.z)
              .putDouble(angle)
              .close();
    return taxi->receiveEvent(event) == 1;
}

bool TaxiSubjectNearlyEqual(double left, double right,
                            double tolerance = 1.0e-7)
{
    return std::fabs(left - right) <= tolerance;
}

bool TaxiSubjectNearlyEqual(const CFVector3 &left,
                            const CFVector3 &right,
                            double tolerance = 1.0e-7)
{
    return TaxiSubjectNearlyEqual(left.x, right.x, tolerance) &&
           TaxiSubjectNearlyEqual(left.y, right.y, tolerance) &&
           TaxiSubjectNearlyEqual(left.z, right.z, tolerance);
}

bool TaxiSubjectNearlyEqual(const CFMatrix3x4 &left,
                            const CFMatrix3x4 &right,
                            double tolerance = 1.0e-7)
{
    return TaxiSubjectNearlyEqual(left.Row(0), right.Row(0), tolerance) &&
           TaxiSubjectNearlyEqual(left.Row(1), right.Row(1), tolerance) &&
           TaxiSubjectNearlyEqual(left.Row(2), right.Row(2), tolerance) &&
           TaxiSubjectNearlyEqual(left.Offset(), right.Offset(), tolerance);
}

struct TaxiVehicleSnapshot
{
    KR_ObjectID attribute;
    CFVector3 position;
    CFVector3 subjectPosition;
    CFVector3 speed;
    CFMatrix3x4 direction;
    double damage;
    double lastTime;
    int bullets;
    int takingTaxi;
};

bool CaptureTaxiVehicle(Vehicle *vehicle, TaxiVehicleSnapshot *snapshot)
{
    if (vehicle == NULL || snapshot == NULL ||
        vehicle->getContext() == NULL || vehicle->m_vehicleAttrID.isNUL())
        return false;
    snapshot->attribute = vehicle->m_vehicleAttrID;
    snapshot->position = vehicle->Pos();
    snapshot->subjectPosition = vehicle->getPosition();
    snapshot->speed = vehicle->Speed();
    snapshot->direction = vehicle->GetDir();
    snapshot->damage = vehicle->m_damage;
    snapshot->lastTime = vehicle->m_lastTime;
    snapshot->bullets = vehicle->m_secBulletCnt;
    snapshot->takingTaxi = Vehicle::m_isTakingTaxiNow;
    return std::isfinite(snapshot->damage) &&
           std::isfinite(snapshot->lastTime) &&
           TaxiSubjectNearlyEqual(snapshot->speed,
                                  CFVector3(0.0, 0.0, 0.0));
}

bool TaxiVehicleMatches(Vehicle *vehicle,
                        const TaxiVehicleSnapshot &snapshot)
{
    return vehicle != NULL &&
           vehicle->m_vehicleAttrID == snapshot.attribute &&
           TaxiSubjectNearlyEqual(vehicle->Pos(), snapshot.position) &&
           TaxiSubjectNearlyEqual(vehicle->getPosition(),
                                  snapshot.subjectPosition) &&
           TaxiSubjectNearlyEqual(vehicle->Speed(), snapshot.speed) &&
           TaxiSubjectNearlyEqual(vehicle->GetDir(), snapshot.direction) &&
           TaxiSubjectNearlyEqual(vehicle->m_damage, snapshot.damage) &&
           TaxiSubjectNearlyEqual(vehicle->m_lastTime, snapshot.lastTime) &&
           vehicle->m_secBulletCnt == snapshot.bullets &&
           Vehicle::m_isTakingTaxiNow == snapshot.takingTaxi;
}

bool RestoreTaxiVehicle(Vehicle *vehicle,
                        const TaxiVehicleSnapshot &snapshot,
                        double timeStamp)
{
    if (vehicle == NULL)
        return false;
    KR_Event event;
    event.label = KR_SET_ATTR;
    event.destination = vehicle->getObjectID();
    event.source = vehicle->getObjectID();
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(snapshot.attribute)
              .close();
    vehicle->setAttr(event);
    if (vehicle->m_vehicleAttrID != snapshot.attribute)
        return false;
    vehicle->Restart();
    vehicle->SetDir(snapshot.direction);
    vehicle->SetPos(snapshot.position);
    vehicle->Stop();
    vehicle->setPosition(snapshot.subjectPosition);
    vehicle->m_damage = snapshot.damage;
    vehicle->m_lastTime = snapshot.lastTime;
    vehicle->m_secBulletCnt = snapshot.bullets;
    Vehicle::m_isTakingTaxiNow = snapshot.takingTaxi;
    return TaxiVehicleMatches(vehicle, snapshot);
}

}  // namespace

void TaxiSubjectState_Link()
{
}

bool TaxiSubjectState_TableReady(SimulationContext *context,
                                 int expectedCapacity)
{
    return context != NULL && g_arena.getContext() == context &&
           expectedCapacity > 0 &&
           g_taxiSubjectCapacity == expectedCapacity &&
           g_arena.searchSeanceClassTable("Taxi") != ct_NULLID;
}

int TaxiSubjectState_Capacity()
{
    return g_arena.searchSeanceClassTable("Taxi") == ct_NULLID
               ? 0
               : g_taxiSubjectCapacity;
}

int TaxiSubjectState_LiveCount()
{
    TaxiSubjectCollector collector = {};
    SimulationContext *context = g_arena.getContext();
    if (!CollectTaxiSubjects(context, collector))
        return 0;
    return static_cast<int>(collector.records.size());
}

int TaxiSubjectState_SoundCount()
{
    TaxiSubjectCollector collector = {};
    if (!CollectTaxiSubjects(g_arena.getContext(), collector))
        return 0;
    int count = 0;
    for (std::size_t index = 0; index < collector.records.size(); ++index)
        if (!collector.records[index].taxi->m_snd.isNUL())
            ++count;
    return count;
}

bool TaxiSubjectState_AllReady(SimulationContext *context)
{
    TaxiSubjectCollector collector = {};
    return CollectTaxiSubjects(context, collector);
}

unsigned long long TaxiSubjectState_Fingerprint(
    SimulationContext *context)
{
    TaxiSubjectCollector collector = {};
    if (!CollectTaxiSubjects(context, collector))
        return 0;
    unsigned long long hash = kTaxiSubjectHashOffset;
    TaxiSubjectHashString(hash, "Taxi");
    TaxiSubjectHashBytes(hash, &g_taxiSubjectCapacity,
                         sizeof(g_taxiSubjectCapacity));
    const int count = static_cast<int>(collector.records.size());
    TaxiSubjectHashBytes(hash, &count, sizeof(count));
    for (std::size_t index = 0; index < collector.records.size(); ++index)
    {
        Taxi *taxi = collector.records[index].taxi;
        TaxiSubjectHashString(
            hash, collector.records[index].taxiAttribute.c_str());
        TaxiSubjectHashString(
            hash, collector.records[index].vehicleAttribute.c_str());
        TaxiSubjectHashVector(hash, taxi->taxiPos());
        TaxiSubjectHashMatrix(hash, taxi->GetDir());
        const double damage = taxi->getDamage();
        const int bullets = taxi->taxiGetBulletCnt();
        const int sound = taxi->m_snd.isNUL() ? 0 : 1;
        TaxiSubjectHashBytes(hash, &damage, sizeof(damage));
        TaxiSubjectHashBytes(hash, &bullets, sizeof(bullets));
        TaxiSubjectHashBytes(hash, &sound, sizeof(sound));
    }
    return hash;
}

bool TaxiSubjectState_IsKnownRetailRoster(SimulationContext *context)
{
    const int capacity = TaxiSubjectState_Capacity();
    const int count = TaxiSubjectState_LiveCount();
    if (!TaxiSubjectState_AllReady(context) ||
        TaxiSubjectState_Fingerprint(context) == 0)
        return false;
    return (capacity == 100 &&
            (count == 20 || count == 35 || count == 38 || count == 66)) ||
           (capacity == 150 && (count == 28 || count == 93)) ||
           (capacity == 80 && count == 0) ||
           (capacity == 20 && count == 1) ||
           (capacity == 4 && count == 2);
}

bool TaxiSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    STaxiSubjectLifecycleProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Taxi");
    const int baselineCount = TaxiSubjectState_LiveCount();
    const int baselineSounds = TaxiSubjectState_SoundCount();
    const unsigned long long baselineFingerprint =
        TaxiSubjectState_Fingerprint(context);
    if (context == NULL || table == ct_NULLID || baselineFingerprint == 0 ||
        baselineCount >= TaxiSubjectState_Capacity())
        return false;

    KR_ObjectID attribute = KR_ObjectID::NUL();
    __attrTaxiTable.userFind(CaptureTaxiAttribute, &attribute);
    if (attribute.isNUL())
        return false;

    KR_ObjectID invalid =
        g_arena.newObject(table, "Taxi.Subject.Invalid.Probe");
    Taxi *invalidTaxi = ResolveTaxi(context, invalid);
    const bool invalidRejected =
        invalidTaxi != NULL &&
        !SendTaxiStart(invalidTaxi, KR_ObjectID::NUL(),
                       CFVector3(32.0, 500.0, -32.0), 0.0, timeStamp) &&
        !invalidTaxi->runtimeReady();
    if (!invalid.isNUL())
        context->removeObject(invalid);
    if (!invalidRejected || TaxiSubjectState_LiveCount() != baselineCount ||
        TaxiSubjectState_SoundCount() != baselineSounds)
        return false;
    summary->invalidStarts = 1;

    KR_ObjectID valid =
        g_arena.newObject(table, "Taxi.Subject.Valid.Probe");
    Taxi *validTaxi = ResolveTaxi(context, valid);
    const bool started =
        validTaxi != NULL &&
        SendTaxiStart(validTaxi, attribute,
                      CFVector3(32.0, 500.0, -32.0), 0.0, timeStamp) &&
        validTaxi->runtimeReady();
    if (started)
    {
        summary->validStarts = 1;
        summary->renderReady = validTaxi->m_skin.Model() != NULL ? 1 : 0;
        summary->soundReady =
            !validTaxi->m_attr->m_buzzing || !validTaxi->m_snd.isNUL()
                ? 1
                : 0;
    }
    if (!valid.isNUL())
        context->removeObject(valid);
    if (TaxiSubjectState_LiveCount() == baselineCount &&
        TaxiSubjectState_SoundCount() == baselineSounds &&
        TaxiSubjectState_Fingerprint(context) == baselineFingerprint)
        summary->rollbacks = 2;
    return started && summary->invalidStarts == 1 &&
           summary->validStarts == 1 && summary->renderReady == 1 &&
           summary->soundReady == 1 && summary->rollbacks == 2;
}

bool TaxiSubjectState_ProbeVehicleTransition(
    SimulationContext *context, const KR_ObjectID &vehicleObject,
    double timeStamp, STaxiVehicleTransitionProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || g_arena.getContext() != context ||
        !std::isfinite(timeStamp))
        return false;

    Vehicle *vehicle = static_cast<Vehicle *>(
        context->queryInterface(vehicleObject, IVehicleIID));
    TaxiVehicleSnapshot vehicleBefore = {};
    if (!CaptureTaxiVehicle(vehicle, &vehicleBefore))
        return false;

    if (!vehicle->tryTakeTaxi(KR_ObjectID::NUL(), timeStamp, false) &&
        TaxiVehicleMatches(vehicle, vehicleBefore))
        summary->invalidTargets = 1;
    else
        return false;

    KR_Event invalidAttribute;
    invalidAttribute.label = KR_SET_ATTR;
    invalidAttribute.destination = vehicleObject;
    invalidAttribute.source = vehicleObject;
    invalidAttribute.timeStamp = timeStamp;
    invalidAttribute.data.open(EDO_WRITE)
                         .putObjectID(KR_ObjectID::NUL())
                         .close();
    vehicle->setAttr(invalidAttribute);
    if (!TaxiVehicleMatches(vehicle, vehicleBefore))
        return false;

    TaxiSubjectCollector collector = {};
    if (!CollectTaxiSubjects(context, collector))
        return false;
    const int baselineCount = static_cast<int>(collector.records.size());
    const int baselineSounds = TaxiSubjectState_SoundCount();
    const unsigned long long baselineFingerprint =
        TaxiSubjectState_Fingerprint(context);
    if (baselineFingerprint == 0)
        return false;
    if (collector.records.empty())
    {
        summary->rollbacks = 1;
        return summary->invalidTargets == 1 &&
               TaxiVehicleMatches(vehicle, vehicleBefore);
    }

    summary->availableTaxis = 1;
    Taxi *taxi = collector.records.front().taxi;
    const KR_ObjectID taxiObject = collector.records.front().object;
    const KR_ObjectID taxiAttribute = taxi->taxiAttributeID();
    const KR_ObjectID vehicleAttribute = taxi->getAttributeForVehicle();
    const CFVector3 taxiPosition = taxi->taxiPos();
    const CFMatrix3x4 taxiDirection = taxi->GetDir();
    const CFMatrix3x4 taxiStoredDirection = taxi->m_taxiDir;
    const double taxiDamage = taxi->getDamage();
    const int taxiBullets = taxi->taxiGetBulletCnt();
    AttributeVehicle *targetAttribute = static_cast<AttributeVehicle *>(
        __attrVehicleTable.searchAttribute(vehicleAttribute));
    if (targetAttribute == NULL)
        return false;

    CFMatrix3x4 expectedDirection;
    expectedDirection.LoadTransposed(taxiDirection);
    const CFVector3 expectedPosition =
        taxiPosition + CFVector3(0.0, targetAttribute->m_bornY, 0.0);
    const bool transitioned =
        vehicle->tryTakeTaxi(taxiObject, timeStamp, false);
    if (transitioned)
        summary->transitions = 1;
    if (transitioned && vehicle->m_vehicleAttrID == vehicleAttribute)
        summary->attributeTransfers = 1;
    if (transitioned &&
        TaxiSubjectNearlyEqual(vehicle->GetDir(), expectedDirection) &&
        TaxiSubjectNearlyEqual(vehicle->Pos(), expectedPosition))
        summary->poseTransfers = 1;
    if (transitioned &&
        TaxiSubjectNearlyEqual(vehicle->m_damage, taxiDamage) &&
        vehicle->m_secBulletCnt == taxiBullets)
        summary->payloadTransfers = 1;
    if (transitioned && !context->isExist(taxiObject) &&
        TaxiSubjectState_LiveCount() == baselineCount - 1)
        summary->removedTaxis = 1;

    const bool vehicleRestored =
        RestoreTaxiVehicle(vehicle, vehicleBefore, timeStamp);
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Taxi");
    KR_ObjectID replacement =
        g_arena.newObject(table, "Taxi.VehicleTransition.Rollback");
    Taxi *replacementTaxi = ResolveTaxi(context, replacement);
    bool taxiRestored =
        replacementTaxi != NULL &&
        SendTaxiStart(replacementTaxi, taxiAttribute, taxiPosition,
                      0.0, timeStamp);
    if (taxiRestored)
    {
        replacementTaxi->m_damage = taxiDamage;
        replacementTaxi->m_bulletCnt = taxiBullets;
        replacementTaxi->m_taxiDir = taxiStoredDirection;
        replacementTaxi->SetDir(taxiDirection);
        replacementTaxi->setPosition(taxiPosition);
        taxiRestored = replacementTaxi->runtimeReady();
    }

    if (vehicleRestored && taxiRestored &&
        TaxiSubjectState_LiveCount() == baselineCount &&
        TaxiSubjectState_SoundCount() == baselineSounds &&
        TaxiSubjectState_Fingerprint(context) == baselineFingerprint)
        summary->rollbacks = 1;

    return summary->invalidTargets == 1 &&
           summary->transitions == 1 &&
           summary->attributeTransfers == 1 &&
           summary->poseTransfers == 1 &&
           summary->payloadTransfers == 1 &&
           summary->removedTaxis == 1 &&
           summary->rollbacks == 1;
}

bool TaxiSubjectState_InspectVehicleProximity(
    SimulationContext *context, const KR_ObjectID &vehicleObject,
    STaxiVehicleProximityState *state)
{
    if (state == NULL)
        return false;
    std::memset(state, 0, sizeof(*state));
    state->nearestTaxi = KR_ObjectID::NUL();
    state->nearestDistance = 1.0e10;
    state->activationDistance = 20.0;
    Vehicle *vehicle = context == NULL ? NULL : static_cast<Vehicle *>(
        context->queryInterface(vehicleObject, IVehicleIID));
    TaxiSubjectCollector collector = {};
    if (vehicle == NULL || !CollectTaxiSubjects(context, collector))
        return false;
    state->availableTaxis = static_cast<int>(collector.records.size());
    const CFVector3 position = vehicle->Pos();
    for (std::size_t index = 0; index < collector.records.size(); ++index)
    {
        const double distance = Abs(
            position - collector.records[index].taxi->taxiPos());
        if (!std::isfinite(distance))
            return false;
        if (distance < state->activationDistance)
            ++state->nearbyTaxis;
        if (distance < state->nearestDistance)
        {
            state->nearestDistance = distance;
            state->nearestTaxi = collector.records[index].object;
        }
    }
    if (collector.records.empty())
        state->nearestDistance = -1.0;
    return true;
}

KR_ObjectID TaxiSubjectState_FirstObject(SimulationContext *context)
{
    TaxiSubjectCollector collector = {};
    return CollectTaxiSubjects(context, collector) &&
                   !collector.records.empty()
               ? collector.records.front().object
               : KR_ObjectID::NUL();
}

KR_ObjectID TaxiSubjectState_FirstPanelVehicleObject(
    SimulationContext *context)
{
    TaxiSubjectCollector collector = {};
    if (!CollectTaxiSubjects(context, collector))
        return KR_ObjectID::NUL();
    for (std::size_t index = 0; index < collector.records.size(); ++index)
    {
        AttributeVehicle *attribute = static_cast<AttributeVehicle *>(
            __attrVehicleTable.searchAttribute(
                collector.records[index].taxi->getAttributeForVehicle()));
        if (attribute != NULL && attribute->m_panel != NULL &&
            attribute->m_panel->IsReady())
            return collector.records[index].object;
    }
    return KR_ObjectID::NUL();
}

bool TaxiSubjectState_DebugVehicleCatalog(
    SimulationContext *context,
    std::vector<STaxiDebugVehicleType> *catalog,
    std::string *failure)
{
    if (catalog == NULL || failure == NULL)
        return false;
    catalog->clear();
    failure->clear();
    if (context == NULL || g_arena.getContext() != context ||
        g_arena.searchSeanceClassTable("Taxi") == ct_NULLID)
    {
        *failure = "Taxi debug catalog requires an active Arena seance";
        return false;
    }
    TaxiDebugCatalogCollector collector = {context, catalog, true};
    __attrTaxiTable.userFind(CollectTaxiDebugVehicleType, &collector);
    if (!collector.valid)
    {
        catalog->clear();
        *failure = "a Level-local TaxiAttr has no valid VehicleAttr target";
        return false;
    }
    std::sort(catalog->begin(), catalog->end(),
              [](const STaxiDebugVehicleType &left,
                 const STaxiDebugVehicleType &right)
              {
                  if (left.taxiAttribute != right.taxiAttribute)
                      return left.taxiAttribute < right.taxiAttribute;
                  return left.vehicleAttribute < right.vehicleAttribute;
              });
    catalog->erase(
        std::unique(catalog->begin(), catalog->end(),
                    [](const STaxiDebugVehicleType &left,
                       const STaxiDebugVehicleType &right)
                    {
                        return left.taxiAttribute == right.taxiAttribute &&
                               left.vehicleAttribute == right.vehicleAttribute;
                    }),
        catalog->end());
    if (catalog->empty())
    {
        *failure = "the active Level defines no spawnable TaxiAttr";
        return false;
    }
    return true;
}

bool TaxiSubjectState_DebugSpawn(
    SimulationContext *context, const char *taxiAttribute,
    const char *objectName, const CFVector3 &position,
    double angle, double timeStamp, KR_ObjectID *spawned,
    STaxiDebugSpawnPlacement *placement, std::string *failure)
{
    if (spawned == NULL || placement == NULL || failure == NULL)
        return false;
    *spawned = KR_ObjectID::NUL();
    *placement = STaxiDebugSpawnPlacement();
    failure->clear();
    const ct_ClassTableID table = g_arena.searchSeanceClassTable("Taxi");
    if (context == NULL || g_arena.getContext() != context ||
        table == ct_NULLID || taxiAttribute == NULL ||
        taxiAttribute[0] == '\0' || objectName == NULL ||
        objectName[0] == '\0' || !std::isfinite(position.x) ||
        !std::isfinite(position.y) || !std::isfinite(position.z) ||
        !std::isfinite(angle) || !std::isfinite(timeStamp) ||
        timeStamp < 0.1)
    {
        *failure = "Taxi debug spawn arguments are invalid";
        return false;
    }
    KR_ObjectID existing = context->searchObject(objectName);
    if (!existing.isNUL())
    {
        *failure = "Taxi debug object name already exists";
        return false;
    }
    KR_ObjectID attribute = context->searchObject(taxiAttribute);
    if (attribute.isNUL() ||
        __attrTaxiTable.searchAttribute(attribute) == NULL)
    {
        *failure = "requested TaxiAttr is not active in this Level";
        return false;
    }
    if (TaxiSubjectState_LiveCount() >= TaxiSubjectState_Capacity())
    {
        *failure = "Taxi subject table is full";
        return false;
    }
    KR_ObjectID object = g_arena.newObject(table, objectName);
    Taxi *taxi = ResolveTaxi(context, object);
    const bool started = taxi != NULL &&
        SendTaxiStart(taxi, attribute, position, angle, timeStamp) &&
        taxi->runtimeReady() &&
        taxi->inspectDebugSpawnPlacement(placement) &&
        placement->ready != 0 &&
        TaxiSubjectNearlyEqual(taxi->taxiPos(),
                               placement->resolvedPosition) &&
        std::fabs(placement->modelBottomClearance) <= 1.0e-6;
    if (!started)
    {
        if (!object.isNUL() && context->isExist(object))
            context->removeObject(object);
        *placement = STaxiDebugSpawnPlacement();
        *failure = "real Taxi subject rejected grounded surface placement";
        return false;
    }
    *spawned = object;
    return true;
}

bool TaxiSubjectState_DebugTakeVehicle(
    SimulationContext *context, const KR_ObjectID &vehicleObject,
    const KR_ObjectID &taxiObject, double timeStamp,
    std::string *failure)
{
    if (failure == NULL)
        return false;
    failure->clear();
    Vehicle *vehicle = context == NULL ? NULL : static_cast<Vehicle *>(
        context->queryInterface(vehicleObject, IVehicleIID));
    Taxi *taxi = ResolveTaxi(context, taxiObject);
    if (context == NULL || vehicle == NULL || taxi == NULL ||
        !taxi->runtimeReady() || !std::isfinite(timeStamp) ||
        timeStamp < 0.1)
    {
        *failure = "Taxi debug enter requires a live Vehicle and Taxi";
        return false;
    }
    if (!vehicle->tryTakeTaxi(taxiObject, timeStamp, true))
    {
        *failure = "retail Vehicle::tryTakeTaxi rejected the spawned Taxi";
        return false;
    }
    return true;
}

bool TaxiSubjectState_DebugPlacementDrift(
    SimulationContext *context, const char *objectName,
    const CFVector3 &expectedPosition, double *drift)
{
    if (context == NULL || objectName == NULL || objectName[0] == '\0' ||
        drift == NULL || !FiniteTaxiVector(expectedPosition))
        return false;
    KR_ObjectID object = context->searchObject(objectName);
    Taxi *taxi = ResolveTaxi(context, object);
    if (taxi == NULL || !taxi->runtimeReady())
        return false;
    *drift = Abs(taxi->taxiPos() - expectedPosition);
    return std::isfinite(*drift);
}

/* End of file C:\NW\ARENA\OBASE\Taxi\Taxi.cpp */
