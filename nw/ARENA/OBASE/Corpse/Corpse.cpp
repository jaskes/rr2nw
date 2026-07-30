/*
 * File  : C:\nw\ARENA\OBASE\Corpse\Corpse.cpp
 * Autor :
 * Ver   1.0 
 */


#include "Corpse.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/corpsemsg.h"
#include "message/skinmsg.h"
#include "h/phisics.h"
#include "message/fountmsg.h"
#include "storage/h/savefile.h"

#ifndef RR2NW_CORPSE_ATTRIBUTE_STATE_EXTERNAL
#include "CorpseAttributeState.inl"
#endif

strg_SUBJECT_TABLE_IMPLEMENTATION(Corpse,true)
static CorpseTable __corpse;


 /*********************************
  *
  *   Corpse implementation
  *
  *********************************/

 //============================================================
strg_CONSTRUCTOR_DYNVIEW(Corpse)
 {
	resetState();
 }

 //============================================================
Corpse::~Corpse()
 {
 }


void Corpse::DestroyMe()
{
     if(  context == 0  )
          return;

	context->removeObject(m_smoke);
	context->removeObject(m_fire);
	context->removeObject(getObjectID() );
}

void Corpse::resetState()
{
	m_attributeIndex = -1;
	m_smoke = KR_ObjectID::NUL();
	m_fire = KR_ObjectID::NUL();
	m_mustDieNow = 0;
	m_attr = 0;
	m_dynamicPublished = false;
	m_isVisible = false;
}

void Corpse::onHide(double )
{
	if (m_mustDieNow)
		DestroyMe();
}

 //============================================================
int Corpse::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case KR_WAKE_UP:
            break;
	case CORPSE_TIME_TO_DIE:	
			m_mustDieNow = 1;
			if (!m_isVisible)
				DestroyMe();
			break;
	case CORPSE_START_ROTTING:
		{
			//int			index;
			double		tm;
			
			KR_ObjectID oID;
			KR_ObjectID parentID;

			CFVector3 pos;

			event.data.open(EDO_READ)
						.getObjectID(parentID)
                        .getInt	  (m_attributeIndex)						
                        .getDouble(pos.x)
                        .getDouble(pos.y)
                        .getDouble(pos.z)
                      .close();


			if( checkCollision( 
                     pos /*+ CFVector3(0,5,0)*/ ,    // начало движения
                     CFVector3(0,-1,0),    // напрвление со скоростью
                     1, // радиус
                     50,  // время для проверки
                     parentID,  // кого игнорировать
                     tm, // время, через которое стукнемся
                     oID      // объект, о который стукнемся
                   ) )
			{				
				pos.y -= tm;
			}
			else
			{
				context->removeObject(getObjectID() );
				break;
			}
			
		
			m_mustDieNow = 0;

			__attrCorpseTable.setAttribute(m_attributeIndex,(ct_Attribute *&)m_attr);
			m_skin.Attach(m_attr->m_cacheSkin);

			pos += CFVector3(m_attr->m_corpseOffsetX,m_attr->m_corpseOffsetY,m_attr->m_corpseOffsetZ);
			setPosition(pos);

			m_smoke = KR_ObjectID::NUL();
			m_fire  = KR_ObjectID::NUL();

			if (m_attr->m_isSmoking)
			{
					m_smoke = g_arena.newObject(m_attr->m_smokerTableID,"Smoker.Corpse");
                                     
			        if(  m_smoke == KR_ObjectID::NUL()  )
					{
						echo("Warning! Smoker table overflow");						
						break;
					}
		
					event.label     = fou_EVCMD_START;	 					
					event.source    = g_arena.getObjectID();
					event.destination = m_smoke;
					event.data.open(EDO_WRITE)
                        .putObjectID(m_attr->m_smokerAttrID)
                        .putDouble(pos.x + m_attr->m_fireOffsetX) 
                        .putDouble(pos.y + m_attr->m_fireOffsetY)
                        .putDouble(pos.z + m_attr->m_fireOffsetZ)
                    .close();
			        g_arena.getContext()->sendEventNow(event);
			}

			if (m_attr->m_isBurning)
			{
					m_fire = g_arena.newObject(m_attr->m_smokerTableID,"Smoker.Corpse");
                                     
			        if(  m_fire == KR_ObjectID::NUL()  )
					{
						echo("Warning! Smoker table overflow");
						break;
					}
		
					event.label     = fou_EVCMD_START;	 					
					event.source    = g_arena.getObjectID();
					event.destination = m_fire;
					event.data.open(EDO_WRITE)
                        .putObjectID(m_attr->m_fireAttrID)
                        .putDouble(pos.x + m_attr->m_fireOffsetX)
                        .putDouble(pos.y + m_attr->m_fireOffsetY)
                        .putDouble(pos.z + m_attr->m_fireOffsetZ)
                    .close();
			        g_arena.getContext()->sendEventNow(event);
			}


			event.label			= CORPSE_TIME_TO_DIE;
			event.source		= getObjectID();
			event.destination	= getObjectID();
			event.timeStamp		+= m_attr->m_minLifeTime;
			issueEvent(event);

		}	
		break;

    default: return 0;
    }
    return 1;
 }



void Corpse::render(CViewDynamicList &list, double ts)
{
	if (m_attr == NULL || m_dynamicPublished)
		return;
	onRender(ts);
	m_viewDynObj.prepareToRender();
	list.Load(&m_viewDynObj);
	m_dynamicPublished = true;
}

void Corpse::endRender(CViewScene *scene)
{
	if (m_dynamicPublished && scene != NULL)
		scene->RemoveLandDynamic(&m_viewDynObj);
	m_dynamicPublished = false;
}

 //============================================================
void Corpse::onRender (double)
{
    CFMatrix3x4 &m = m_skin.GetDirModify();
    m.LoadIdentity();
    //m.RotateOxL(m_rotateOx);
    //m.RotateOyL(m_rotateOy);
    m.TranslateL( getPosition() );   
}


 //============================================================
void Corpse::addNotify()
 {
    ct_Subject::addNotify();
	resetState();
 }

 //============================================================
void Corpse::removeNotify()
 {
	if (context != NULL)
		while (context->removeEvent(CORPSE_TIME_TO_DIE, getObjectID()) == 1)
		{
		}
	if (m_dynamicPublished)
	{
		CViewScene *scene = CViewScene::Current();
		if (scene != NULL)
			scene->RemoveLandDynamic(&m_viewDynObj);
		m_dynamicPublished = false;
	}
    ct_Subject::removeNotify();
	resetState();
 }



CFVector3 Corpse::realPosition() { return getPosition() ; }

// ----- interface IDynamicObject
void      *Corpse::queryInterface( int interNum )
{
    switch( interNum )
    {
		case IUnknownIID:       return (KR_Object*)this;    
    }

    return 0;
}



bool	Corpse::dump(PIN_SaveFile & sf)
{
		if (!ct_Subject::dump(sf))
			return false;

		if (!sf.WriteData( (char *) & m_attributeIndex, sizeof(CorpseData)  ))
			return false;

		return true;
}

bool	Corpse::load(PIN_SaveFile & sf)
{
		if (!ct_Subject::load(sf))
			return false;


		if (!sf.GetData( (char *) & m_attributeIndex, sizeof(CorpseData)  ))
			return false;
		
		return true;
}

void	Corpse::loadNotify()
{
	ct_Subject::loadNotify(); 
	__attrCorpseTable.setAttribute(m_attributeIndex,(ct_Attribute *&)m_attr);
	m_skin.Attach(m_attr->m_cacheSkin);
}

/* End of file C:\nw\ARENA\OBASE\Corpse\Corpse.cpp */
