/*
 * File  : C:\NW\ARENA\OBASE\Farter\Farter.cpp
 * Autor :
 * Ver   1.0
 */
#include <cmath>
#include <cstring>
#include <new>

#include "Farter.h"
#include "FarterSubjectState.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/fartmsg.h"
#include "Sound.h"
#include "obase/sound/wavobj.h"
#include "obase/sound/SoundObjectState.h"
#include "message/sndmsg.h"

#ifndef RR2NW_FARTER_ATTRIBUTE_STATE_EXTERNAL
#include "FarterAttributeState.inl"
#endif

static AttributeFarter __defaultAttr;

 //===========================================================================
class FarterTable : public ct_SubjectTable
{
 private:
    Farter *m_table;
 public:
    FarterTable()
    {
        m_table = NULL;
        registerClass( "Farter" );
    }
    ~FarterTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    virtual  bool      isAudible();
    Farter            *find(const KR_ObjectID &objectID);
    int                capacity() const { return m_maxObjectQnty; }
    int                liveCount();
};


bool FarterTable::isAudible()
{
  return true;
}

static FarterTable  __classTable;
 /*********************************
  *
  *   Farter implementation
  *
  *********************************/

 //============================================================
Farter::Farter()
 {
    resetState();
 }

 //============================================================
Farter::~Farter()
 {
 }

void Farter::resetState()
 {
    m_position = CFVector3(0,0,0);
    m_snd = KR_ObjectID::NUL();
    m_attr = &__defaultAttr;
 }

 //============================================================
int Farter::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case KR_WAKE_UP:
            break;

    case KR_SET_ATTR:
            s_ASSERTNQ("Farter:receiveEvent:case KR_SET_ATTR: do't supported");
            break;

    case START_FARTING:
       {
        KR_ObjectID oID;
        CFVector3 position;
        double ts = event.timeStamp;

        s_EventData &data = event.data.open(EDO_READ);
        if (data.remaining() !=
            static_cast<int>(sizeof(KR_ObjectID) + sizeof(double) * 3))
        {
            data.close();
            return 0;
        }
        data.getObjectID(oID)
            .getDouble(position.x)
            .getDouble(position.y)
            .getDouble(position.z)
            .close();
        if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
            !std::isfinite(position.z))
            return 0;

        ct_Attribute *attr = __attrFarterTable.searchAttribute(oID);

        if( attr==NULL )
           return 0;

        if (!m_snd.isNUL())
        {
            if (context->isExist(m_snd))
                context->removeObject(m_snd);
            m_snd = KR_ObjectID::NUL();
        }
        m_attr = (AttributeFarter*)attr;
        m_position = position;


	 updateSound( getObjectID(),
	    	     context,
		     m_attr->m_ctsndID,
		     m_attr->m_wav,
		     m_snd );

         if (m_snd.isNUL())
         {
            resetState();
            return 0;
         }
	 {
          event.label = snd_EV_MOVE_TO;
          event.destination = m_snd;
          event.source      = getObjectID();
	  event.timeStamp   = ts;
          event.data.open(EDO_WRITE)
                     .putDouble(m_position.x)
                     .putDouble(m_position.y)
                     .putDouble(m_position.z)
                   .close();
          context->sendEventNow( event );
        }


      }
      break;
    default: return 0;
    }
    return 1;
 }

 //============================================================

void Farter::onExitAudibleZone(double ts) 
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

 //============================================================

void Farter::onEnterAudibleZone(double ts) 
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

void Farter::addNotify()
 {
    ct_Subject::addNotify();
    resetState();
 }

 //============================================================
void Farter::removeNotify()
 {

    if(  !m_snd.isNUL()  )
    {
         if (context->isExist(m_snd))
             context->removeObject( m_snd );
         m_snd = KR_ObjectID::NUL();
    }

    resetState();
    ct_Subject::removeNotify();
    // insert your code this
 }

 //============================================================
CFVector3     Farter::realPosition() {  return m_position;  }

 //============================================================
void Farter::draw()
 {
 }

 /*************************************
  *
  *   FarterTable implementation
  *
  *************************************/

 //============================================================
void FarterTable::allocObjects( int objectQnty )
 {
    m_table = new (std::nothrow) Farter[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void FarterTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *FarterTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index < m_maxObjectQnty ,"FarterTable::getObjectPTR");
    return &(m_table[ index ]);
 }

namespace {

struct FarterLiveQuery
{
    KR_ObjectID target;
    bool found;
};

bool FindLiveFarter(const KR_ObjectID objectID, void *user)
{
    FarterLiveQuery *query = static_cast<FarterLiveQuery *>(user);
    if (objectID == query->target)
    {
        query->found = true;
        return false;
    }
    return true;
}

bool CountLiveFarter(const KR_ObjectID, void *user)
{
    ++(*static_cast<int *>(user));
    return true;
}

const unsigned long long kFarterSubjectHashOffset =
    14695981039346656037ull;
const unsigned long long kFarterSubjectHashPrime = 1099511628211ull;

void FarterSubjectHashBytes(unsigned long long &hash,
                            const void *data,
                            int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kFarterSubjectHashPrime;
    }
}

void FarterSubjectHashString(unsigned long long &hash, const char *value)
{
    FarterSubjectHashBytes(hash, value,
                           static_cast<int>(std::strlen(value)) + 1);
}

}  // namespace

Farter *FarterTable::find(const KR_ObjectID &objectID)
{
    FarterLiveQuery query = {objectID, false};
    userFind(FindLiveFarter, &query);
    if (!query.found)
        return NULL;
    for (int i = 0; i < m_maxObjectQnty; ++i)
        if (m_table[i].getObjectID() == objectID)
            return &m_table[i];
    return NULL;
}

int FarterTable::liveCount()
{
    int count = 0;
    userFind(CountLiveFarter, &count);
    return count;
}

void FarterSubjectState_Link()
{
}

bool FarterSubjectState_TableReady(SimulationContext *context, int capacity)
{
    return context != NULL && capacity > 0 &&
           g_arena.getContext() == context &&
           g_arena.searchSeanceClassTable("Farter") != ct_NULLID &&
           __classTable.capacity() == capacity;
}

int FarterSubjectState_Capacity()
{
    return __classTable.capacity();
}

int FarterSubjectState_LiveCount()
{
    return __classTable.liveCount();
}

unsigned long long FarterSubjectState_Fingerprint(SimulationContext *context)
{
    if (!FarterSubjectState_TableReady(context, __classTable.capacity()) ||
        __classTable.liveCount() != 0)
        return 0;
    unsigned long long hash = kFarterSubjectHashOffset;
    const int capacity = __classTable.capacity();
    const int audible = __classTable.isAudible() ? 1 : 0;
    const int commandLifecycle = 1;
    FarterSubjectHashString(hash, "Farter");
    FarterSubjectHashBytes(hash, &capacity, sizeof(capacity));
    FarterSubjectHashBytes(hash, &audible, sizeof(audible));
    FarterSubjectHashBytes(hash, &commandLifecycle,
                           sizeof(commandLifecycle));
    return hash;
}

unsigned long long FarterSubjectState_AbsentFingerprint()
{
    unsigned long long hash = kFarterSubjectHashOffset;
    const int capacity = 0;
    const int audible = 0;
    const int commandLifecycle = 1;
    FarterSubjectHashString(hash, "Farter");
    FarterSubjectHashBytes(hash, &capacity, sizeof(capacity));
    FarterSubjectHashBytes(hash, &audible, sizeof(audible));
    FarterSubjectHashBytes(hash, &commandLifecycle,
                           sizeof(commandLifecycle));
    return hash;
}

bool FarterSubjectState_ProbeLifecycle(SimulationContext *context,
                                       const char *attributeName,
                                       double timeStamp)
{
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        __classTable.liveCount() != 0 || SoundObjectState_LiveCount() != 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeFarter *attribute = static_cast<AttributeFarter *>(
        __attrFarterTable.searchAttribute(attributeID));
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Farter");
    if (attributeID.isNUL() || attribute == NULL || attribute->m_wav == NULL ||
        attribute->m_ctsndID == ct_NULLID || table == ct_NULLID ||
        context->isExist("Farter.Subject.Probe") ||
        context->isExist("snd.snd"))
        return false;

    KR_ObjectID invalid =
        g_arena.newObject(table, "Farter.Subject.Invalid.Probe");
    Farter *invalidObject = __classTable.find(invalid);
    KR_Event event;
    event.label = START_FARTING;
    event.data.open(EDO_WRITE).putInt(1).close();
    const bool invalidRejected = !invalid.isNUL() && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 &&
        invalidObject->m_attr == &__defaultAttr &&
        !invalidObject->hasSoundObject();
    if (!invalid.isNUL() && context->isExist(invalid))
        context->removeObject(invalid);
    if (!invalidRejected || __classTable.liveCount() != 0 ||
        SoundObjectState_LiveCount() != 0)
        return false;

    KR_ObjectID subject =
        g_arena.newObject(table, "Farter.Subject.Probe");
    Farter *object = __classTable.find(subject);
    if (subject.isNUL() || object == NULL)
        return false;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    event = KR_Event();
    event.label = START_FARTING;
    event.source = g_arena.getObjectID();
    event.destination = subject;
    event.timeStamp = ts;
    event.data.open(EDO_WRITE)
              .putObjectID(attributeID)
              .putDouble(31.0)
              .putDouble(7.5)
              .putDouble(-19.0)
            .close();
    context->sendEventNow(event);
    const KR_ObjectID sound = object->soundObjectID();
    const CFVector3 startedPosition = object->farterPosition();
    const bool started = object->m_attr == attribute &&
        startedPosition.x == 31.0 && startedPosition.y == 7.5 &&
        startedPosition.z == -19.0 &&
        object->hasSoundObject() && SoundObjectState_LiveCount() == 1 &&
        SoundObjectState_Matches(sound, attribute->m_wav,
                                 31.0, 7.5, -19.0, true, false, 0);

    object->onEnterAudibleZone(ts);
    const bool entered =
        SoundObjectState_Matches(sound, attribute->m_wav,
                                 31.0, 7.5, -19.0, true, true, 0);
    object->onExitAudibleZone(ts);
    const bool exited =
        SoundObjectState_Matches(sound, attribute->m_wav,
                                 31.0, 7.5, -19.0, true, false, 0);

    context->removeObject(subject);
    subject = g_arena.newObject(table, "Farter.Subject.Probe");
    Farter *reused = __classTable.find(subject);
    const CFVector3 reusedPosition =
        reused == NULL ? CFVector3(1,1,1) : reused->farterPosition();
    const bool reusedClean = reused != NULL &&
        reused->m_attr == &__defaultAttr && !reused->hasSoundObject() &&
        reusedPosition.x == 0.0 && reusedPosition.y == 0.0 &&
        reusedPosition.z == 0.0;
    if (!subject.isNUL() && context->isExist(subject))
        context->removeObject(subject);

    return started && entered && exited && reusedClean &&
           __classTable.liveCount() == 0 &&
           SoundObjectState_LiveCount() == 0 &&
           !context->isExist("Farter.Subject.Invalid.Probe") &&
           !context->isExist("Farter.Subject.Probe") &&
           !context->isExist("snd.snd");
}

/* End of file C:\NW\ARENA\OBASE\Farter\Farter.cpp */
