#ifndef RR2NW_WAV_RESOURCE_STATE_INL
#define RR2NW_WAV_RESOURCE_STATE_INL

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

WAVObjTable __wavObjTable;

namespace {

const unsigned long long kWAVHashOffset = 14695981039346656037ull;
const unsigned long long kWAVHashPrime = 1099511628211ull;
const char kWAVPathPrefix[] = "..\\SOUND\\";

void WAVHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kWAVHashPrime;
    }
}

void WAVHashString(unsigned long long &hash, const char *value)
{
    WAVHashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

struct WAVRosterEntry
{
    std::string name;
    WAVObj *object;
};

struct WAVRosterCollector
{
    SimulationContext *context;
    std::vector<WAVRosterEntry> entries;
    bool valid;
};

bool CollectWAVRosterEntry(const KR_ObjectID objectID, void *user)
{
    WAVRosterCollector *collector = static_cast<WAVRosterCollector *>(user);
    const char *name = collector->context->searchObject(objectID);
    WAVObj *object = static_cast<WAVObj *>(
        collector->context->queryInterface(objectID, IUnknownIID));
    if (name == NULL || object == NULL || !object->m_loaded ||
        object->m_flags < 0 || object->m_flags > 1 ||
        object->m_rsxCE.cbSize != sizeof(RSXCACHEDEMITTERDESC) ||
        object->m_rsxEModel.cbSize != sizeof(RSXEMITTERMODEL) ||
        std::strncmp(object->m_rsxCE.szFilename, kWAVPathPrefix,
                     sizeof(kWAVPathPrefix) - 1) != 0)
    {
        collector->valid = false;
        return false;
    }
    WAVRosterEntry entry = {name, object};
    collector->entries.push_back(entry);
    return true;
}

bool WAVRosterEntryLess(const WAVRosterEntry &left,
                        const WAVRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectWAVRoster(SimulationContext *context,
                      WAVRosterCollector &collector)
{
    if (context == NULL || __wavObjTable.capacity() <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __wavObjTable.userFind(CollectWAVRosterEntry, &collector);
    if (!collector.valid)
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              WAVRosterEntryLess);
    return true;
}

void ResetWAVMetadata(WAVObj *object)
{
    object->m_loaded = false;
    object->m_flags = 0;
    ZeroMemory(&object->m_rsxCE, sizeof(object->m_rsxCE));
    ZeroMemory(&object->m_rsxEModel, sizeof(object->m_rsxEModel));
}

}  // namespace

WAVObjTable::WAVObjTable() : m_table(NULL)
{
    registerClass("WAVObj");
}

WAVObjTable::~WAVObjTable()
{
    delete [] m_table;
    m_table = NULL;
}

void WAVObjTable::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) WAVObj[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void WAVObjTable::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *WAVObjTable::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "WAVObjTable::getObjectPTR");
    return &(m_table[index]);
}

WAVObj::WAVObj()
{
    ResetWAVMetadata(this);
}

WAVObj::~WAVObj()
{
}

int WAVObj::receiveEvent(KR_Event &event)
{
    switch (event.label)
    {
    case KR_WAKE_UP:
        break;

    case sk_EV_LOAD:
        {
            s_ASSERT(!m_loaded, "WAVObj::receiveEvent():LOAD Duplicate load");
            char fname[100];
            double fMinFront;
            double fMinBack;
            double fMaxFront;
            double fMaxBack;
            double fIntensity;
            int flags = 0;
            s_EventData &data = event.data.open(EDO_READ);
            data.getStr(fname, sizeof(fname))
                .getDouble(fMinFront)
                .getDouble(fMinBack)
                .getDouble(fMaxFront)
                .getDouble(fMaxBack)
                .getDouble(fIntensity);
            const int optionalBytes = data.remaining();
            if (optionalBytes == static_cast<int>(sizeof(flags)))
                data.getInt(flags);
            else if (optionalBytes != 0)
            {
                data.close();
                return 0;
            }
            data.close();
            if (flags < 0 || flags > 1)
                return 0;
            load(fname, fMinFront, fMinBack, fMaxFront, fMaxBack,
                 fIntensity, flags);
            m_loaded = true;
        }
        break;

    case sk_EV_QUERY_MODEL_PTR:
        {
            s_ASSERT(m_loaded, "WAVObj::receiveEvent():QUERY_MODEL_PTR");
            event.label = sk_EV_QUERY_MODEL_PTR_OK;
            void *self = this;
            event.data.open(EDO_WRITE).put(&self, sizeof(void *)).close();
        }
        break;

    default:
        return 0;
    }
    return 1;
}

void WAVObj::addNotify()
{
    ct_Object::addNotify();
    ResetWAVMetadata(this);
}

void WAVObj::removeNotify()
{
    ct_Object::removeNotify();
}

void WAVObj::load(const char *fname, double fMinFront, double fMinBack,
                  double fMaxFront, double fMaxBack, double fIntensity,
                  int flags)
{
    ZeroMemory(&m_rsxCE, sizeof(RSXCACHEDEMITTERDESC));
    m_rsxCE.cbSize = sizeof(RSXCACHEDEMITTERDESC);
    m_rsxCE.dwFlags = RSXEMITTERDESC_NODOPPLER | RSXEMITTERDESC_NOREVERB;
    // Retail LOADWAV.SCI calls LoadWAVEx(..., 1) for streamed/uncached
    // dialogue and music. The January source always forced cached playback.
    if ((flags & 1) == 0)
        m_rsxCE.dwFlags |=
            RSXEMITTERDESC_PREPROCESS | RSXEMITTERDESC_INMEMORY;
    if (!snd_true3d)
        m_rsxCE.dwFlags |= RSXEMITTERDESC_NOSPATIALIZE;
    std::snprintf(m_rsxCE.szFilename, sizeof(m_rsxCE.szFilename),
                  "%s%s", kWAVPathPrefix, fname);

    m_rsxEModel.fMinFront = static_cast<float>(fMinFront);
    m_rsxEModel.fMinBack = static_cast<float>(fMinBack);
    m_rsxEModel.fMaxFront = static_cast<float>(fMaxFront);
    m_rsxEModel.fMaxBack = static_cast<float>(fMaxBack);
    m_rsxEModel.fIntensity = static_cast<float>(fIntensity);
    m_rsxEModel.cbSize = sizeof(RSXEMITTERMODEL);
    m_flags = flags;
}

void WAVResourceState_Link()
{
}

int WAVResourceState_RosterSize(SimulationContext *context)
{
    WAVRosterCollector collector = {};
    return CollectWAVRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : -1;
}

int WAVResourceState_Capacity()
{
    return __wavObjTable.capacity();
}

unsigned long long WAVResourceState_Fingerprint(SimulationContext *context)
{
    WAVRosterCollector collector = {};
    if (!CollectWAVRoster(context, collector))
        return 0;
    unsigned long long hash = kWAVHashOffset;
    const int capacity = __wavObjTable.capacity();
    WAVHashBytes(hash, &capacity, sizeof(capacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        WAVObj &object = *collector.entries[i].object;
        WAVHashString(hash, collector.entries[i].name.c_str());
        WAVHashString(hash, object.m_rsxCE.szFilename +
                                sizeof(kWAVPathPrefix) - 1);
        WAVHashBytes(hash, &object.m_rsxEModel.fMinFront,
                     sizeof(object.m_rsxEModel.fMinFront));
        WAVHashBytes(hash, &object.m_rsxEModel.fMinBack,
                     sizeof(object.m_rsxEModel.fMinBack));
        WAVHashBytes(hash, &object.m_rsxEModel.fMaxFront,
                     sizeof(object.m_rsxEModel.fMaxFront));
        WAVHashBytes(hash, &object.m_rsxEModel.fMaxBack,
                     sizeof(object.m_rsxEModel.fMaxBack));
        WAVHashBytes(hash, &object.m_rsxEModel.fIntensity,
                     sizeof(object.m_rsxEModel.fIntensity));
        WAVHashBytes(hash, &object.m_flags, sizeof(object.m_flags));
    }
    return hash;
}

bool WAVResourceState_AllLoaded(SimulationContext *context)
{
    WAVRosterCollector collector = {};
    return CollectWAVRoster(context, collector) &&
           !collector.entries.empty();
}

#endif
