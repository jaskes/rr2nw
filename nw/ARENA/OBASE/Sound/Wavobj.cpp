/*
 * File  : C:\NW\ARENA\OBASE\Sound\WAVOBJ.cpp
 * Autor :
 * Ver   1.0
 */
#include "WAVOBJ.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/skinmsg.h"
#include "message/sndmsg.h"
#include "WAVResourceState.h"

#ifndef RR2NW_WAV_RESOURCE_STATE_EXTERNAL
#include "WAVResourceState.inl"
#endif


bool SetSoundAttr(	const KR_ObjectID & selfID,
			SimulationContext *context,
			char * soundName,
			ct_ClassTableID & ctsndID,
			void * wav)
{
    if (context != NULL && soundName != NULL && *soundName && wav != NULL)
    {

	KR_ObjectID	wavID;

        wavID     = context->searchObject(soundName);
        const ct_ClassTableID soundTable =
            g_arena.searchSeanceClassTable("SoundObj");

	if (!wavID.isNUL() && soundTable != ct_NULLID)
	{
	   KR_Event event;
           event.label       = sk_EV_QUERY_MODEL_PTR;
           event.destination = wavID;
           event.source      = selfID;
           event.timeStamp   = 0.1; //FIXME
           context->sendEventNow( event );
	   //context->addEvent( event );

	   WAVObj *resolved = NULL;
	   s_EventData &data = event.data.open(EDO_READ);
	   if (event.label != sk_EV_QUERY_MODEL_PTR_OK ||
	       data.remaining() != static_cast<int>(sizeof(resolved)))
	   {
	       data.close();
	       return false;
	   }
	   data.get(&resolved, sizeof(resolved)).close();
	   if (!WAVResourceState_IsLoadedPointer(resolved))
	       return false;

	   *static_cast<WAVObj **>(wav) = resolved;
	   ctsndID = soundTable;

	   return true;
	}
	
    }

    return false;
}                   	


void updateSound( const KR_ObjectID & selfID,
		  SimulationContext *context,
		  ct_ClassTableID & ctsndID,
		  WAVObj * wav,
		  KR_ObjectID & snd )
{
    snd = KR_ObjectID::NUL();

    if ( context != NULL && ctsndID != ct_NULLID &&
         WAVResourceState_IsLoadedPointer(wav))
    {
	snd = g_arena.newObject(ctsndID,"snd.snd");

    if(  snd.isNUL()  ) 
         return;

	KR_Event event;

	event.data.open(EDO_WRITE)
              .put( & wav,sizeof(void *))
         .close();
      
	event.label = snd_EV_SET_WAV;
    event.destination = snd;
	event.source      = selfID;
	event.timeStamp   = 0.1; // FIXME

    context->sendEventNow( event );
    //context->addEvent( event );
   }
}

/* End of file C:\NW\ARENA\OBASE\Sound\WAVOBJ.cpp */
