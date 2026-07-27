/*

 * File  : C:\NW\ARENA\OBASE\Lamp\Lamp.cpp
 * Autor :
 * Ver   1.0 
 */
#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "graph.h"
#include "Lamp.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/fountmsg.h"
#include "message/Lampmsg.h"
#include "h/light.h"
#include "h/phisics.h"
#include "h/cachesmoke.h"
#include "kernel/h/session.h"
#include "storage/h/savefile.h"


#define RGB_TO_LIST(col)  ((col)>>16), ((col)>>8)&255, (col)&255

#ifndef RR2NW_LAMP_ATTRIBUTE_STATE_EXTERNAL
#include "LampAttributeState.inl"
#endif

 //===========================================================================
class LampTable : public ct_SubjectTable
{
 private:
    Lamp *m_table;
 public:
    LampTable()
    {
        m_table = NULL;
        registerClass( "Lamp" );
    }
    ~LampTable()
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
bool LampTable::isRendering()
{
     return true;
}

static LampTable  __classTable;
 /*********************************
  *
  *   Lamp implementation
  *
  *********************************/

 //============================================================
Lamp::Lamp()
 {
    m_attr = 0;
 }

 //============================================================
Lamp::~Lamp()
 {
 }

 //============================================================
void Lamp::onView(double ts)
{
    if(  !m_sentMove  && m_attr->m_lightMode != 0) 
    {
          KR_Event event;
          event.label       = lmp_EV_MOVE;
          event.source      = getObjectID();
          event.destination = getObjectID();
          event.timeStamp   = ts;
          issueEvent( event );
          m_sentMove = true;
    }
}

CFVector3 Lamp::realPosition()
{
     return m_lampPosition;
}

void Lamp::RandomChange(KR_Event &event)
{
	if(  m_isVisible  )
	{
		double ts = event.timeStamp;
		
		event.timeStamp += context->rnd_f(
			m_attr->m_createIncMin,
			m_attr->m_createIncMax
			);
		issueEvent( event );
		m_sentMove = true;
		
		m_brightness +=  context->rnd_f(m_attr->m_lightBrightStep) 
			- m_attr->m_lightBrightStep/2;
		if(  m_brightness < m_attr->m_minLightBright  )
			m_brightness = m_attr->m_minLightBright;
		else
			if(  m_brightness > m_attr->m_maxLightBright  )
				m_brightness = m_attr->m_maxLightBright;
			
	}
	else 
	m_sentMove = false;
}







void Lamp::Fade()
{

	double dt = Session::m_moment - m_lastEventTime;
        m_lastEventTime = Session::m_moment;

	if(!m_isVisible)
		return;

	double db;

	switch (m_blinkState)
	{
	case 1: // fade out, check if min bright

		db = m_attr->m_FadeOutCoeff * dt * dt;
		
		m_brightness  -= db;
		m_currentTransparency -= m_attr->m_coronaFadeCoeff * db;


		if(  m_brightness < m_attr->m_minLightBright  )
		{
			m_brightness = m_attr->m_minLightBright;
			m_currentTransparency = 0;
			m_blinkState = 2;
		}
		break;	
	case 2: // fade in, check if full bright

		db = m_attr->m_FadeInCoeff  * dt * dt ;

		m_brightness += db;
		m_currentTransparency += m_attr->m_coronaFadeCoeff * db;

		if(  m_brightness > m_attr->m_maxLightBright  )
		{
			m_brightness = m_attr->m_maxLightBright;
			m_currentTransparency =	m_attr->m_coronaAlpha;
			m_blinkState = 1;
		}
		break;
	}
}

void Lamp::Blink(KR_Event &event)
{
	if (m_blinkState) // on
	{
			m_blinkState = 0;

			event.timeStamp += m_attr->m_sleepTime;
			issueEvent(event);
	}
	 else	// off
	{
			m_blinkState = 1;

			event.timeStamp += m_attr->m_maxBrightTime;
			issueEvent(event);		
	}
}

void Lamp::setLampAttr()
{
	ct_Attribute *attr = __attrLampTable.searchAttribute(m_lampAttrID);
	if( attr==NULL )
		echo( "Fountain::receiveEvent: Unknown attribute %s",
		context->searchObject(m_lampAttrID));
	else m_attr = (AttributeLamp*)attr;
	
	if(  m_attr->m_onLand  )
	{
		double height;
		CFVector3 normal;
		CViewScene::Current()->GetTerrain()->GetPlane(m_lampPosition,normal,height);
		m_lampPosition.y = height;
	}
	
}

 //============================================================
int Lamp::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case lmp_EV_MOVE:

			switch (m_attr->m_lightMode )
			{
				case 1: //randomly changeable
					RandomChange(event);
					break;
				case 2: // blink
					Blink(event);
					break;
			}	
            break;

	case lmp_EV_CHANGEPOS:

				if (m_movementCurrentDirection == 0) // forward
				{
					m_movementCurrentStep++;
					if (m_movementCurrentStep == m_movementNumberOfSteps)
					{
						if (m_attr->m_movementType)
						{
							m_movementCurrentDirection = 1;	// back;
						}
							else
						{
							m_movementCurrentStep = 0;
						}

					}
				}
				 else // back
				{
					m_movementCurrentStep--;
					if (m_movementCurrentStep == 0)
						m_movementCurrentDirection = 0;
				}

				m_lampPosition = m_startPosition + m_movementDelta * m_movementCurrentStep;
	
			    event.timeStamp  += m_attr->m_movementDeltaT;
				issueEvent( event );		


			break;

    case KR_WAKE_UP:
            break;

    case KR_SET_ATTR:
            s_ASSERTNQ("Lamp:receiveEvent:case KR_SET_ATTR: do't supported");
            break;

    case lmp_EV_REMOVE:
            context->removeObject(getObjectID());
            break;

	case lmp_EV_SETENDPOS:
		{
			KR_ObjectID oID;

		       event.data.open(EDO_READ)
                        .getObjectID(oID)
                        .getDouble(m_endPosition.x)
                        .getDouble(m_endPosition.y)
                        .getDouble(m_endPosition.z)
						.getInt   (m_movementNumberOfSteps)
						.getInt   (m_movementCurrentStep)
                      .close();

			m_movementDelta.x = (m_endPosition.x - m_startPosition.x) / m_movementNumberOfSteps;
			m_movementDelta.y = (m_endPosition.y - m_startPosition.y) / m_movementNumberOfSteps;
			m_movementDelta.z = (m_endPosition.z - m_startPosition.z) / m_movementNumberOfSteps;
		}
			break;

    case lmp_EV_START:
     {
            //KR_ObjectID oID;
            event.data.open(EDO_READ)
                        .getObjectID(m_lampAttrID)
                        .getDouble(m_lampPosition.x)
                        .getDouble(m_lampPosition.y)
                        .getDouble(m_lampPosition.z)
                      .close();

	
			m_startPosition = m_lampPosition;

			setLampAttr();
			
     }
     m_sentMove = false;
     m_lastEventTime = m_startTime = event.timeStamp;

     m_blinkState = 1;
     m_brightness = m_attr->m_maxLightBright;
     m_currentTransparency = m_attr->m_coronaAlpha;
	 m_movementCurrentDirection = 0;


	 m_particle.m_color = GRCreateColor(RGB_TO_LIST(m_attr->m_coronaRGB));


     
     if(   m_attr->m_maxTimeLife > 0  )
     {
           event.label       = lmp_EV_REMOVE;
           event.source      = getObjectID();
           event.destination = getObjectID();
           event.timeStamp   = m_startTime + m_attr->m_maxTimeLife;
           issueEvent( event );
     }

	 if ( m_attr->m_isMoving)
	 {
           event.label       = lmp_EV_CHANGEPOS;
           event.source      = getObjectID();
           event.destination = getObjectID();
           event.timeStamp   = m_startTime + m_attr->m_movementDeltaT;
           issueEvent( event );		
	 }


     break;


    default: return 0;
    }
    return 1;
 }

 //============================================================
void Lamp::addNotify()
 {
    ct_Subject::addNotify();
    // insert your code this
    m_attr = 0;
    m_brightness = 0;
 }

 //============================================================
void Lamp::removeNotify()
 {
    ct_Subject::removeNotify();
    // insert your code this
 }


 /*************************************
  *
  *   LampTable implementation
  *
  *************************************/

 //============================================================
void LampTable::allocObjects( int objectQnty )
 {
    m_table = new Lamp[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void LampTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *LampTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index <= m_maxObjectQnty ,"LampTable::getObjectPTR");
    return &(m_table[ index ]);
 }

void Lamp::render( CViewDynamicList &list,double)
{


	if (m_attr->m_hasParticle)
	{	    
	    m_particle.prepareToRender(m_lampPosition,m_attr->m_particleWidth);
	    list.Load( & m_particle);
	}
	


	if (m_attr->m_lightMode == 3)
	 Fade();

	
	if (m_blinkState)
	{
		g_lightChain.add( m_lampPosition+CFVector3(0,m_attr->m_lightOffset,0), 
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
				//m_attr->m_coronaAlpha,
				m_currentTransparency,
				m_attr->m_coronaColor,
				m_attr->m_coronaHText
				);
			list.Load( &m_corona );
		}

		m_renderLastFrame = 1;
	}
}

void Lamp::endRender( CViewScene *scene )
{
     if(  m_attr->m_useCorona  && m_renderLastFrame)	
          scene->RemoveLandDynamic( &m_corona );


	if (m_attr->m_hasParticle)	
	    scene->RemoveLandDynamic( & m_particle);


	 m_renderLastFrame = 0;
}


bool	Lamp::dump(PIN_SaveFile & sf)
{

		if (!ct_Subject::dump(sf))
			return false;

		if (!sf.WriteData( (char *) & m_lampAttrID, sizeof(LampData)  ))
			return false;

		return true;
}

bool	Lamp::load(PIN_SaveFile & sf)
{
		if (!ct_Subject::load(sf))
			return false;

		if (!sf.GetData( (char *) & m_lampAttrID, sizeof(LampData)  ))
			return false;
		
		return true;
}

void	Lamp::loadNotify()
{
	ct_Subject::loadNotify();
	setLampAttr();
}


/* End of file C:\NW\ARENA\OBASE\Lamp\Lamp.cpp */
