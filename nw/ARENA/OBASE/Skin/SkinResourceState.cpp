#include "SkinResourceState.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "SKIN.H"
#include "enum/spaceEnum.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "kernel/h/session.h"
#include "message/peopmsg.h"
#include "message/skinmsg.h"
#include "../output/animconst.h"

namespace {

class RecoveredSkinSprite : public ct_Object
{
 public:
    CViewTexture m_texture;
    bool m_loaded;

    RecoveredSkinSprite() : m_loaded(false) {}
    virtual ~RecoveredSkinSprite() {}
    virtual int receiveEvent(KR_Event &event);
    virtual void addNotify();
    virtual void removeNotify();
    virtual bool shouldDump() { return false; }
};

class RecoveredSkinTable : public ct_ClassTable
{
    Skin *m_table;

 public:
    RecoveredSkinTable() : m_table(NULL)
    {
        registerClass("Skin");
    }

    virtual ~RecoveredSkinTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void allocObjects(int objectQnty)
    {
        delete [] m_table;
        m_table = NULL;
        if (objectQnty <= 0)
        {
            m_maxObjectQnty = 0;
            return;
        }
        m_table = new (std::nothrow) Skin[objectQnty];
        if (m_table == NULL)
            m_maxObjectQnty = 0;
    }

    virtual void freeObjects()
    {
        delete [] m_table;
        m_table = NULL;
        m_maxObjectQnty = 0;
    }

    virtual ct_Object *getObjectPTR(int index)
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "RecoveredSkinTable::getObjectPTR");
        return &(m_table[index]);
    }

    Skin *find(const KR_ObjectID &object)
    {
        for (int i = 0; i < m_maxObjectQnty; ++i)
            if (m_table[i].getObjectID() == object)
                return &(m_table[i]);
        return NULL;
    }
};

class RecoveredSkinSprTable : public ct_ClassTable
{
    RecoveredSkinSprite *m_table;

 public:
    RecoveredSkinSprTable() : m_table(NULL)
    {
        registerClass("SkinSpr");
    }

    virtual ~RecoveredSkinSprTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void allocObjects(int objectQnty)
    {
        delete [] m_table;
        m_table = NULL;
        if (objectQnty <= 0)
        {
            m_maxObjectQnty = 0;
            return;
        }
        m_table = new (std::nothrow) RecoveredSkinSprite[objectQnty];
        if (m_table == NULL)
            m_maxObjectQnty = 0;
    }

    virtual void freeObjects()
    {
        delete [] m_table;
        m_table = NULL;
        m_maxObjectQnty = 0;
    }

    virtual ct_Object *getObjectPTR(int index)
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "RecoveredSkinSprTable::getObjectPTR");
        return &(m_table[index]);
    }

    RecoveredSkinSprite *find(const KR_ObjectID &object)
    {
        for (int i = 0; i < m_maxObjectQnty; ++i)
            if (m_table[i].getObjectID() == object)
                return &(m_table[i]);
        return NULL;
    }
};

RecoveredSkinTable g_skinTable;
RecoveredSkinSprTable g_skinSprTable;

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

void HashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kHashPrime;
    }
}

void HashString(unsigned long long &hash, const char *value)
{
    if (value == NULL)
    {
        const char empty = 0;
        HashBytes(hash, &empty, 1);
        return;
    }
    HashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

struct ResourceEntry
{
    std::string kind;
    std::string name;
    int firstMetric;
    int secondMetric;
    double radius;
    double height;
    bool loaded;
};

struct ResourceCollector
{
    SimulationContext *context;
    std::vector<ResourceEntry> entries;
    bool valid;
};

bool CollectSkin(const KR_ObjectID object, void *user)
{
    ResourceCollector *collector = static_cast<ResourceCollector *>(user);
    const char *name = collector->context->searchObject(object);
    Skin *skin = g_skinTable.find(object);
    if (name == NULL || skin == NULL)
    {
        collector->valid = false;
        return false;
    }
    ResourceEntry entry;
    entry.kind = "Skin";
    entry.name = name;
    entry.firstMetric = skin->m_model.NumBaseSets();
    entry.secondMetric = skin->m_reducNum;
    entry.radius = skin->m_loaded ? skin->m_model.Radius() : 0.0;
    entry.height = skin->m_loaded ? skin->m_model.Height() : 0.0;
    entry.loaded = skin->m_loaded;
    collector->entries.push_back(entry);
    return true;
}

bool CollectSkinSpr(const KR_ObjectID object, void *user)
{
    ResourceCollector *collector = static_cast<ResourceCollector *>(user);
    const char *name = collector->context->searchObject(object);
    RecoveredSkinSprite *skin = g_skinSprTable.find(object);
    if (name == NULL || skin == NULL)
    {
        collector->valid = false;
        return false;
    }
    ResourceEntry entry;
    entry.kind = "SkinSpr";
    entry.name = name;
    entry.firstMetric = skin->m_texture.Width();
    entry.secondMetric = skin->m_texture.Height();
    entry.radius = 0.0;
    entry.height = 0.0;
    entry.loaded = skin->m_loaded && skin->m_texture.HImage() != NULL;
    collector->entries.push_back(entry);
    return true;
}

bool ResourceEntryLess(const ResourceEntry &left, const ResourceEntry &right)
{
    if (left.kind != right.kind)
        return left.kind < right.kind;
    return left.name < right.name;
}

bool CollectResources(SimulationContext *context, ResourceCollector &collector)
{
    if (context == NULL)
        return false;
    collector.context = context;
    collector.valid = true;
    g_skinTable.userFind(CollectSkin, &collector);
    if (collector.valid)
        g_skinSprTable.userFind(CollectSkinSpr, &collector);
    if (!collector.valid)
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              ResourceEntryLess);
    return true;
}

static void SkinSpriteCallback(byte *image, SRGB *)
{
    if (image != NULL)
        ((dword *)image)[-3] =
            TEXTURE_ALPHA | TEXTURE_SPRITE | TEXTURE_TXR_FORMAT;
}

}  // namespace

Skin::Skin()
{
    startInitialize();
}

Skin::~Skin()
{
    removeAniSets();
}

int Skin::receiveEvent(KR_Event &event)
{
    switch (event.label)
    {
    case KR_WAKE_UP:
        break;

    case pe_EV_SETANIM:
        if (m_animProgSP < m_animProgCnt)
        {
            int number = -1;
            event.data.open(EDO_READ).getInt(number);
            if (number < 0 || number >= m_aniCnt0)
            {
                event.data.close();
                return 0;
            }
            if (!m_animProg[m_animProgSP].setAnim(event, number))
            {
                event.data.close();
                return 0;
            }
            event.data.close();
            ++m_animProgSP;
        }
        else
        {
            echo("Error: animate prog len too big");
            return 0;
        }
        break;

    case sk_EV_LOAD:
        if (m_loaded)
            return 0;
        {
            char fileName[100] = {};
            event.data.open(EDO_READ).getStr(fileName, sizeof(fileName)).close();
            load(fileName);
        }
        return m_loaded ? 1 : 0;

    case sk_EV_QUERY_MODEL_PTR:
        if (!m_loaded)
            return 0;
        event.label = sk_EV_QUERY_MODEL_PTR_OK;
        {
            CViewObjectModel *model = &m_model;
            event.data.open(EDO_WRITE).put(&model, sizeof(void *)).close();
        }
        break;

    case sk_EV_PROG:
        {
            char command[40] = {};
            int number = 0;
            int number0 = 0;
            int programLength = 0;
            event.data.open(EDO_READ).getStr(command, sizeof(command));
            if (std::strcmp(command, "set0") == 0)
            {
                event.data.getInt(number).getStr(command, sizeof(command));
                set0(number, command);
            }
            else if (std::strcmp(command, "set") == 0)
            {
                event.data.getInt(number).getStr(command, sizeof(command));
                set(number, command);
            }
            else if (std::strcmp(command, "createAniSets") == 0)
            {
                event.data.getInt(number).getInt(number0);
                createAniSets(number, number0);
            }
            else if (std::strcmp(command, "createAniSetsAuto") == 0)
            {
                event.data.getInt(number).getInt(number0).getInt(programLength);
                createAniSets(number, number0, programLength);
            }
            else
            {
                event.data.close();
                echo("Skin::receiveEvent.sk_EV_PROG: Unknown command %s",
                     command);
                return 0;
            }
            event.data.close();
        }
        break;

    default:
        return 0;
    }
    return 1;
}

void Skin::addNotify()
{
    ct_Object::addNotify();
}

void Skin::removeNotify()
{
    removeAniSets();
    m_model.~CViewObjectModel();
    new (&m_model) CViewObjectModel();
    m_loaded = false;
    m_reducNum = 1;
    ct_Object::removeNotify();
}

void Skin::load(const char *fileName)
{
    if (fileName == NULL || fileName[0] == 0 || m_loaded)
        return;
    CTaggedFile file(FALSE);
    if (!file.Open(fileName, FALSE))
        return;
    m_model.Read(file, TRUE);
    const bool valid = file.IsOK() && file.Close(FALSE) &&
                       m_model.NumBaseSets() > 0 &&
                       m_model.Radius() >= 0.0 && m_model.Height() >= 0.0;
    if (!valid)
    {
        m_model.~CViewObjectModel();
        new (&m_model) CViewObjectModel();
        return;
    }
    m_model.Split(TRUE);
    m_loaded = true;
}

void Skin::createAniSets(int aniCnt, int aniCnt0)
{
    if (!m_loaded || aniCnt < 0 || aniCnt0 < 0)
        return;
    removeAniSets();
    const int reductionCount = m_model.NumBaseSets();
    if (reductionCount <= 0 || aniCnt > 4096 || aniCnt0 > 4096)
        return;
    m_reducNum = reductionCount;
    m_aniCnt = aniCnt;
    m_aniCnt0 = aniCnt0;
    if (m_aniCnt != 0)
    {
        const std::size_t count =
            static_cast<std::size_t>(m_aniCnt) * m_reducNum;
        m_array = new (std::nothrow) AniCell[count];
        if (m_array == NULL)
        {
            removeAniSets();
            return;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            m_array[i].m_use = false;
            m_array[i].m_fs = NULL;
        }
    }
    if (m_aniCnt0 != 0)
    {
        const std::size_t count =
            static_cast<std::size_t>(m_aniCnt0) * m_reducNum;
        m_array0 = new (std::nothrow) AniCell0[count];
        if (m_array0 == NULL)
        {
            removeAniSets();
            return;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            m_array0[i].m_use = false;
            m_array0[i].m_fs = NULL;
        }
    }
}

void Skin::createAniSets(int aniCnt, int aniCnt0, int programLength)
{
    if (!m_loaded || aniCnt < 0 || aniCnt0 < 0 ||
        programLength < 0 || programLength > 4096)
        return;
    createAniSets(aniCnt, aniCnt0);
    if (m_aniCnt != aniCnt || m_aniCnt0 != aniCnt0 || m_reducNum <= 0)
        return;
    if (programLength != 0)
    {
        m_animProg = new (std::nothrow) AnimateInfo[programLength];
        if (m_animProg == NULL)
        {
            removeAniSets();
            return;
        }
        m_animProgCnt = programLength;
        for (int i = 0; i < programLength; ++i)
            m_animProg[i].startInitialize(this);
    }
}

void Skin::removeAniSets()
{
    delete [] m_array;
    delete [] m_array0;
    delete [] m_animProg;
    m_array = NULL;
    m_array0 = NULL;
    m_animProg = NULL;
    m_aniCnt = 0;
    m_aniCnt0 = 0;
    m_reducNum = m_loaded ? m_model.NumBaseSets() : 1;
    m_isAutoAnim = 0;
    m_animProgCnt = 0;
    m_animProgSP = 0;
}

void Skin::set(int setNumber, const char *name)
{
    if (setNumber < 0 || setNumber >= m_aniCnt || name == NULL ||
        m_array == NULL)
        return;
    const int base = setNumber * m_reducNum;
    for (int i = 0; i < m_reducNum; ++i)
    {
        AniCell &cell = m_array[base + i];
        cell.m_use = true;
        cell.m_fs = &(m_model.BaseSet(0).Base(i).KFSet().Mod(name));
    }
}

void Skin::set0(int setNumber, const char *name)
{
    if (setNumber < 0 || setNumber >= m_aniCnt0 || name == NULL ||
        m_array0 == NULL)
        return;
    const int base = setNumber * m_reducNum;
    for (int i = 0; i < m_reducNum; ++i)
    {
        AniCell0 &cell = m_array0[base + i];
        cell.m_use = true;
        cell.m_fs = &(m_model.BaseSet(0).Base(i).KFSet().Mod0(name));
    }
}

CViewBaseModifier *Skin::get(int setNumber, int reduction)
{
    if (setNumber < 0 || setNumber >= m_aniCnt || reduction < 0 ||
        reduction >= m_reducNum || m_array == NULL)
        return &CViewBaseModifier::Null();
    AniCell &cell = m_array[m_reducNum * setNumber + reduction];
    return cell.m_use && cell.m_fs != NULL
               ? cell.m_fs
               : &CViewBaseModifier::Null();
}

CViewBaseModifier0 *Skin::get0(int setNumber, int reduction)
{
    if (setNumber < 0 || setNumber >= m_aniCnt0 || reduction < 0 ||
        reduction >= m_reducNum || m_array0 == NULL)
        return &CViewBaseModifier0::Null();
    AniCell0 &cell = m_array0[m_reducNum * setNumber + reduction];
    return cell.m_use && cell.m_fs != NULL
               ? cell.m_fs
               : &CViewBaseModifier0::Null();
}

void *Skin::queryInterface(int iid)
{
    return iid == ISkinIID ? static_cast<ISkin *>(this) : NULL;
}

void Skin::runAutoProg(CViewObjectBaseSet *baseSet,
                       CViewObjectBase *base, int)
{
    for (int i = 0; i < m_animProgSP; ++i)
        m_animProg[i].animateProg(baseSet, base);
}

int Skin::isAutoAnim()
{
    return m_isAutoAnim;
}

void AnimateAutoCallBack(CViewObjectBaseSet *baseSet,
                         CViewObjectBase *base, CViewObjectRef *reference)
{
    if (reference == NULL || reference->GetUserAttrib() == NULL)
        return;
    Skin *skin = static_cast<Skin *>(reference->GetUserAttrib());
    skin->runAutoProg(baseSet, base, base == NULL ? 0 : base->ReductionNum());
}

void Skin::skinSetAnimAuto(CViewObjectRef *reference)
{
    if (reference == NULL)
        return;
    reference->SetUserAttrib(this);
    reference->SetAnimationCallback(AnimateAutoCallBack);
}

int AnimateInfo::setAnim(KR_Event &event, int animNum)
{
    if (event.label != pe_EV_SETANIM ||
        m_acellCnt >= ANIMATE_CELL_CNT)
        return 0;
    AnimateCell &cell = m_acell[m_acellCnt++];
    cell.m_animNum = animNum;
    event.data.getInt(cell.m_type);
    switch (cell.m_type)
    {
    case anim_UPDATE:
    case anim_LOADIDENTITY:
        return 1;

    case anim_MOVE:
        event.data.descend(VECTOR3D_F, 0)
            .getDouble(cell.m_axis.x)
            .getDouble(cell.m_axis.y)
            .getDouble(cell.m_axis.z)
            .ascend()
            .descend(VECTOR3D_F, 0)
            .getDouble(cell.m_dir.x)
            .getDouble(cell.m_dir.y)
            .getDouble(cell.m_dir.z)
            .ascend()
            .getDouble(cell.A)
            .getDouble(cell.w)
            .getDouble(cell.F);
        return 1;

    case anim_ROTATEOX:
    case anim_ROTATEOY:
    case anim_ROTATEOZ:
        cell.A = 0;
        event.data.descend(VECTOR3D_F, 0)
            .getDouble(cell.m_axis.x)
            .getDouble(cell.m_axis.y)
            .getDouble(cell.m_axis.z)
            .ascend()
            .getDouble(cell.w)
            .getDouble(cell.F);
        return 1;

    case anim_ROTATEOXC:
    case anim_ROTATEOYC:
    case anim_ROTATEOZC:
        event.data.descend(VECTOR3D_F, 0)
            .getDouble(cell.m_axis.x)
            .getDouble(cell.m_axis.y)
            .getDouble(cell.m_axis.z)
            .ascend()
            .getDouble(cell.A)
            .getDouble(cell.w)
            .getDouble(cell.F);
        return 1;

    default:
        --m_acellCnt;
        echo("AnimateInfo::setAnim: unsupported retail animation command %d",
             cell.m_type);
        return 0;
    }
}

void AnimateInfo::animateProg(CViewObjectBaseSet *, CViewObjectBase *base)
{
    if (base == NULL || m_askin == NULL)
        return;
    const int reduction = base->ReductionNum();
    const double time = Session::m_viewTime;
    for (int i = 0; i < m_acellCnt; ++i)
    {
        AnimateCell &cell = m_acell[i];
        CViewBaseModifier0 *modifier = m_askin->get0(cell.m_animNum, reduction);
        switch (cell.m_type)
        {
        case anim_UPDATE:
            modifier->Update();
            break;
        case anim_MOVE:
            modifier->Translate(cell.m_axis +
                                cell.m_dir * cell.A *
                                    std::sin(cell.w * time + cell.F));
            break;
        case anim_ROTATEOX:
            modifier->RotateOx(cell.w * time + cell.F, cell.m_axis);
            break;
        case anim_ROTATEOY:
            modifier->RotateOy(cell.w * time + cell.F, cell.m_axis);
            break;
        case anim_ROTATEOZ:
            modifier->RotateOz(cell.w * time + cell.F, cell.m_axis);
            break;
        case anim_ROTATEOXC:
            modifier->RotateOx(cell.A * std::sin(cell.w * time + cell.F),
                               cell.m_axis);
            break;
        case anim_ROTATEOYC:
            modifier->RotateOy(cell.A * std::sin(cell.w * time + cell.F),
                               cell.m_axis);
            break;
        case anim_ROTATEOZC:
            modifier->RotateOz(cell.A * std::sin(cell.w * time + cell.F),
                               cell.m_axis);
            break;
        case anim_LOADIDENTITY:
            modifier->LoadIdentity();
            break;
        }
    }
}

int RecoveredSkinSprite::receiveEvent(KR_Event &event)
{
    switch (event.label)
    {
    case KR_WAKE_UP:
        return 1;

    case sk_EV_LOAD:
        if (m_loaded)
            return 0;
        {
            char fileName[100] = {};
            event.data.open(EDO_READ).getStr(fileName, sizeof(fileName)).close();
            if (fileName[0] == 0)
                return 0;
            m_texture.Read(fileName, TRUE, SkinSpriteCallback);
            m_loaded = m_texture.HImage() != NULL &&
                       m_texture.Width() > 0 && m_texture.Height() > 0;
            if (!m_loaded)
                m_texture.Release();
        }
        return m_loaded ? 1 : 0;

    case sk_EV_QUERY_MODEL_PTR:
        if (!m_loaded)
            return 0;
        event.label = sk_EV_QUERY_MODEL_PTR_OK;
        {
            CViewTexture *texture = &m_texture;
            event.data.open(EDO_WRITE).put(&texture, sizeof(void *)).close();
        }
        return 1;

    default:
        return 0;
    }
}

void RecoveredSkinSprite::addNotify()
{
    ct_Object::addNotify();
}

void RecoveredSkinSprite::removeNotify()
{
    m_texture.Release();
    m_loaded = false;
    ct_Object::removeNotify();
}

void SkinResourceState_Link()
{
}

bool SkinResourceState_ResolveLoadedModel(SimulationContext *context,
                                          const char *objectName,
                                          KR_ObjectID *objectID,
                                          CViewObjectModel **model)
{
    if (objectID != NULL)
        *objectID = KR_ObjectID::NUL();
    if (model != NULL)
        *model = NULL;
    if (context == NULL || objectName == NULL || objectName[0] == 0 ||
        objectID == NULL || model == NULL)
        return false;
    KR_ObjectID resolvedID = context->searchObject(objectName);
    Skin *skin = resolvedID.isNUL() ? NULL : g_skinTable.find(resolvedID);
    if (skin == NULL || !skin->m_loaded)
        return false;
    *objectID = resolvedID;
    *model = &skin->m_model;
    return true;
}

int SkinResourceState_ModelCount(SimulationContext *context)
{
    ResourceCollector collector = {};
    if (!CollectResources(context, collector))
        return 0;
    int count = 0;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (collector.entries[i].kind == "Skin")
            ++count;
    return count;
}

int SkinResourceState_SpriteCount(SimulationContext *context)
{
    ResourceCollector collector = {};
    if (!CollectResources(context, collector))
        return 0;
    int count = 0;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (collector.entries[i].kind == "SkinSpr")
            ++count;
    return count;
}

unsigned long long SkinResourceState_Fingerprint(SimulationContext *context)
{
    ResourceCollector collector = {};
    if (!CollectResources(context, collector) || collector.entries.empty())
        return 0;
    unsigned long long hash = kHashOffset;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        const ResourceEntry &entry = collector.entries[i];
        HashString(hash, entry.kind.c_str());
        HashString(hash, entry.name.c_str());
        HashBytes(hash, &entry.firstMetric, sizeof(entry.firstMetric));
        HashBytes(hash, &entry.secondMetric, sizeof(entry.secondMetric));
        HashBytes(hash, &entry.radius, sizeof(entry.radius));
        HashBytes(hash, &entry.height, sizeof(entry.height));
        const int loaded = entry.loaded ? 1 : 0;
        HashBytes(hash, &loaded, sizeof(loaded));
    }
    return hash;
}

bool SkinResourceState_AllLoaded(SimulationContext *context)
{
    ResourceCollector collector = {};
    if (!CollectResources(context, collector) || collector.entries.empty())
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (!collector.entries[i].loaded)
            return false;
    return true;
}
