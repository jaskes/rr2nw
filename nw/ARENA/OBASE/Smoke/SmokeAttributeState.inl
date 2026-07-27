#ifndef RR2NW_SMOKE_ATTRIBUTE_STATE_INL
#define RR2NW_SMOKE_ATTRIBUTE_STATE_INL

AttributeTableSmoke __attrSmokeTable;

namespace {

const unsigned long long kSmokeHashOffset = 14695981039346656037ull;
const unsigned long long kSmokeHashPrime = 1099511628211ull;

void SmokeHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kSmokeHashPrime;
    }
}

void SmokeHashString(unsigned long long &hash, const char *value)
{
    SmokeHashBytes(hash, value, static_cast<int>(strlen(value)) + 1);
}

void SmokeHashAttribute(unsigned long long &hash, AttributeSmoke &attr)
{
#define RR2NW_SMOKE_HASH_FIELD(field) \
    SmokeHashBytes(hash, &attr.field, sizeof(attr.field))
    RR2NW_SMOKE_HASH_FIELD(m_radius);
    RR2NW_SMOKE_HASH_FIELD(m_onLand);
    RR2NW_SMOKE_HASH_FIELD(m_maxBlob);
    RR2NW_SMOKE_HASH_FIELD(m_minDirInc);
    RR2NW_SMOKE_HASH_FIELD(m_maxDirInc);
    RR2NW_SMOKE_HASH_FIELD(m_rndOfs);
    RR2NW_SMOKE_HASH_FIELD(m_ofsHAngle);
    RR2NW_SMOKE_HASH_FIELD(m_ofsVAngle);
    RR2NW_SMOKE_HASH_FIELD(m_ofsSpeed);
    RR2NW_SMOKE_HASH_FIELD(m_dirHAngle);
    RR2NW_SMOKE_HASH_FIELD(m_dirVAngle);
    RR2NW_SMOKE_HASH_FIELD(m_dirSpeed);
    RR2NW_SMOKE_HASH_FIELD(m_maxTimeLife);
    RR2NW_SMOKE_HASH_FIELD(m_minRA);
    RR2NW_SMOKE_HASH_FIELD(m_maxRA);
    RR2NW_SMOKE_HASH_FIELD(m_minRB);
    RR2NW_SMOKE_HASH_FIELD(m_maxRB);
    RR2NW_SMOKE_HASH_FIELD(m_minRC);
    RR2NW_SMOKE_HASH_FIELD(m_maxRC);
    RR2NW_SMOKE_HASH_FIELD(m_minTA);
    RR2NW_SMOKE_HASH_FIELD(m_maxTA);
    RR2NW_SMOKE_HASH_FIELD(m_minTB);
    RR2NW_SMOKE_HASH_FIELD(m_maxTB);
    RR2NW_SMOKE_HASH_FIELD(m_minTC);
    RR2NW_SMOKE_HASH_FIELD(m_maxTC);
    SmokeHashString(hash, attr.m_imageName);
    RR2NW_SMOKE_HASH_FIELD(m_RGB);
    RR2NW_SMOKE_HASH_FIELD(m_timeIncrement);
    RR2NW_SMOKE_HASH_FIELD(RGB0);
    RR2NW_SMOKE_HASH_FIELD(RGB1);
    RR2NW_SMOKE_HASH_FIELD(RGB2);
    RR2NW_SMOKE_HASH_FIELD(RGB3);
    RR2NW_SMOKE_HASH_FIELD(m_isColorGradient);
    RR2NW_SMOKE_HASH_FIELD(m_useRndDir);
    RR2NW_SMOKE_HASH_FIELD(m_rndDir);
    RR2NW_SMOKE_HASH_FIELD(m_r0);
    RR2NW_SMOKE_HASH_FIELD(m_r1);
    RR2NW_SMOKE_HASH_FIELD(m_r2);
    RR2NW_SMOKE_HASH_FIELD(m_r3);
#undef RR2NW_SMOKE_HASH_FIELD
}

bool SmokeCachesAreUnresolved(AttributeSmoke &attr)
{
    if (attr.m_cacheImage != NULL || attr.m_cacheColor != 0)
        return false;
    for (int i = 0; i < AttributeSmoke::MAX_COLOR; ++i)
        if (attr.colors[i] != 0)
            return false;
    return true;
}

}  // namespace

void SmokeAttributeState_Link()
{
}

void AttributeTableSmoke::allocObjects(int objectQnty)
{
    m_table = new AttributeSmoke[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableSmoke::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableSmoke::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableSmoke::getObjectPTR");
    return &(m_table[index]);
}

inline double SmokeGetR(int x) { return x >> 16; }
inline double SmokeGetG(int x) { return (x >> 8) & 255; }
inline double SmokeGetB(int x) { return x & 255; }

unsigned long SmokeCalcRGB(double x, double r[4], double g[4], double b[4])
{
    const double x2 = x * x;
    const double x3 = x2 * x;
    int R = static_cast<int>(r[0] + r[1] * x + r[2] * x2 + r[3] * x3);
    int G = static_cast<int>(g[0] + g[1] * x + g[2] * x2 + g[3] * x3);
    int B = static_cast<int>(b[0] + b[1] * x + b[2] * x2 + b[3] * x3);
    if (R < 0) R = 0; else if (R > 255) R = 255;
    if (G < 0) G = 0; else if (G > 255) G = 255;
    if (B < 0) B = 0; else if (B > 255) B = 255;
    return GRTransparentColor(R, G, B);
}

void AttributeSmoke::update(double)
{
    m_cacheColor = GRTransparentColor(m_RGB >> 16, (m_RGB >> 8) & 255,
                                      m_RGB & 255);
    m_cacheImage = g_loadSmoke(m_imageName, NULL);
    if (m_isColorGradient)
    {
        double r[4], g[4], b[4];
        calcCoef(MAX_COLOR / 3.0, SmokeGetR(RGB0), SmokeGetR(RGB1),
                 SmokeGetR(RGB2), SmokeGetR(RGB3),
                 r[0], r[1], r[2], r[3]);
        calcCoef(MAX_COLOR / 3.0, SmokeGetG(RGB0), SmokeGetG(RGB1),
                 SmokeGetG(RGB2), SmokeGetG(RGB3),
                 g[0], g[1], g[2], g[3]);
        calcCoef(MAX_COLOR / 3.0, SmokeGetB(RGB0), SmokeGetB(RGB1),
                 SmokeGetB(RGB2), SmokeGetB(RGB3),
                 b[0], b[1], b[2], b[3]);
        for (int i = 0; i < MAX_COLOR; ++i)
            colors[i] = SmokeCalcRGB(i * 32.0 / 31.0, r, g, b);
    }
}

unsigned long long SmokeAttributeState_RetailFingerprint(
    SimulationContext *context)
{
    static const char *names[] = {
        "Smoke.Attr.Small", "Smoke.Attr.Led", "Smoke.Attr.LedSm",
        "Smoke.Attr.LedBig", "Smoke.Attr.Def", "Smoke.Attr.Fire",
        "Smoke.Attr.FireGrad", "Smoke.Attr.Volcano",
        "Smoke.Attr.FireArea", "Smoke.Attr.Tower", "Smoke.Attr.Huge",
        "Smoke.Attr.White", "Smoke.Attr.FireSm", "Smoke.Attr.FireMd",
        "Smoke.Attr.Trace", "Smoke.Attr.FireMdBlue", "Smoke.Attr.Corpse",
        "Smoke.Attr.Fire.Corpse"
    };
    if (context == NULL)
        return 0;

    unsigned long long hash = kSmokeHashOffset;
    for (int i = 0; i < static_cast<int>(sizeof(names) / sizeof(names[0]));
         ++i)
    {
        KR_ObjectID object = context->searchObject(names[i]);
        if (object.isNUL())
            return 0;
        AttributeSmoke *attr = static_cast<AttributeSmoke *>(
            __attrSmokeTable.searchAttribute(object));
        if (attr == NULL || !SmokeCachesAreUnresolved(*attr))
            return 0;
        SmokeHashString(hash, names[i]);
        SmokeHashAttribute(hash, *attr);
    }
    return hash;
}

bool SmokeAttributeState_IsRetailRoster(SimulationContext *context)
{
    const unsigned long long expected = 13981601751930040122ull;
    return SmokeAttributeState_RetailFingerprint(context) == expected;
}

#endif
