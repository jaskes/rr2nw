/*
 * File  : C:\NW\ARENA\OBASE\Sound\SoundObj.cpp
 * Autor :
 * Ver   1.0
 */
#include <cmath>
#include <cstring>
#include <new>

#include "SoundObj.h"
#include "SoundObjectState.h"
#include "WAVResourceState.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/unitmsg.h"
#include "message/sndmsg.h"



 //===========================================================================
class SoundObjTable : public ct_ClassTable
{
 private:
    SoundObj *m_table;
 public:
    SoundObjTable()
    {
        m_table = NULL;
        registerClass( "SoundObj" );
    }
    ~SoundObjTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void       allocObjects( int objectQnty );
    virtual void       freeObjects ();
    virtual ct_Object *getObjectPTR( int index );
    SoundObj          *find(const KR_ObjectID &objectID);
    int                capacity() const { return m_maxObjectQnty; }
    int                liveCount();
};

static SoundObjTable  __classTable;
 /*********************************
  *
  *   SoundObj implementation
  *
  *********************************/

 //============================================================
SoundObj::SoundObj()
 {
    resetState();
 }

 //============================================================
SoundObj::~SoundObj()
 {
    releaseEmitter();
 }

void SoundObj::releaseEmitter()
 {
#ifndef RR2NW_SOUNDOBJ_DEVICE_FREE
    if (m_lpCE != 0)
    {
        if (m_playing)
            m_lpCE->ControlMedia(RSX_STOP, 0, 0);
        m_lpCE->Release();
    }
#endif
    m_lpCE = 0;
    m_emitterValid = 0;
    m_playing = 0;
    m_playCount = 0;
 }

void SoundObj::resetState()
 {
    m_lpCE = 0;
    m_emitterValid = 0;
    m_positionValid = 0;
    m_playing = 0;
    m_playCount = 0;
    m_position = CFVector3(0,0,0);
    m_wav = 0;
 }

 //============================================================
int SoundObj::receiveEvent( KR_Event &event )
 {
    switch( event.label )
    {
    case KR_WAKE_UP:
            break;

    case snd_EV_SET_WAV:
            {
            WAVObj *wav = 0;
            s_EventData &data = event.data.open(EDO_READ);
            if (data.remaining() != static_cast<int>(sizeof(wav)))
            {
                data.close();
                return 0;
            }
            data.get(&wav, sizeof(wav)).close();
            if (!WAVResourceState_IsLoadedPointer(wav))
                return 0;

            releaseEmitter();
            m_wav = wav;

#ifndef RR2NW_SOUNDOBJ_DEVICE_FREE
	    if (!lpRSX2Unk)
		return 1;
            HRESULT hr = CoCreateInstance(
                CLSID_RSXCACHEDEMITTER,     // GUID for cachedemitter object
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_IRSXCachedEmitter,
                (void ** )&m_lpCE);

            if ( FAILED(hr) )
                return 1;

            hr = m_lpCE->Initialize(&getWAV()->m_rsxCE, lpRSX2Unk);

            if ( FAILED(hr))
            {
                releaseEmitter();
                return 1;
            }

            hr = m_lpCE->SetModel(&getWAV()->m_rsxEModel);

            if ( FAILED(hr))
            {
                releaseEmitter();
                return 1;
            }


            m_emitterValid = 1;
            if (m_positionValid)
                onChangePos();
#endif
            }
            break;

    case snd_EV_MOVE_TO:
	    {
            CFVector3 position;
            s_EventData &data = event.data.open(EDO_READ);
            if (data.remaining() !=
                static_cast<int>(sizeof(double) * 3))
            {
                data.close();
                return 0;
            }
            data.getDouble(position.x)
                .getDouble(position.y)
                .getDouble(position.z)
                .close();
            if (!std::isfinite(position.x) ||
                !std::isfinite(position.y) ||
                !std::isfinite(position.z))
                return 0;
            m_position = position;
            m_positionValid = 1;
            onChangePos();
	    }
            break;

    case snd_EV_START:	    	
            {	
            int count;
            s_EventData &data = event.data.open(EDO_READ);
            if (data.remaining() != static_cast<int>(sizeof(count)))
            {
                data.close();
                return 0;
            }
            data.getInt(count).close();
            if (m_wav == 0 || count < 0)
                return 0;
            startPlay(count);
            }
            break;

    case snd_EV_END:
            if (m_wav == 0)
                return 0;
            endPlay();
            break;

    default: return 0;
    }
    return 1;
 }

 //============================================================
void SoundObj::addNotify()
 {
    ct_Object::addNotify();
    resetState();
 }

 //============================================================
void SoundObj::removeNotify()
 {
    releaseEmitter();
    resetState();
    ct_Object::removeNotify();
 }

 /*************************************
  *
  *   SoundObjTable implementation
  *
  *************************************/

 //============================================================
void SoundObjTable::allocObjects( int objectQnty )
 {
    m_table = new (std::nothrow) SoundObj[ objectQnty ];

    if(  m_table == NULL  )
         m_maxObjectQnty = 0;
 }

 //============================================================
void SoundObjTable::freeObjects()
 {
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
 }

 //============================================================
ct_Object *SoundObjTable::getObjectPTR( int index )
 {
    s_ASSERT( index >= 0 && index < m_maxObjectQnty ,"SoundObjTable::getObjectPTR");
    return &(m_table[ index ]);
 }

namespace {

struct SoundObjectLiveQuery
{
    KR_ObjectID target;
    bool found;
};

bool FindLiveSoundObject(const KR_ObjectID objectID, void *user)
{
    SoundObjectLiveQuery *query =
        static_cast<SoundObjectLiveQuery *>(user);
    if (objectID == query->target)
    {
        query->found = true;
        return false;
    }
    return true;
}

bool CountSoundObject(const KR_ObjectID, void *user)
 {
    ++(*static_cast<int *>(user));
    return true;
 }

}  // namespace

SoundObj *SoundObjTable::find(const KR_ObjectID &objectID)
 {
    SoundObjectLiveQuery query = {objectID, false};
    userFind(FindLiveSoundObject, &query);
    if (!query.found)
        return 0;
    for (int i = 0; i < m_maxObjectQnty; ++i)
        if (m_table[i].getObjectID() == objectID)
            return &m_table[i];
    return 0;
 }

int SoundObjTable::liveCount()
 {
    int count = 0;
    userFind(CountSoundObject, &count);
    return count;
 }



WAVObj *SoundObj::getWAV()
 {
    s_ASSERT(m_wav,"SoundObj::getWAV(): Unknown wave");
    return m_wav;
 }

CFVector3  SoundObj::getPosition()
 {
    return m_position;
 }

void SoundObj::onChangePos()
 {
    if (!m_emitterValid)
        return;

//    m_positionValid = 1;	

    RSXVECTOR3D v3d;
    CFVector3 pos = getPosition();

    v3d.x = pos.x;
    v3d.y = pos.y;
    v3d.z = pos.z;

    m_lpCE->SetPosition(&v3d);
 }

void SoundObj::startPlay( int count )
 {
    if (m_wav == 0 || count < 0)
        return;
    m_playing = 1;
    m_playCount = count;
    if (m_emitterValid)
        m_lpCE->ControlMedia(RSX_PLAY, count, 0);
 }

void SoundObj::endPlay()
 {
    m_playing = 0;
    m_playCount = 0;
    if (m_emitterValid)
        m_lpCE->ControlMedia(RSX_STOP, 0, 0);
 }

namespace {

const unsigned long long kSoundObjectHashOffset = 14695981039346656037ull;
const unsigned long long kSoundObjectHashPrime = 1099511628211ull;

void SoundObjectHashBytes(unsigned long long &hash, const void *data, int size)
 {
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kSoundObjectHashPrime;
    }
 }

void SoundObjectHashString(unsigned long long &hash, const char *value)
 {
    SoundObjectHashBytes(hash, value,
                         static_cast<int>(std::strlen(value)) + 1);
 }

}  // namespace

void SoundObjectState_Link()
 {
 }

bool SoundObjectState_TableReady(SimulationContext *context, int capacity)
 {
    return context != 0 && capacity > 0 &&
           g_arena.getContext() == context &&
           g_arena.searchSeanceClassTable("SoundObj") != ct_NULLID &&
           __classTable.capacity() == capacity;
 }

bool SoundObjectState_DeviceFree()
 {
#ifdef RR2NW_SOUNDOBJ_DEVICE_FREE
    return true;
#else
    return false;
#endif
 }

int SoundObjectState_Capacity()
 {
    return __classTable.capacity();
 }

int SoundObjectState_LiveCount()
 {
    return __classTable.liveCount();
 }

bool SoundObjectState_Matches(const KR_ObjectID &objectID,
                              const WAVObj *wav,
                              double x,
                              double y,
                              double z,
                              bool positionValid,
                              bool playing,
                              int playCount)
 {
    SoundObj *object = __classTable.find(objectID);
    if (object == 0 || object->m_wav != wav ||
        object->positionValid() != positionValid ||
        object->playing() != playing || object->playCount() != playCount)
        return false;
    if (!positionValid)
        return true;
    const CFVector3 position = object->getPosition();
    return position.x == x && position.y == y && position.z == z;
 }

unsigned long long SoundObjectState_Fingerprint(SimulationContext *context)
 {
    if (!SoundObjectState_TableReady(context, __classTable.capacity()))
        return 0;
    unsigned long long hash = kSoundObjectHashOffset;
    const int capacity = __classTable.capacity();
    const int deviceFree = SoundObjectState_DeviceFree() ? 1 : 0;
    const int wavBinding = 1;
    const int eventLifecycle = 1;
    SoundObjectHashString(hash, "SoundObj");
    SoundObjectHashBytes(hash, &capacity, sizeof(capacity));
    SoundObjectHashBytes(hash, &deviceFree, sizeof(deviceFree));
    SoundObjectHashBytes(hash, &wavBinding, sizeof(wavBinding));
    SoundObjectHashBytes(hash, &eventLifecycle, sizeof(eventLifecycle));
    return hash;
 }

bool SoundObjectState_ProbeLifecycle(SimulationContext *context,
                                     const char *wavName,
                                     double timeStamp)
 {
    if (!SoundObjectState_DeviceFree() || context == 0 || wavName == 0 ||
        wavName[0] == 0)
        return false;
    const int liveBaseline = __classTable.liveCount();
    WAVObj *wav = 0;
    if (!WAVResourceState_ResolveLoaded(context, wavName, &wav))
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("SoundObj");
    if (table == ct_NULLID || context->isExist("SoundObj.Invalid.Probe"))
        return false;

    KR_ObjectID invalid =
        g_arena.newObject(table, "SoundObj.Invalid.Probe");
    SoundObj *invalidObject = __classTable.find(invalid);
    WAVObj *missing = 0;
    KR_Event event;
    event.label = snd_EV_SET_WAV;
    event.data.open(EDO_WRITE).put(&missing, sizeof(missing)).close();
    const bool invalidRejected = !invalid.isNUL() && invalidObject != 0 &&
        invalidObject->receiveEvent(event) == 0 &&
        !invalidObject->hasWAV() && !invalidObject->emitterValid() &&
        !invalidObject->positionValid() && !invalidObject->playing();
    if (!invalid.isNUL() && context->isExist(invalid))
        context->removeObject(invalid);
    if (!invalidRejected || __classTable.liveCount() != liveBaseline)
        return false;

    KR_ObjectID sound;
    ct_ClassTableID soundTable = table;
    updateSound(g_arena.getObjectID(), context, soundTable, wav, sound);
    SoundObj *object = __classTable.find(sound);
    if (sound.isNUL() || object == 0 || object->m_wav != wav ||
        object->emitterValid() || object->positionValid() ||
        object->playing())
    {
        if (!sound.isNUL() && context->isExist(sound))
            context->removeObject(sound);
        return false;
    }

    event = KR_Event();
    event.label = snd_EV_MOVE_TO;
    event.source = g_arena.getObjectID();
    event.destination = sound;
    event.timeStamp = timeStamp < 0.1 ? 0.1 : timeStamp;
    event.data.open(EDO_WRITE)
              .putDouble(12.0)
              .putDouble(-3.5)
              .putDouble(44.0)
            .close();
    context->sendEventNow(event);
    const CFVector3 expectedPosition(12.0, -3.5, 44.0);
    const bool moved = object->positionValid() &&
                       object->getPosition() == expectedPosition;

    event.label = snd_EV_START;
    event.data.open(EDO_WRITE).putInt(0).close();
    context->sendEventNow(event);
    const bool started = object->playing() && object->playCount() == 0;
    event.label = snd_EV_END;
    event.data.open(EDO_WRITE).close();
    context->sendEventNow(event);
    const bool ended = !object->playing() && object->playCount() == 0;

    context->removeObject(sound);
    sound = KR_ObjectID::NUL();
    updateSound(g_arena.getObjectID(), context, soundTable, wav, sound);
    SoundObj *reused = __classTable.find(sound);
    const bool reusedClean = reused != 0 && reused->m_wav == wav &&
                             !reused->emitterValid() &&
                             !reused->positionValid() &&
                             !reused->playing();
    if (!sound.isNUL() && context->isExist(sound))
        context->removeObject(sound);

    return moved && started && ended && reusedClean &&
           __classTable.liveCount() == liveBaseline &&
           !context->isExist("SoundObj.Invalid.Probe");
 }


/* End of file C:\NW\ARENA\OBASE\Sound\SoundObj.cpp */
